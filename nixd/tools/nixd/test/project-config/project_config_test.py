#!/usr/bin/env python3
"""Real-process project-configuration scenarios selected by command line."""

import errno
import json
import os
from pathlib import Path
import stat
import sys
import tempfile
import time

sys.dont_write_bytecode = True

from lsp_test_client import Client


CAPABILITIES = {"workspace": {"configuration": True}}
NO_PROVIDERS = ("--nixpkgs-expr=", "--nixos-options-expr=")


def write_json(path, value):
    path.write_text(json.dumps(value), encoding="utf-8")


def write_executable(path, body):
    path.write_text(
        "#!/usr/bin/env python3\n"
        "import os\n"
        "import sys\n"
        "sys.stdin.read()\n"
        f"{body}\n",
        encoding="utf-8",
    )
    path.chmod(path.stat().st_mode | stat.S_IXUSR)


def test_environment(bin_dir):
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join((str(bin_dir), env.get("PATH", "")))
    return env


def write_formatter(bin_dir, name, marker=None):
    if marker is None:
        body = "sys.stdout.write(os.getcwd())"
    else:
        body = f"sys.stdout.write({marker!r})"
    write_executable(bin_dir / name, body)


def write_project(root, command):
    write_json(root / ".nixd.json", {"formatting": {"command": [command]}})


def formatted_text(client, request_id, uri):
    response = client.formatting(request_id, uri)
    assert "error" not in response, response
    edits = response["result"]
    return None if not edits else edits[0]["newText"]


def show_message(message):
    return message.get("method") == "window/showMessage"


def warning_messages(client):
    return [
        message["params"]["message"]
        for message in client.received
        if show_message(message)
    ]


def expect_startup_warning(client, expected):
    assert warning_messages(client) == [], client.received
    client.initialized()
    warning = client.wait_for(show_message)
    assert warning["params"] == {"type": 2, "message": expected}, warning
    assert warning_messages(client) == [expected], client.received


def editor_patch(command):
    return [{"formatting": {"command": [command]}}]


def request_editor_configuration(client):
    client.notify("workspace/didChangeConfiguration", {"settings": {}})
    return client.workspace_request()


def wait_for_formatted_text(client, uri, expected, request_id):
    deadline = time.monotonic() + 10
    last = None
    while time.monotonic() < deadline:
        last = formatted_text(client, request_id, uri)
        request_id += 1
        if last == expected:
            return request_id
    raise AssertionError(f"formatter never produced {expected!r}; last={last!r}")


def completion(client, request_id, uri):
    return client.request(request_id, "textDocument/completion", {
        "textDocument": {"uri": uri},
        "position": {"line": 0, "character": 13},
        "context": {"triggerKind": 1},
    })["result"]


def hover(client, request_id, uri):
    return client.request(request_id, "textDocument/hover", {
        "textDocument": {"uri": uri},
        "position": {"line": 0, "character": 18},
    })["result"]


def definition(client, request_id, uri):
    return client.request(request_id, "textDocument/definition", {
        "textDocument": {"uri": uri},
        "position": {"line": 0, "character": 18},
    })["result"]


def inlay_hints(client, request_id, uri):
    return client.request(request_id, "textDocument/inlayHint", {
        "textDocument": {"uri": uri},
        "range": {
            "start": {"line": 0, "character": 0},
            "end": {"line": 0, "character": 29},
        },
    })["result"]


def completion_labels(result):
    return {item.get("label") for item in result["items"]}


def wait_for_endpoint(client, operation, predicate, request_id, description):
    deadline = time.monotonic() + 15
    last = None
    while time.monotonic() < deadline:
        last = operation(client, request_id)
        request_id += 1
        if predicate(last):
            return last, request_id
    raise AssertionError(f"{description} never recovered; last={last!r}")


def exercise_empty_formatter():
    config = {
        "formatting": {"command": []},
        "nixpkgs": {"expr": ""},
        "options": {},
    }
    with tempfile.TemporaryDirectory() as temporary:
        root = Path(temporary)
        uri = (root / "empty-formatter.nix").as_uri()
        with Client(args=("--config=" + json.dumps(config),), cwd=root) as client:
            client.initialize(root_path=str(root))
            client.open_document(uri, "{ value = 1; }\n")
            response = client.formatting(1, uri)
            assert response == {"jsonrpc": "2.0", "id": 1, "result": []}, response


