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
        self.received = []
        self._stdout = bytearray()
        self._stderr = bytearray()
        self._stderr_lock = threading.Lock()
        self._stderr_done = threading.Event()
        self._stderr_thread = threading.Thread(
            target=self._drain_stderr, name="nixd-test-stderr", daemon=True
        )
        self._stderr_thread.start()

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

    def send(self, message):
        if self.proc.stdin is None or self.proc.stdin.closed:
            raise BrokenPipeError("nixd protocol input is closed")
        payload = json.dumps(message, separators=(",", ":")).encode()
        frame = memoryview(
            f"Content-Length: {len(payload)}\r\n\r\n".encode() + payload
        )
        descriptor = self.proc.stdin.fileno()
        while frame:
            written = os.write(descriptor, frame)
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
                self.reply(message, result=None)

    def assert_no_message(self, predicate, timeout=0.25):
        deadline = time.monotonic() + timeout
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                return
            readable, _, _ = select.select([self.proc.stdout], [], [], remaining)
            if not readable:
                return
            message = self.receive(remaining)
            if predicate(message):
                raise AssertionError(f"unexpected LSP message: {message}")
            if "id" in message and "method" in message:
                self.reply(message, result=None)

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

    def notify(self, method, params=None):
        self.send({
            "jsonrpc": "2.0",
            "method": method,
            "params": {} if params is None else params,
        })

    def reply(self, request, *, result=None, error=None):
        response = {"jsonrpc": "2.0", "id": request["id"]}
        if error is None:
            response["result"] = result
        else:
            response["error"] = error
        self.send(response)

    def initialize(
        self,
        *,
        root_uri=None,
        root_path=None,
        workspace_folders=None,
        capabilities=None,
    ):
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
        })
        return self.wait_for(lambda message: message.get("id") == 0)

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
        self.send({
            "jsonrpc": "2.0",
            "id": request_id,
            "method": method,
            "params": params,
        })
        return self.wait_for(
            lambda message: message.get("id") == request_id, timeout=timeout
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

    def _group_exists(self):
        if os.name != "posix":
            return False
        try:
            os.killpg(self.proc.pid, 0)
            return True
        except ProcessLookupError:
            return False

    def _terminate_process_group(self):
        if not self._group_exists():
            return
        os.killpg(self.proc.pid, signal.SIGTERM)
        deadline = time.monotonic() + SHUTDOWN_SECONDS
        while self._group_exists() and time.monotonic() < deadline:
            time.sleep(0.01)
        if self._group_exists():
            os.killpg(self.proc.pid, signal.SIGKILL)

    def close(self):
        if self.proc.poll() is None:
            try:
                self.notify("exit")
            except (BrokenPipeError, OSError):
                pass
        if self.proc.stdin is not None and not self.proc.stdin.closed:
            self.proc.stdin.close()
        try:
            self.proc.wait(timeout=SHUTDOWN_SECONDS)
        except subprocess.TimeoutExpired:
            if os.name == "posix":
                self._terminate_process_group()
            else:
                self.proc.terminate()
            try:
                self.proc.wait(timeout=SHUTDOWN_SECONDS)
            except subprocess.TimeoutExpired:
                if os.name == "posix":
                    os.killpg(self.proc.pid, signal.SIGKILL)
                else:
                    self.proc.kill()
                self.proc.wait(timeout=SHUTDOWN_SECONDS)
        self._terminate_process_group()
        self._stderr_done.wait(SHUTDOWN_SECONDS)
        self._stderr_thread.join(timeout=SHUTDOWN_SECONDS)
        return self.stderr

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        self.close()
