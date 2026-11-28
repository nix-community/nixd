# RUN: rm -rf %t && mkdir -p %t
# RUN: printf '{ pkgs, ... }: { options = { schemaBar = { inner = { _type = "option"; }; }; }; }\n' > %t/mod.nix
# RUN: sed "s|@DIR@|%t|g" %s > %t/in.md
# RUN: nixd --lit-test \
# RUN: --config='{"schemaDirective":{"enable":true}}' \
# RUN: --nixos-options-expr="{ foo.bar = { _type = \"option\"; }; }" \
# RUN: --nixpkgs-expr="{ lib = { mkDefault = x: x; evalModules = { modules }: (import (builtins.head modules)) (builtins.elemAt modules 1)._module.args; }; }" \
# RUN: < %t/in.md > %t/out.json
# RUN: FileCheck %s --check-prefix=SCHEMA < %t/out.json
# RUN: FileCheck %s --check-prefix=PLAIN < %t/out.json

A document naming its own option module gets that module's options, and only
those. A document that names nothing keeps the workspace-configured ones. The
module takes `pkgs`, which it gets as a module argument, as under NixOS; the
directive sits below a comment header, still in the leading comment block.

The two completions are answered by different workers, so their replies may
interleave: each is checked by its own prefix over the whole transcript.

<-- initialize(0)

```json
{
   "jsonrpc":"2.0",
   "id":0,
   "method":"initialize",
   "params":{
      "processId":123,
      "rootPath":"",
      "capabilities":{
      },
      "trace":"off"
   }
}
```


<-- textDocument/didOpen


```nix file://@DIR@/config.nix
# SPDX-License-Identifier: MIT
#:schema ./mod.nix
{ bar = 1; sc }
```

```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "method": "textDocument/completion",
    "params": {
        "textDocument": {
            "uri": "file://@DIR@/config.nix"
        },
        "position": {
            "line": 2,
            "character": 12
        },
        "context": {
            "triggerKind": 1
        }
    }
}
```

The directive's module is the only provider consulted: `foo`, declared by the
workspace-configured `nixos` provider, is absent, and the item list closes
right after the single item.

```
     SCHEMA: "id": 1,
SCHEMA-NEXT:  "jsonrpc": "2.0",
SCHEMA-NEXT:  "result": {
SCHEMA-NEXT:    "isIncomplete": false,
SCHEMA-NEXT:    "items": [
SCHEMA-NEXT:      {
SCHEMA-NEXT:        "data": "",
SCHEMA-NEXT:        "detail": "{{.*}}/mod.nix",
SCHEMA-NEXT:        "kind": 7,
SCHEMA-NEXT:        "label": "schemaBar",
SCHEMA-NEXT:        "score": 0,
SCHEMA-NEXT:        "textEdit": {
SCHEMA-NEXT:          "newText": "schemaBar",
SCHEMA-NEXT:          "range": {
SCHEMA-NEXT:            "end": {
SCHEMA-NEXT:              "character": 13,
SCHEMA-NEXT:              "line": 2
SCHEMA-NEXT:            },
SCHEMA-NEXT:            "start": {
SCHEMA-NEXT:              "character": 11,
SCHEMA-NEXT:              "line": 2
SCHEMA-NEXT:            }
SCHEMA-NEXT:          }
SCHEMA-NEXT:        }
SCHEMA-NEXT:      }
SCHEMA-NEXT:    ]
```


<-- textDocument/didOpen


```nix file:///plain.nix
{ bar = 1; fo }
```

```json
{
    "jsonrpc": "2.0",
    "id": 2,
    "method": "textDocument/completion",
    "params": {
        "textDocument": {
            "uri": "file:///plain.nix"
        },
        "position": {
            "line": 0,
            "character": 12
        },
        "context": {
            "triggerKind": 1
        }
    }
}
```

A document with no directive is untouched by any of this.

```
     PLAIN: "id": 2,
PLAIN-NEXT:  "jsonrpc": "2.0",
PLAIN-NEXT:  "result": {
PLAIN-NEXT:    "isIncomplete": false,
PLAIN-NEXT:    "items": [
PLAIN-NEXT:      {
PLAIN-NEXT:        "data": "",
PLAIN-NEXT:        "detail": "nixos",
PLAIN-NEXT:        "kind": 7,
PLAIN-NEXT:        "label": "foo",
```


```json
{"jsonrpc":"2.0","method":"exit"}
```