def exercise_project_opt_in(base):
    root = base / "opt-in"
    bin_dir = root / "bin"
    bin_dir.mkdir(parents=True)
    write_formatter(bin_dir, "nixfmt", "default-formatter")
    write_formatter(bin_dir, "project-formatter", "project-formatter")
    write_project(root, "project-formatter")
    uri = (root / "opt-in.nix").as_uri()
    with Client(args=NO_PROVIDERS, cwd=root, env=test_environment(bin_dir)) as client:
        client.initialize(root_uri=root.as_uri())
        client.open_document(uri, "{ value = 1; }\n")
        assert formatted_text(client, 10, uri) == "default-formatter"


def exercise_explicit_config(base):
    root = base / "explicit-config"
    bin_dir = root / "bin"
    bin_dir.mkdir(parents=True)
    write_formatter(bin_dir, "cli-formatter", "cli-formatter")
    (root / ".nixd.json").symlink_to(".nixd.json")
    config = {
        "formatting": {"command": ["cli-formatter"]},
        "nixpkgs": {
            "expr": '{ cliMarker.meta.description = "CLI configuration"; }'
        },
        "options": {},
    }
    args = (
        "--enable-project-config",
        '--nixpkgs-expr={ legacyMarker.meta.description = "legacy"; }',
        "--config=" + json.dumps(config),
    )
    format_uri = (root / "explicit-format.nix").as_uri()
    hover_uri = (root / "explicit-hover.nix").as_uri()
    client = Client(args=args, cwd=root, env=test_environment(bin_dir))
    try:
        client.initialize(root_uri=root.as_uri())
        client.initialized()
        client.assert_no_message(show_message)
        client.open_document(format_uri, "{ value = 1; }\n")
        client.open_document(hover_uri, "pkgs.cliMarker\n")
        assert formatted_text(client, 11, format_uri) == "cli-formatter"
        hover = client.request(12, "textDocument/hover", {
            "textDocument": {"uri": hover_uri},
            "position": {"line": 0, "character": 10},
        })
        assert "CLI configuration" in json.dumps(hover["result"]), hover
        assert "legacy" not in json.dumps(hover["result"]), hover
    finally:
        stderr = client.close()
    assert ".nixd.json" not in stderr, stderr


def exercise_root_precedence(base):
    bin_dir = base / "precedence-bin"
    bin_dir.mkdir()
    env = test_environment(bin_dir)
    roots = {}
    for name in ("root-uri", "root-path", "workspace", "launch"):
        root = base / name
        root.mkdir()
        command = f"{name}-formatter"
        write_formatter(bin_dir, command, name)
        write_project(root, command)
        roots[name] = root

    cases = (
        ({
            "root_uri": roots["root-uri"].as_uri(),
            "root_path": str(roots["root-path"]),
            "workspace_folders": [{
                "uri": roots["workspace"].as_uri(), "name": "workspace"
            }],
        }, "root-uri"),
        ({
            "root_path": str(roots["root-path"]),
            "workspace_folders": [{
                "uri": roots["workspace"].as_uri(), "name": "workspace"
            }],
        }, "root-path"),
        ({
            "root_path": "",
            "workspace_folders": [{
                "uri": roots["workspace"].as_uri(), "name": "workspace"
            }],
        }, "workspace"),
        ({"root_path": ""}, "launch"),
    )
    for index, (initialize, expected) in enumerate(cases, 20):
        root = roots[expected]
        uri = (root / f"{expected}.nix").as_uri()
        with Client(
            args=(*NO_PROVIDERS, "--enable-project-config"),
            cwd=roots["launch"],
            env=env,
        ) as client:
            client.initialize(**initialize)
            client.open_document(uri, "{ value = 1; }\n")
            assert formatted_text(client, index, uri) == expected


