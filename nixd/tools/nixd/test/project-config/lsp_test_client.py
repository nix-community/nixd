#!/usr/bin/env python3
"""Bounded real-process LSP client for nixd integration tests."""

import json
import os
import select
import shutil
import signal
import subprocess
import threading
import time


TIMEOUT_SECONDS = 30
SHUTDOWN_SECONDS = 3


class Client:
    def __init__(self, *, args=(), cwd=None, env=None):
        executable = shutil.which("nixd", path=(env or os.environ).get("PATH"))
        if executable is None:
            raise RuntimeError("nixd is not available on PATH")
        self.received = []
        self._stdout = bytearray()
        self._stderr = bytearray()
        self._stderr_lock = threading.Lock()
        self._stderr_done = threading.Event()
        self._stderr_thread = None
        self._stderr_thread_started = False
        self._close_lock = threading.Lock()
        self._closed = False
        self.proc = subprocess.Popen(
            [executable, *args],
            cwd=cwd,
            env=env,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            bufsize=0,
            start_new_session=(os.name == "posix"),
        )
        try:
            os.set_blocking(self.proc.stdin.fileno(), False)
            self._stderr_thread = threading.Thread(
                target=self._drain_stderr, name="nixd-test-stderr", daemon=True
            )
            self._stderr_thread.start()
            self._stderr_thread_started = True
        except BaseException:
            self._cleanup_process(send_exit=False)
            self._closed = True
            raise

    def _drain_stderr(self):
        try:
            while True:
                chunk = os.read(self.proc.stderr.fileno(), 8192)
                if not chunk:
                    return
                with self._stderr_lock:
                    self._stderr.extend(chunk)
        finally:
            self._stderr_done.set()

    @property
    def stderr(self):
        with self._stderr_lock:
            return self._stderr.decode(errors="replace")

    def send(self, message, timeout=TIMEOUT_SECONDS):
        if self.proc.stdin is None or self.proc.stdin.closed:
            raise BrokenPipeError("nixd protocol input is closed")
        payload = json.dumps(message, separators=(",", ":")).encode()
        frame = memoryview(
            f"Content-Length: {len(payload)}\r\n\r\n".encode() + payload
        )
        descriptor = self.proc.stdin.fileno()
        deadline = time.monotonic() + timeout
        while frame:
            try:
                written = os.write(descriptor, frame)
            except InterruptedError:
                continue
            except BlockingIOError:
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    raise TimeoutError("timed out writing an LSP message")
                _, writable, _ = select.select([], [descriptor], [], remaining)
                if not writable:
                    raise TimeoutError("timed out writing an LSP message")
                continue
            if written == 0:
                raise BrokenPipeError("nixd closed its protocol input")
            frame = frame[written:]

    def _fill_stdout(self, deadline):
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise TimeoutError(
                f"timed out while reading an LSP message; stderr={self.stderr!r}"
            )
        readable, _, _ = select.select([self.proc.stdout], [], [], remaining)
        if not readable:
            raise TimeoutError(
                f"timed out while reading an LSP message; stderr={self.stderr!r}"
            )
        chunk = os.read(self.proc.stdout.fileno(), 8192)
        if not chunk:
            raise RuntimeError(
                f"nixd closed its protocol stream; stderr={self.stderr!r}"
            )
        self._stdout.extend(chunk)

    def receive(self, timeout=TIMEOUT_SECONDS):
        deadline = time.monotonic() + timeout
        separator = b"\r\n\r\n"
        while separator not in self._stdout:
            self._fill_stdout(deadline)
        header_end = self._stdout.index(separator)
        header_block = bytes(self._stdout[:header_end])
        headers = {}
        for line in header_block.split(b"\r\n"):
            key, value = line.decode().split(":", 1)
            headers[key.lower()] = value.strip()
        length = int(headers["content-length"])
        frame_end = header_end + len(separator) + length
        while len(self._stdout) < frame_end:
            self._fill_stdout(deadline)
        payload = bytes(self._stdout[header_end + len(separator) : frame_end])
        del self._stdout[:frame_end]
        message = json.loads(payload)
        self.received.append(message)
        return message

    def _has_complete_message(self):
        separator = b"\r\n\r\n"
        if separator not in self._stdout:
            return False
        header_end = self._stdout.index(separator)
        headers = {}
        for line in self._stdout[:header_end].split(b"\r\n"):
            key, value = line.decode().split(":", 1)
            headers[key.lower()] = value.strip()
        return len(self._stdout) >= (
            header_end + len(separator) + int(headers["content-length"])
        )

    def wait_for(self, predicate, timeout=TIMEOUT_SECONDS):
        deadline = time.monotonic() + timeout
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise TimeoutError("timed out waiting for an LSP predicate")
            message = self.receive(remaining)
            if predicate(message):
                return message
            if "id" in message and "method" in message:
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    raise TimeoutError("timed out replying to an LSP request")
                self.reply(message, result=None, timeout=remaining)

    def assert_no_message(self, predicate, timeout=0.25):
        deadline = time.monotonic() + timeout
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                return
            if not self._has_complete_message():
                readable, _, _ = select.select(
                    [self.proc.stdout], [], [], remaining
                )
                if not readable:
                    return
            message = self.receive(remaining)
            if predicate(message):
                raise AssertionError(f"unexpected LSP message: {message}")
            if "id" in message and "method" in message:
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    return
                self.reply(message, result=None, timeout=remaining)

    def read_stderr_until(self, predicate, timeout=TIMEOUT_SECONDS):
        deadline = time.monotonic() + timeout
        while True:
            text = self.stderr
            if predicate(text):
                return text
            if self._stderr_done.is_set():
                raise RuntimeError(f"nixd closed stderr before predicate: {text!r}")
            if time.monotonic() >= deadline:
                raise TimeoutError(f"stderr predicate not met: {text!r}")
            self._stderr_done.wait(0.01)

    def notify(self, method, params=None, timeout=TIMEOUT_SECONDS):
        self.send({
            "jsonrpc": "2.0",
            "method": method,
            "params": {} if params is None else params,
        }, timeout=timeout)

    def reply(
        self, request, *, result=None, error=None, timeout=TIMEOUT_SECONDS
    ):
        response = {"jsonrpc": "2.0", "id": request["id"]}
        if error is None:
            response["result"] = result
        else:
            response["error"] = error
        self.send(response, timeout=timeout)

    def initialize(
        self,
        *,
        root_uri=None,
        root_path=None,
        workspace_folders=None,
        capabilities=None,
        timeout=TIMEOUT_SECONDS,
    ):
        deadline = time.monotonic() + timeout
        params = {"processId": os.getpid(), "capabilities": capabilities or {}}
        if root_uri is not None:
            params["rootUri"] = root_uri
        if root_path is not None:
            params["rootPath"] = root_path
        if workspace_folders is not None:
            params["workspaceFolders"] = workspace_folders
        self.send({
            "jsonrpc": "2.0",
            "id": 0,
            "method": "initialize",
            "params": params,
        }, timeout=timeout)
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise TimeoutError("timed out waiting for initialize response")
        return self.wait_for(
            lambda message: message.get("id") == 0
            and "method" not in message,
            timeout=remaining,
        )

    def initialized(self):
        self.notify("initialized")

    def open_document(self, uri, text, version=1):
        self.notify("textDocument/didOpen", {
            "textDocument": {
                "uri": uri,
                "languageId": "nix",
                "version": version,
                "text": text,
            }
        })

    def request(self, request_id, method, params, timeout=TIMEOUT_SECONDS):
        deadline = time.monotonic() + timeout
        self.send({
            "jsonrpc": "2.0",
            "id": request_id,
            "method": method,
            "params": params,
        }, timeout=timeout)
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise TimeoutError("timed out waiting for an LSP response")
        return self.wait_for(
            lambda message: message.get("id") == request_id
            and "method" not in message,
            timeout=remaining,
        )

    def formatting(self, request_id, uri, timeout=TIMEOUT_SECONDS):
        return self.request(request_id, "textDocument/formatting", {
            "textDocument": {"uri": uri},
            "options": {"tabSize": 2, "insertSpaces": True},
        }, timeout=timeout)

    def workspace_request(self, timeout=TIMEOUT_SECONDS):
        return self.wait_for(
            lambda message: message.get("method") == "workspace/configuration",
            timeout=timeout,
        )

    def _unreaped_leader_state(self):
        try:
            result = os.waitid(
                os.P_PID,
                self.proc.pid,
                os.WEXITED | os.WNOHANG | os.WNOWAIT,
            )
        except ChildProcessError:
            return "reaped"
        return "exited" if result is not None else "running"

    def _wait_for_unreaped_leader(self, deadline):
        state = self._unreaped_leader_state()
        while state == "running":
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                return state
            self._stderr_done.wait(min(0.01, remaining))
            state = self._unreaped_leader_state()
        return state

    def _signal_process_group(self, signum):
        try:
            os.killpg(self.proc.pid, signum)
        except (ProcessLookupError, PermissionError):
            pass

    def _cleanup_posix(self, deadline):
        started = time.monotonic()
        duration = max(0.0, deadline - started)
        graceful_deadline = started + duration / 2
        terminate_deadline = started + duration * 3 / 4
        state = self._wait_for_unreaped_leader(graceful_deadline)
        if state == "running":
            self._signal_process_group(signal.SIGTERM)
            state = self._wait_for_unreaped_leader(terminate_deadline)
        if state != "reaped":
            # The leader remains an unreaped child here, so its process-group
            # identity cannot be recycled before this final descendant cleanup.
            # Do not probe killpg(PGID, 0) afterward: the zombie leader keeps
            # that probe true even when no live descendants remain. The harness
            # instead verifies an exact same-group descendant disappears.
            self._signal_process_group(signal.SIGKILL)
            if state == "running":
                self._wait_for_unreaped_leader(deadline)
        self.proc.wait(timeout=max(0.01, deadline - time.monotonic()))

    def _cleanup_non_posix(self, deadline):
        try:
            self.proc.wait(timeout=max(0.01, deadline - time.monotonic()))
            return
        except subprocess.TimeoutExpired:
            self.proc.terminate()
        try:
            self.proc.wait(timeout=max(0.01, deadline - time.monotonic()))
            return
        except subprocess.TimeoutExpired:
            self.proc.kill()
        self.proc.wait(timeout=max(0.01, deadline - time.monotonic()))

    def _cleanup_process(self, *, send_exit):
        deadline = time.monotonic() + SHUTDOWN_SECONDS
        if send_exit:
            try:
                self.notify(
                    "exit",
                    timeout=min(
                        SHUTDOWN_SECONDS / 4,
                        max(0.0, deadline - time.monotonic()),
                    ),
                )
            except (BrokenPipeError, OSError, TimeoutError):
                pass
        if self.proc.stdin is not None and not self.proc.stdin.closed:
            self.proc.stdin.close()
        if os.name == "posix":
            self._cleanup_posix(deadline)
        else:
            self._cleanup_non_posix(deadline)

        if self._stderr_thread_started:
            self._stderr_done.wait(max(0.0, deadline - time.monotonic()))
            self._stderr_thread.join(
                timeout=max(0.0, deadline - time.monotonic())
            )
        for stream in (self.proc.stdout, self.proc.stderr):
            if stream is not None and not stream.closed:
                stream.close()

    def close(self):
        with self._close_lock:
            if not self._closed:
                try:
                    self._cleanup_process(send_exit=True)
                finally:
                    self._closed = True
            return self.stderr

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        self.close()
