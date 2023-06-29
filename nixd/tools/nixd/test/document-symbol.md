# RUN: nixd --lit-test < %s | FileCheck %s

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

```nix
let
  bar = 1;
  foo = { z = 1;} ;
{
    x = bar;
    y = with foo; z;
    nested = {
        n1 = 1;
        n2 = 2;
    };
    nested.a = 3;
}

```

```json
{
   "jsonrpc":"2.0",
   "method":"textDocument/didOpen",
   "params":{
      "textDocument":{
         "uri":"file:///symbol.nix",
         "languageId":"nix",
         "version":1,
         "text":"let\r\n  bar = 1;\r\n  foo = { z = 1; };\r\nin\r\n{\r\n  x = bar;\r\n  y = with foo; z;\r\n  nested = {\r\n    n1 = 1;\r\n    n2 = 2;\r\n  };\r\n  nested.a = 3;\r\n}\r\n"
      }
   }
}
```

<-- textDocument/documentSymbol(1)

```json
{
    "jsonrpc": "2.0",
    "id": 2,
    "method": "textDocument/documentSymbol",
    "params": {
        "textDocument": {
            "uri": "file:///symbol.nix"
        }
    }
}
```

```
     CHECK:    "result": [
CHECK-NEXT:    {
CHECK-NEXT:      "kind": 8,
CHECK-NEXT:      "name": "bar",
CHECK-NEXT:      "range": {
CHECK-NEXT:        "end": {
CHECK-NEXT:          "character": 9,
CHECK-NEXT:          "line": 1
CHECK-NEXT:        },
CHECK-NEXT:        "start": {
CHECK-NEXT:          "character": 2,
CHECK-NEXT:          "line": 1
CHECK-NEXT:        }
CHECK-NEXT:      },
CHECK-NEXT:      "selectionRange": {
CHECK-NEXT:        "end": {
CHECK-NEXT:          "character": 5,
CHECK-NEXT:          "line": 1
CHECK-NEXT:        },
CHECK-NEXT:        "start": {
CHECK-NEXT:          "character": 2,
CHECK-NEXT:          "line": 1
CHECK-NEXT:        }
CHECK-NEXT:      }
CHECK-NEXT:    },
CHECK-NEXT:    {
CHECK-NEXT:      "children": [
CHECK-NEXT:        {
CHECK-NEXT:          "kind": 8,
CHECK-NEXT:          "name": "z",
CHECK-NEXT:          "range": {
CHECK-NEXT:            "end": {
CHECK-NEXT:              "character": 15,
CHECK-NEXT:              "line": 2
CHECK-NEXT:            },
CHECK-NEXT:            "start": {
CHECK-NEXT:              "character": 10,
CHECK-NEXT:              "line": 2
CHECK-NEXT:            }
CHECK-NEXT:          },
CHECK-NEXT:          "selectionRange": {
CHECK-NEXT:            "end": {
CHECK-NEXT:              "character": 11,
CHECK-NEXT:              "line": 2
CHECK-NEXT:            },
CHECK-NEXT:            "start": {
CHECK-NEXT:              "character": 10,
CHECK-NEXT:              "line": 2
CHECK-NEXT:            }
CHECK-NEXT:          }
CHECK-NEXT:        }
CHECK-NEXT:      ],
CHECK-NEXT:      "kind": 8,
CHECK-NEXT:      "name": "foo",
CHECK-NEXT:      "range": {
CHECK-NEXT:        "end": {
CHECK-NEXT:          "character": 18,
CHECK-NEXT:          "line": 2
CHECK-NEXT:        },
CHECK-NEXT:        "start": {
CHECK-NEXT:          "character": 2,
CHECK-NEXT:          "line": 2
CHECK-NEXT:        }
CHECK-NEXT:      },
CHECK-NEXT:      "selectionRange": {
CHECK-NEXT:        "end": {
CHECK-NEXT:          "character": 5,
CHECK-NEXT:          "line": 2
CHECK-NEXT:        },
CHECK-NEXT:        "start": {
CHECK-NEXT:          "character": 2,
CHECK-NEXT:          "line": 2
CHECK-NEXT:        }
CHECK-NEXT:      }
CHECK-NEXT:    },
CHECK-NEXT:    {
CHECK-NEXT:      "children": [
CHECK-NEXT:        {
CHECK-NEXT:          "kind": 13,
CHECK-NEXT:          "name": "bar",
CHECK-NEXT:          "range": {
CHECK-NEXT:            "end": {
CHECK-NEXT:              "character": 9,
CHECK-NEXT:              "line": 5
CHECK-NEXT:            },
CHECK-NEXT:            "start": {
CHECK-NEXT:              "character": 6,
CHECK-NEXT:              "line": 5
CHECK-NEXT:            }
CHECK-NEXT:          },
CHECK-NEXT:          "selectionRange": {
CHECK-NEXT:            "end": {
CHECK-NEXT:              "character": 9,
CHECK-NEXT:              "line": 5
CHECK-NEXT:            },
CHECK-NEXT:            "start": {
CHECK-NEXT:              "character": 6,
CHECK-NEXT:              "line": 5
CHECK-NEXT:            }
CHECK-NEXT:          }
CHECK-NEXT:        }
CHECK-NEXT:      ],
CHECK-NEXT:      "kind": 8,
CHECK-NEXT:      "name": "x",
CHECK-NEXT:      "range": {
CHECK-NEXT:        "end": {
CHECK-NEXT:          "character": 9,
CHECK-NEXT:          "line": 5
CHECK-NEXT:        },
CHECK-NEXT:        "start": {
CHECK-NEXT:          "character": 2,
CHECK-NEXT:          "line": 5
CHECK-NEXT:        }
CHECK-NEXT:      },
CHECK-NEXT:      "selectionRange": {
CHECK-NEXT:        "end": {
CHECK-NEXT:          "character": 3,
CHECK-NEXT:          "line": 5
CHECK-NEXT:        },
CHECK-NEXT:        "start": {
CHECK-NEXT:          "character": 2,
CHECK-NEXT:          "line": 5
CHECK-NEXT:        }
CHECK-NEXT:      }
CHECK-NEXT:    },
CHECK-NEXT:    {
CHECK-NEXT:      "children": [
CHECK-NEXT:        {
CHECK-NEXT:          "kind": 13,
CHECK-NEXT:          "name": "foo",
CHECK-NEXT:          "range": {
CHECK-NEXT:            "end": {
CHECK-NEXT:              "character": 14,
CHECK-NEXT:              "line": 6
CHECK-NEXT:            },
CHECK-NEXT:            "start": {
CHECK-NEXT:              "character": 11,
CHECK-NEXT:              "line": 6
CHECK-NEXT:            }
CHECK-NEXT:          },
CHECK-NEXT:          "selectionRange": {
CHECK-NEXT:            "end": {
CHECK-NEXT:              "character": 14,
CHECK-NEXT:              "line": 6
CHECK-NEXT:            },
CHECK-NEXT:            "start": {
CHECK-NEXT:              "character": 11,
CHECK-NEXT:              "line": 6
CHECK-NEXT:            }
CHECK-NEXT:          }
CHECK-NEXT:        },
CHECK-NEXT:        {
CHECK-NEXT:          "kind": 13,
CHECK-NEXT:          "name": "z",
CHECK-NEXT:          "range": {
CHECK-NEXT:            "end": {
CHECK-NEXT:              "character": 17,
CHECK-NEXT:              "line": 6
CHECK-NEXT:            },
CHECK-NEXT:            "start": {
CHECK-NEXT:              "character": 16,
CHECK-NEXT:              "line": 6
CHECK-NEXT:            }
CHECK-NEXT:          },
CHECK-NEXT:          "selectionRange": {
CHECK-NEXT:            "end": {
CHECK-NEXT:              "character": 17,
CHECK-NEXT:              "line": 6
CHECK-NEXT:            },
CHECK-NEXT:            "start": {
CHECK-NEXT:              "character": 16,
CHECK-NEXT:              "line": 6
CHECK-NEXT:            }
CHECK-NEXT:          }
CHECK-NEXT:        }
CHECK-NEXT:      ],
CHECK-NEXT:      "kind": 8,
CHECK-NEXT:      "name": "y",
CHECK-NEXT:      "range": {
CHECK-NEXT:        "end": {
CHECK-NEXT:          "character": 17,
CHECK-NEXT:          "line": 6
CHECK-NEXT:        },
CHECK-NEXT:        "start": {
CHECK-NEXT:          "character": 2,
CHECK-NEXT:          "line": 6
CHECK-NEXT:        }
CHECK-NEXT:      },
CHECK-NEXT:      "selectionRange": {
CHECK-NEXT:        "end": {
CHECK-NEXT:          "character": 3,
CHECK-NEXT:          "line": 6
CHECK-NEXT:        },
CHECK-NEXT:        "start": {
CHECK-NEXT:          "character": 2,
CHECK-NEXT:          "line": 6
CHECK-NEXT:        }
CHECK-NEXT:      }
CHECK-NEXT:    },
CHECK-NEXT:    {
CHECK-NEXT:      "children": [
CHECK-NEXT:        {
CHECK-NEXT:          "kind": 8,
CHECK-NEXT:          "name": "n1",
CHECK-NEXT:          "range": {
CHECK-NEXT:            "end": {
CHECK-NEXT:              "character": 10,
CHECK-NEXT:              "line": 8
CHECK-NEXT:            },
CHECK-NEXT:            "start": {
CHECK-NEXT:              "character": 4,
CHECK-NEXT:              "line": 8
CHECK-NEXT:            }
CHECK-NEXT:          },
CHECK-NEXT:          "selectionRange": {
CHECK-NEXT:            "end": {
CHECK-NEXT:              "character": 6,
CHECK-NEXT:              "line": 8
CHECK-NEXT:            },
CHECK-NEXT:            "start": {
CHECK-NEXT:              "character": 4,
CHECK-NEXT:              "line": 8
CHECK-NEXT:            }
CHECK-NEXT:          }
CHECK-NEXT:        },
CHECK-NEXT:        {
CHECK-NEXT:          "kind": 8,
CHECK-NEXT:          "name": "n2",
CHECK-NEXT:          "range": {
CHECK-NEXT:            "end": {
CHECK-NEXT:              "character": 10,
CHECK-NEXT:              "line": 9
CHECK-NEXT:            },
CHECK-NEXT:            "start": {
CHECK-NEXT:              "character": 4,
CHECK-NEXT:              "line": 9
CHECK-NEXT:            }
CHECK-NEXT:          },
CHECK-NEXT:          "selectionRange": {
CHECK-NEXT:            "end": {
CHECK-NEXT:              "character": 6,
CHECK-NEXT:              "line": 9
CHECK-NEXT:            },
CHECK-NEXT:            "start": {
CHECK-NEXT:              "character": 4,
CHECK-NEXT:              "line": 9
CHECK-NEXT:            }
CHECK-NEXT:          }
CHECK-NEXT:        },
CHECK-NEXT:        {
CHECK-NEXT:          "kind": 8,
CHECK-NEXT:          "name": "a",
CHECK-NEXT:          "range": {
CHECK-NEXT:            "end": {
CHECK-NEXT:              "character": 14,
CHECK-NEXT:              "line": 11
CHECK-NEXT:            },
CHECK-NEXT:            "start": {
CHECK-NEXT:              "character": 2,
CHECK-NEXT:              "line": 11
CHECK-NEXT:            }
CHECK-NEXT:          },
CHECK-NEXT:          "selectionRange": {
CHECK-NEXT:            "end": {
CHECK-NEXT:              "character": 10,
CHECK-NEXT:              "line": 11
CHECK-NEXT:            },
CHECK-NEXT:            "start": {
CHECK-NEXT:              "character": 2,
CHECK-NEXT:              "line": 11
CHECK-NEXT:            }
CHECK-NEXT:          }
CHECK-NEXT:        }
CHECK-NEXT:      ],
CHECK-NEXT:      "kind": 8,
CHECK-NEXT:      "name": "nested",
CHECK-NEXT:      "range": {
CHECK-NEXT:        "end": {
CHECK-NEXT:          "character": 3,
CHECK-NEXT:          "line": 10
CHECK-NEXT:        },
CHECK-NEXT:        "start": {
CHECK-NEXT:          "character": 2,
CHECK-NEXT:          "line": 7
CHECK-NEXT:        }
CHECK-NEXT:      },
CHECK-NEXT:      "selectionRange": {
CHECK-NEXT:        "end": {
CHECK-NEXT:          "character": 8,
CHECK-NEXT:          "line": 7
CHECK-NEXT:        },
CHECK-NEXT:        "start": {
CHECK-NEXT:          "character": 2,
CHECK-NEXT:          "line": 7
CHECK-NEXT:        }
CHECK-NEXT:      }
CHECK-NEXT:    }
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