def exercise_empty_project_rebases(base):
    launch = base / "empty-launch"
    selected = base / "empty-selected"
    bin_dir = base / "empty-bin"
    launch.mkdir()
    selected.mkdir()
    bin_dir.mkdir()
    write_formatter(bin_dir, "nixfmt")
    write_json(selected / ".nixd.json", {})
    (selected / "nixpkgs.nix").write_text(
        '{ cwdMarker.meta.description = "selected evaluator cwd"; }\n',
        encoding="utf-8",
    )
    uri = (selected / "empty-project.nix").as_uri()
    with Client(
        args=(
            "--enable-project-config",
            "--nixpkgs-expr=import ./nixpkgs.nix",
            "--nixos-options-expr=",
        ),
        cwd=launch,
        env=test_environment(bin_dir),
    ) as client:
        client.initialize(root_uri=selected.as_uri())
        client.open_document(uri, "pkgs.cwdMarker\n")
        actual_cwd = formatted_text(client, 30, uri)
        assert Path(actual_cwd).resolve() == selected.resolve(), (
            actual_cwd,
            selected,
        )
        hover = client.request(31, "textDocument/hover", {
            "textDocument": {"uri": uri},
            "position": {"line": 0, "character": 9},
        })
        assert "selected evaluator cwd" in json.dumps(hover["result"]), hover


def exercise_no_fallback_roots(base):
    launch = base / "fallback-launch"
    lower = base / "fallback-lower"
    bin_dir = base / "fallback-bin"
    launch.mkdir()
    lower.mkdir()
    bin_dir.mkdir()
    write_formatter(bin_dir, "nixfmt")
    write_formatter(bin_dir, "must-not-load", "must-not-load")
    write_project(lower, "must-not-load")
    env = test_environment(bin_dir)
    uri = (launch / "fallback.nix").as_uri()

    missing = base / "missing-root"
    with Client(
        args=(*NO_PROVIDERS, "--enable-project-config"), cwd=launch, env=env
    ) as client:
        client.initialize(root_uri=missing.as_uri(), root_path=str(lower))
        expected = (
            "cannot use rootUri as project configuration root "
            f"{missing}: {os.strerror(errno.ENOENT)}"
        )
        expect_startup_warning(client, expected)
        client.open_document(uri, "{ value = 1; }\n")
        assert Path(formatted_text(client, 40, uri)).resolve() == launch.resolve()
        assert warning_messages(client) == [expected]

    missing_root_path = base / "missing-root-path"
    with Client(
        args=(*NO_PROVIDERS, "--enable-project-config"), cwd=launch, env=env
    ) as client:
        client.initialize(
            root_path=str(missing_root_path),
            workspace_folders=[{
                "uri": lower.as_uri(), "name": "must-not-fall-through"
            }],
        )
        expected = (
            "cannot use rootPath as project configuration root "
            f"{missing_root_path}: {os.strerror(errno.ENOENT)}"
        )
        expect_startup_warning(client, expected)
        client.open_document(uri, "{ value = 1; }\n")
        assert Path(formatted_text(client, 41, uri)).resolve() == launch.resolve()
        assert warning_messages(client) == [expected]

    missing_workspace = base / "missing-workspace"
    with Client(
        args=(*NO_PROVIDERS, "--enable-project-config"), cwd=launch, env=env
    ) as client:
        client.initialize(
            root_path="",
            workspace_folders=[{
                "uri": missing_workspace.as_uri(), "name": "missing"
            }],
        )
        expected = (
            "cannot use workspaceFolders[0] as project configuration root "
            f"{missing_workspace}: {os.strerror(errno.ENOENT)}"
        )
        expect_startup_warning(client, expected)
        client.open_document(uri, "{ value = 1; }\n")
        assert Path(formatted_text(client, 42, uri)).resolve() == launch.resolve()
        assert warning_messages(client) == [expected]

    with Client(
        args=(*NO_PROVIDERS, "--enable-project-config"), cwd=launch, env=env
    ) as client:
        client.initialize(
            root_uri="https://example.test/root", root_path=str(lower)
        )
        expected = (
            "cannot use rootUri as project configuration root: "
            "clangd only supports 'file' URI scheme for workspace files at "
            "(root).rootUri"
        )
        expect_startup_warning(client, expected)
        client.open_document(uri, "{ value = 1; }\n")
        assert Path(formatted_text(client, 43, uri)).resolve() == launch.resolve()
        assert warning_messages(client) == [expected]

    with Client(
        args=(*NO_PROVIDERS, "--enable-project-config"), cwd=launch, env=env
    ) as client:
        client.initialize(
            root_uri=lower.as_uri(),
            workspace_folders=[
                {"uri": lower.as_uri(), "name": "valid"},
                {"uri": 42, "name": "malformed"},
            ],
        )
        expected = (
            "cannot load project configuration with multiple workspace "
            "folders (2 supplied)"
        )
        expect_startup_warning(client, expected)
        client.open_document(uri, "{ value = 1; }\n")
        assert Path(formatted_text(client, 44, uri)).resolve() == launch.resolve()
        assert warning_messages(client) == [expected]


