# RUN: nixd-attrset-eval --lit-test < %s | FileCheck %s


```json
{
   "jsonrpc":"2.0",
   "id":0,
   "method":"attrset/evalExpr",
   "params": "import <nixpkgs> { }"
}
```


```json
{
   "jsonrpc":"2.0",
   "id":1,
   "method":"attrset/attrpathComplete",
   "params": {
        "Scope": [ ],
        "Prefix": "py"
   }
}
```

```
     CHECK:   "id": 1,
CHECK-NEXT:   "jsonrpc": "2.0",
CHECK-NEXT:   "result": [
CHECK-NEXT:     "py-spy",
CHECK-NEXT:     "py3c",
CHECK-NEXT:     "py65",
CHECK-NEXT:     "pyCA",
CHECK-NEXT:     "pycflow2dot",
CHECK-NEXT:     "pycoin",
CHECK-NEXT:     "pycritty",
CHECK-NEXT:     "pydeps",
CHECK-NEXT:     "pydf",
CHECK-NEXT:     "pyditz",
CHECK-NEXT:     "pyenv",
CHECK-NEXT:     "pygmentex",
CHECK-NEXT:     "pyinfra",
CHECK-NEXT:     "pykms",
CHECK-NEXT:     "pylint",
CHECK-NEXT:     "pylint-exit",
CHECK-NEXT:     "pyload-ng",
CHECK-NEXT:     "pylode",
CHECK-NEXT:     "pyls-black",
CHECK-NEXT:     "pyls-mypy",
CHECK-NEXT:     "pylyzer",
CHECK-NEXT:     "pymol",
CHECK-NEXT:     "pympress",
CHECK-NEXT:     "pynac",
CHECK-NEXT:     "pynitrokey",
CHECK-NEXT:     "pyo3-pack",
CHECK-NEXT:     "pyocd",
CHECK-NEXT:     "pyotherside",
CHECK-NEXT:     "pyp",
CHECK-NEXT:     "pypass",
CHECK-NEXT:     "pypi-mirror"
CHECK-NEXT:   ]
```

```json
{"jsonrpc":"2.0","method":"exit"}
```