def exercise_no_ancestor_search(base):
    launch = base / "ancestor-launch"
    parent = base / "ancestor"
    child = parent / "child"
    bin_dir = base / "ancestor-bin"
    launch.mkdir()
    child.mkdir(parents=True)
    bin_dir.mkdir()
    write_formatter(bin_dir, "nixfmt")
    write_formatter(bin_dir, "ancestor-formatter", "ancestor")
    write_project(parent, "ancestor-formatter")
    uri = (child / "no-ancestor.nix").as_uri()
    with Client(
        args=(*NO_PROVIDERS, "--enable-project-config"),
        cwd=launch,
        env=test_environment(bin_dir),
    ) as client:
        client.initialize(root_uri=child.as_uri())
        client.initialized()
        client.assert_no_message(show_message)
        client.open_document(uri, "{ value = 1; }\n")
        assert Path(formatted_text(client, 50, uri)).resolve() == launch.resolve()


def exercise_source_selection():
    with tempfile.TemporaryDirectory() as temporary:
        base = Path(temporary)
        exercise_project_opt_in(base)
        exercise_explicit_config(base)
        exercise_root_precedence(base)
        exercise_empty_project_rebases(base)
        exercise_no_fallback_roots(base)
        exercise_no_ancestor_search(base)


def exercise_startup_case(base, kind, request_id):
    launch = base / f"{kind}-launch"
    selected = base / f"{kind}-selected"
    bin_dir = base / f"{kind}-bin"
    launch.mkdir()
    selected.mkdir()
    bin_dir.mkdir()
    write_formatter(bin_dir, "nixfmt")
    project_path = selected / ".nixd.json"
    if kind == "missing":
        expected = None
    elif kind == "invalid-json":
        project_path.write_text("{", encoding="utf-8")
        expected = (
            f"failed to load project configuration at {project_path}: "
            "JSON result cannot be parsed: [1:1, byte=1]: Expected object key"
        )
    elif kind == "invalid-schema":
        project_path.write_text(
            '{"formatting":{"command":42}}', encoding="utf-8"
        )
        expected = (
            f"failed to load project configuration at {project_path}: "
            "JSON schema mismatch: expected array at "
            "(root).formatting.command"
        )
    elif kind == "unreadable":
        project_path.symlink_to(project_path.name)
        expected = (
            f"failed to load project configuration at {project_path}: "
            f"{os.strerror(errno.ELOOP)}"
        )
    else:
        raise AssertionError(f"unknown startup case: {kind}")

    uri = (launch / f"{kind}.nix").as_uri()
    client = Client(
        args=(*NO_PROVIDERS, "--enable-project-config"),
        cwd=launch,
        env=test_environment(bin_dir),
    )
    try:
        client.initialize(root_uri=selected.as_uri())
        if expected is None:
            assert warning_messages(client) == []
            client.initialized()
            client.assert_no_message(show_message)
        else:
            logged = client.read_stderr_until(lambda text: expected in text)
            assert expected in logged, logged
            expect_startup_warning(client, expected)
        client.open_document(uri, "{ value = 1; }\n")
        assert (
            Path(formatted_text(client, request_id, uri)).resolve()
            == launch.resolve()
        )
        assert warning_messages(client) == ([] if expected is None else [expected])
    finally:
        client.close()


def exercise_startup_warnings():
    with tempfile.TemporaryDirectory() as temporary:
        base = Path(temporary)
        for request_id, kind in enumerate(
            ("missing", "invalid-json", "invalid-schema", "unreadable"), 60
        ):
            exercise_startup_case(base, kind, request_id)


def exercise_editor_overlays():
    with tempfile.TemporaryDirectory() as temporary:
        root = Path(temporary)
        bin_dir = root / "bin"
        bin_dir.mkdir()
        for marker in (
            "base",
            "editor-one",
            "editor-two",
            "retained",
            "newer-success",
            "newer-failure",
            "failure-barrier",
            "stale-success",
        ):
            write_formatter(bin_dir, marker, marker)
        write_project(root, "base")
        uri = (root / "editor-overlays.nix").as_uri()
        client = Client(
            args=(*NO_PROVIDERS, "--enable-project-config"),
            cwd=root,
            env=test_environment(bin_dir),
        )
        request_id = 100
        try:
            client.initialize(root_uri=root.as_uri(), capabilities=CAPABILITIES)
            initial = client.workspace_request()
            client.reply(initial, result=[None])
            client.open_document(uri, "{ value = 1; }\n")
            request_id = wait_for_formatted_text(
                client, uri, "base", request_id
            )

            request = request_editor_configuration(client)
            client.reply(request, result=editor_patch("editor-one"))
            request_id = wait_for_formatted_text(
                client, uri, "editor-one", request_id
            )
            request = request_editor_configuration(client)
            client.reply(request, result=[None])
            request_id = wait_for_formatted_text(
                client, uri, "base", request_id
            )

            request = request_editor_configuration(client)
            client.reply(request, result=editor_patch("editor-two"))
            request_id = wait_for_formatted_text(
                client, uri, "editor-two", request_id
            )
            request = request_editor_configuration(client)
            client.reply(request, result=[{}])
            request_id = wait_for_formatted_text(
                client, uri, "base", request_id
            )

            request = request_editor_configuration(client)
            client.reply(request, result=editor_patch("retained"))
            request_id = wait_for_formatted_text(
                client, uri, "retained", request_id
            )

            invalid_responses = (
                (
                    [{"formatting": {"command": 42}}],
                    "workspace/configuration: parse error expected array at "
                    "(root).formatting.command",
                ),
                (
                    [{}, {}],
                    "workspace/configuration: expected exactly one array item",
                ),
                (
                    [],
                    "workspace/configuration: expected exactly one array item",
                ),
            )
            for result, error_text in invalid_responses:
                before = client.stderr.count(error_text)
                request = request_editor_configuration(client)
                client.reply(request, result=result)
                client.read_stderr_until(
                    lambda text, before=before, error_text=error_text: (
                        text.count(error_text) > before
                    )
                )
                assert formatted_text(client, request_id, uri) == "retained"
                request_id += 1

            response_error = "active workspace response failed"
            request = request_editor_configuration(client)
            client.reply(request, error={"code": -32603, "message": response_error})
            client.read_stderr_until(lambda text: response_error in text)
            assert formatted_text(client, request_id, uri) == "retained"
            request_id += 1

            older = request_editor_configuration(client)
            newer = request_editor_configuration(client)
            client.reply(newer, result=editor_patch("newer-success"))
            request_id = wait_for_formatted_text(
                client, uri, "newer-success", request_id
            )
            client.reply(older, result=editor_patch("stale-success"))
            barrier_error = (
                "workspace/configuration: expected exactly one array item"
            )
            before = client.stderr.count(barrier_error)
            barrier = request_editor_configuration(client)
            client.reply(barrier, result=[])
            client.read_stderr_until(
                lambda text: text.count(barrier_error) > before
            )
            assert formatted_text(client, request_id, uri) == "newer-success"
            request_id += 1

            older = request_editor_configuration(client)
            newer = request_editor_configuration(client)
            client.reply(newer, result=editor_patch("newer-failure"))
            request_id = wait_for_formatted_text(
                client, uri, "newer-failure", request_id
            )
            stale_error = "delayed stale response must not be reported"
            client.reply(older, error={"code": -32603, "message": stale_error})
            barrier = request_editor_configuration(client)
            client.reply(barrier, result=editor_patch("failure-barrier"))
            wait_for_formatted_text(client, uri, "failure-barrier", request_id)
            assert stale_error not in client.stderr, client.stderr
        finally:
            client.close()


def exercise_provider_recovery():
    initial_description = "NIXD transition initial"
    recovered_description = "NIXD transition recovered"
    initial_uri = Path("/nixd-transition-initial.nix").as_uri()
    recovered_uri = Path("/nixd-transition-recovered.nix").as_uri()
    initial_expr = (
        '{ nixdTransition = { meta.description = "NIXD transition initial"; '
        'meta.position = "/nixd-transition-initial.nix:19"; '
        'version = "1.0.0"; }; }'
    )
    recovered_expr = (
        '{ nixdTransition = { meta.description = "NIXD transition recovered"; '
        'meta.position = "/nixd-transition-recovered.nix:29"; '
        'version = "2.0.0"; }; }'
    )
    with tempfile.TemporaryDirectory() as temporary:
        root = Path(temporary)
        bin_dir = root / "bin"
        bin_dir.mkdir()
        for marker in ("provider-initial", "provider-invalid", "provider-recovered"):
            write_formatter(bin_dir, marker, marker)
        config = {
            "formatting": {"command": ["provider-initial"]},
            "nixpkgs": {"expr": initial_expr},
            "options": {},
        }
        uri = (root / "provider-recovery.nix").as_uri()
        client = Client(
            args=("--config=" + json.dumps(config),),
            cwd=root,
            env=test_environment(bin_dir),
        )
        request_id = 200
        try:
            client.initialize(root_uri=root.as_uri(), capabilities=CAPABILITIES)
            initial_request = client.workspace_request()
            client.reply(initial_request, result=[None])
            client.open_document(uri, "with pkgs; [ nixdTransition ]\n")

            initial_completion = completion(client, request_id, uri)
            request_id += 1
            assert "nixdTransition" in completion_labels(initial_completion)
            initial_hover = hover(client, request_id, uri)
            request_id += 1
            assert initial_description in json.dumps(initial_hover), initial_hover
            initial_definition = definition(client, request_id, uri)
            request_id += 1
            assert initial_uri in json.dumps(initial_definition), initial_definition
            initial_inlay = inlay_hints(client, request_id, uri)
            request_id += 1
            assert any(
                hint.get("label") == ": 1.0.0" for hint in initial_inlay
            ), initial_inlay

            invalid = request_editor_configuration(client)
            client.reply(invalid, result=[{
                "formatting": {"command": ["provider-invalid"]},
                "nixpkgs": {
                    "expr": 'builtins.throw "nixd-transition-invalid"'
                },
                "options": {},
            }])
            request_id = wait_for_formatted_text(
                client, uri, "provider-invalid", request_id
            )
            invalid_completion = completion(client, request_id, uri)
            request_id += 1
            assert "nixdTransition" not in completion_labels(invalid_completion)
            invalid_hover = hover(client, request_id, uri)
            request_id += 1
            assert invalid_hover is None, invalid_hover
            invalid_definition = definition(client, request_id, uri)
            request_id += 1
            assert initial_uri not in json.dumps(invalid_definition), invalid_definition
            invalid_inlay = inlay_hints(client, request_id, uri)
            request_id += 1
            assert invalid_inlay == [], invalid_inlay

            recovered = request_editor_configuration(client)
            client.reply(recovered, result=[{
                "formatting": {"command": ["provider-recovered"]},
                "nixpkgs": {"expr": recovered_expr},
                "options": {},
            }])
            request_id = wait_for_formatted_text(
                client, uri, "provider-recovered", request_id
            )
            recovered_completion, request_id = wait_for_endpoint(
                client,
                lambda current, current_id: completion(current, current_id, uri),
                lambda result: "nixdTransition" in completion_labels(result),
                request_id,
                "completion",
            )
            assert "nixdTransition" in completion_labels(recovered_completion)
            recovered_hover, request_id = wait_for_endpoint(
                client,
                lambda current, current_id: hover(current, current_id, uri),
                lambda result: recovered_description in json.dumps(result),
                request_id,
                "hover",
            )
            assert recovered_description in json.dumps(recovered_hover)
            recovered_definition, request_id = wait_for_endpoint(
                client,
                lambda current, current_id: definition(current, current_id, uri),
                lambda result: recovered_uri in json.dumps(result),
                request_id,
                "definition",
            )
            assert recovered_uri in json.dumps(recovered_definition)
            recovered_inlay, request_id = wait_for_endpoint(
                client,
                lambda current, current_id: inlay_hints(current, current_id, uri),
                lambda result: any(
                    hint.get("label") == ": 2.0.0" for hint in result
                ),
                request_id,
                "inlay hints",
            )
            assert any(
                hint.get("label") == ": 2.0.0" for hint in recovered_inlay
            )
        finally:
            client.close()


SCENARIOS = {
    "empty-formatter": exercise_empty_formatter,
    "source-selection": exercise_source_selection,
    "startup-warnings": exercise_startup_warnings,
    "editor-overlays": exercise_editor_overlays,
    "provider-recovery": exercise_provider_recovery,
}


def main():
    if len(sys.argv) != 2 or sys.argv[1] not in SCENARIOS:
        names = ", ".join(sorted(SCENARIOS))
        raise SystemExit(f"usage: {Path(sys.argv[0]).name} {{{names}}}")
    SCENARIOS[sys.argv[1]]()


if __name__ == "__main__":
    main()
