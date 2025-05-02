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


<-- textDocument/didOpen


```nix file:///basic.nix
{
    x = 1;
    anonymousLambda = { a }: a;
    namedLambda = a: a;
    numbers = 1 + 2.0;
    bool = true;
    bool2= false;
    string = "1";
    string2 = "${builtins.foo}";
    undefined = x;
    list = [ 1 2 3 ];
}

```

<-- textDocument/documentSymbol(2)


```json
{
   "jsonrpc":"2.0",
   "id":2,
   "method":"textDocument/documentSymbol",
   "params":{
      "textDocument":{
         "uri":"file:///basic.nix"
      }
   }
}
```

```
     CHECK:  "id": 2,
CHECK-NEXT:  "jsonrpc": "2.0",
CHECK-NEXT:  "result": [
CHECK-NEXT:    {
CHECK-NEXT:      "children": [
CHECK-NEXT:        {
CHECK-NEXT:          "children": [
CHECK-NEXT:            {
CHECK-NEXT:              "detail": "identifier",
CHECK-NEXT:              "kind": 14,
CHECK-NEXT:              "name": "a",
CHECK-NEXT:              "range": {
CHECK-NEXT:                "end": {
CHECK-NEXT:                  "character": 30,
CHECK-NEXT:                  "line": 2
CHECK-NEXT:                },
CHECK-NEXT:                "start": {
CHECK-NEXT:                  "character": 29,
CHECK-NEXT:                  "line": 2
CHECK-NEXT:                }
CHECK-NEXT:              },
CHECK-NEXT:              "selectionRange": {
CHECK-NEXT:                "end": {
CHECK-NEXT:                  "character": 30,
CHECK-NEXT:                  "line": 2
CHECK-NEXT:                },
CHECK-NEXT:                "start": {
CHECK-NEXT:                  "character": 29,
CHECK-NEXT:                  "line": 2
CHECK-NEXT:                }
CHECK-NEXT:              }
CHECK-NEXT:            }
CHECK-NEXT:          ],
CHECK-NEXT:          "detail": "lambda",
CHECK-NEXT:          "kind": 12,
CHECK-NEXT:          "name": "(anonymous lambda)",
CHECK-NEXT:          "range": {
CHECK-NEXT:            "end": {
CHECK-NEXT:              "character": 30,
CHECK-NEXT:              "line": 2
CHECK-NEXT:            },
CHECK-NEXT:            "start": {
CHECK-NEXT:              "character": 22,
CHECK-NEXT:              "line": 2
CHECK-NEXT:            }
CHECK-NEXT:          },
CHECK-NEXT:          "selectionRange": {
CHECK-NEXT:            "end": {
CHECK-NEXT:              "character": 27,
CHECK-NEXT:              "line": 2
CHECK-NEXT:            },
CHECK-NEXT:            "start": {
CHECK-NEXT:              "character": 22,
CHECK-NEXT:              "line": 2
CHECK-NEXT:            }
CHECK-NEXT:          }
CHECK-NEXT:        }
CHECK-NEXT:      ],
CHECK-NEXT:      "detail": "attribute",
CHECK-NEXT:      "kind": 8,
CHECK-NEXT:      "name": "anonymousLambda",
CHECK-NEXT:      "range": {
CHECK-NEXT:        "end": {
CHECK-NEXT:          "character": 30,
CHECK-NEXT:          "line": 2
CHECK-NEXT:        },
CHECK-NEXT:        "start": {
CHECK-NEXT:          "character": 4,
CHECK-NEXT:          "line": 2
CHECK-NEXT:        }
CHECK-NEXT:      },
CHECK-NEXT:      "selectionRange": {
CHECK-NEXT:        "end": {
CHECK-NEXT:          "character": 19,
CHECK-NEXT:          "line": 2
CHECK-NEXT:        },
CHECK-NEXT:        "start": {
CHECK-NEXT:          "character": 4,
CHECK-NEXT:          "line": 2
CHECK-NEXT:        }
CHECK-NEXT:      }
CHECK-NEXT:    },
CHECK-NEXT:    {
CHECK-NEXT:      "children": [
CHECK-NEXT:        {
CHECK-NEXT:          "detail": "builtin boolean",
CHECK-NEXT:          "kind": 17,
CHECK-NEXT:          "name": "true",
CHECK-NEXT:          "range": {
CHECK-NEXT:            "end": {
CHECK-NEXT:              "character": 15,
CHECK-NEXT:              "line": 5
CHECK-NEXT:            },
CHECK-NEXT:            "start": {
CHECK-NEXT:              "character": 11,
CHECK-NEXT:              "line": 5
CHECK-NEXT:            }
CHECK-NEXT:          },
CHECK-NEXT:          "selectionRange": {
CHECK-NEXT:            "end": {
CHECK-NEXT:              "character": 15,
CHECK-NEXT:              "line": 5
CHECK-NEXT:            },
CHECK-NEXT:            "start": {
CHECK-NEXT:              "character": 11,
CHECK-NEXT:              "line": 5
CHECK-NEXT:            }
CHECK-NEXT:          }
CHECK-NEXT:        }
CHECK-NEXT:      ],
CHECK-NEXT:      "detail": "attribute",
CHECK-NEXT:      "kind": 8,
CHECK-NEXT:      "name": "bool",
CHECK-NEXT:      "range": {
CHECK-NEXT:        "end": {
CHECK-NEXT:          "character": 15,
CHECK-NEXT:          "line": 5
CHECK-NEXT:        },
CHECK-NEXT:        "start": {
CHECK-NEXT:          "character": 4,
CHECK-NEXT:          "line": 5
CHECK-NEXT:        }
CHECK-NEXT:      },
CHECK-NEXT:      "selectionRange": {
CHECK-NEXT:        "end": {
CHECK-NEXT:          "character": 8,
CHECK-NEXT:          "line": 5
CHECK-NEXT:        },
CHECK-NEXT:        "start": {
CHECK-NEXT:          "character": 4,
CHECK-NEXT:          "line": 5
CHECK-NEXT:        }
CHECK-NEXT:      }
CHECK-NEXT:    },
CHECK-NEXT:    {
CHECK-NEXT:      "children": [
CHECK-NEXT:        {
CHECK-NEXT:          "detail": "builtin boolean",
CHECK-NEXT:          "kind": 17,
CHECK-NEXT:          "name": "false",
CHECK-NEXT:          "range": {
CHECK-NEXT:            "end": {
CHECK-NEXT:              "character": 16,
CHECK-NEXT:              "line": 6
CHECK-NEXT:            },
CHECK-NEXT:            "start": {
CHECK-NEXT:              "character": 11,
CHECK-NEXT:              "line": 6
CHECK-NEXT:            }
CHECK-NEXT:          },
CHECK-NEXT:          "selectionRange": {
CHECK-NEXT:            "end": {
CHECK-NEXT:              "character": 16,
CHECK-NEXT:              "line": 6
CHECK-NEXT:            },
CHECK-NEXT:            "start": {
CHECK-NEXT:              "character": 11,
CHECK-NEXT:              "line": 6
CHECK-NEXT:            }
CHECK-NEXT:          }
CHECK-NEXT:        }
CHECK-NEXT:      ],
CHECK-NEXT:      "detail": "attribute",
CHECK-NEXT:      "kind": 8,
CHECK-NEXT:      "name": "bool2",
CHECK-NEXT:      "range": {
CHECK-NEXT:        "end": {
CHECK-NEXT:          "character": 16,
CHECK-NEXT:          "line": 6
CHECK-NEXT:        },
CHECK-NEXT:        "start": {
CHECK-NEXT:          "character": 4,
CHECK-NEXT:          "line": 6
CHECK-NEXT:        }
CHECK-NEXT:      },
CHECK-NEXT:      "selectionRange": {
CHECK-NEXT:        "end": {
CHECK-NEXT:          "character": 9,
CHECK-NEXT:          "line": 6
CHECK-NEXT:        },
CHECK-NEXT:        "start": {
CHECK-NEXT:          "character": 4,
CHECK-NEXT:          "line": 6
CHECK-NEXT:        }
CHECK-NEXT:      }
CHECK-NEXT:    },
CHECK-NEXT:    {
CHECK-NEXT:      "children": [
CHECK-NEXT:        {
CHECK-NEXT:          "children": [
CHECK-NEXT:            {
CHECK-NEXT:              "detail": "integer",
CHECK-NEXT:              "kind": 16,
CHECK-NEXT:              "name": "1",
CHECK-NEXT:              "range": {
CHECK-NEXT:                "end": {
CHECK-NEXT:                  "character": 14,
CHECK-NEXT:                  "line": 10
CHECK-NEXT:                },
CHECK-NEXT:                "start": {
CHECK-NEXT:                  "character": 13,
CHECK-NEXT:                  "line": 10
CHECK-NEXT:                }
CHECK-NEXT:              },
CHECK-NEXT:              "selectionRange": {
CHECK-NEXT:                "end": {
CHECK-NEXT:                  "character": 14,
CHECK-NEXT:                  "line": 10
CHECK-NEXT:                },
CHECK-NEXT:                "start": {
CHECK-NEXT:                  "character": 13,
CHECK-NEXT:                  "line": 10
CHECK-NEXT:                }
CHECK-NEXT:              }
CHECK-NEXT:            },
CHECK-NEXT:            {
CHECK-NEXT:              "detail": "integer",
CHECK-NEXT:              "kind": 16,
CHECK-NEXT:              "name": "2",
CHECK-NEXT:              "range": {
CHECK-NEXT:                "end": {
CHECK-NEXT:                  "character": 16,
CHECK-NEXT:                  "line": 10
CHECK-NEXT:                },
CHECK-NEXT:                "start": {
CHECK-NEXT:                  "character": 15,
CHECK-NEXT:                  "line": 10
CHECK-NEXT:                }
CHECK-NEXT:              },
CHECK-NEXT:              "selectionRange": {
CHECK-NEXT:                "end": {
CHECK-NEXT:                  "character": 16,
CHECK-NEXT:                  "line": 10
CHECK-NEXT:                },
CHECK-NEXT:                "start": {
CHECK-NEXT:                  "character": 15,
CHECK-NEXT:                  "line": 10
CHECK-NEXT:                }
CHECK-NEXT:              }
CHECK-NEXT:            },
CHECK-NEXT:            {
CHECK-NEXT:              "detail": "integer",
CHECK-NEXT:              "kind": 16,
CHECK-NEXT:              "name": "3",
CHECK-NEXT:              "range": {
CHECK-NEXT:                "end": {
CHECK-NEXT:                  "character": 18,
CHECK-NEXT:                  "line": 10
CHECK-NEXT:                },
CHECK-NEXT:                "start": {
CHECK-NEXT:                  "character": 17,
CHECK-NEXT:                  "line": 10
CHECK-NEXT:                }
CHECK-NEXT:              },
CHECK-NEXT:              "selectionRange": {
CHECK-NEXT:                "end": {
CHECK-NEXT:                  "character": 18,
CHECK-NEXT:                  "line": 10
CHECK-NEXT:                },
CHECK-NEXT:                "start": {
CHECK-NEXT:                  "character": 17,
CHECK-NEXT:                  "line": 10
CHECK-NEXT:                }
CHECK-NEXT:              }
CHECK-NEXT:            }
CHECK-NEXT:          ],
CHECK-NEXT:          "detail": "list",
CHECK-NEXT:          "kind": 18,
CHECK-NEXT:          "name": "{anonymous}",
CHECK-NEXT:          "range": {
CHECK-NEXT:            "end": {
CHECK-NEXT:              "character": 20,
CHECK-NEXT:              "line": 10
CHECK-NEXT:            },
CHECK-NEXT:            "start": {
CHECK-NEXT:              "character": 11,
CHECK-NEXT:              "line": 10
CHECK-NEXT:            }
CHECK-NEXT:          },
CHECK-NEXT:          "selectionRange": {
CHECK-NEXT:            "end": {
CHECK-NEXT:              "character": 20,
CHECK-NEXT:              "line": 10
CHECK-NEXT:            },
CHECK-NEXT:            "start": {
CHECK-NEXT:              "character": 11,
CHECK-NEXT:              "line": 10
CHECK-NEXT:            }
CHECK-NEXT:          }
CHECK-NEXT:        }
CHECK-NEXT:      ],
CHECK-NEXT:      "detail": "attribute",
CHECK-NEXT:      "kind": 8,
CHECK-NEXT:      "name": "list",
CHECK-NEXT:      "range": {
CHECK-NEXT:        "end": {
CHECK-NEXT:          "character": 20,
CHECK-NEXT:          "line": 10
CHECK-NEXT:        },
CHECK-NEXT:        "start": {
CHECK-NEXT:          "character": 4,
CHECK-NEXT:          "line": 10
CHECK-NEXT:        }
CHECK-NEXT:      },
CHECK-NEXT:      "selectionRange": {
CHECK-NEXT:        "end": {
CHECK-NEXT:          "character": 8,
CHECK-NEXT:          "line": 10
CHECK-NEXT:        },
CHECK-NEXT:        "start": {
CHECK-NEXT:          "character": 4,
CHECK-NEXT:          "line": 10
CHECK-NEXT:        }
CHECK-NEXT:      }
CHECK-NEXT:    },
CHECK-NEXT:    {
CHECK-NEXT:      "children": [
CHECK-NEXT:        {
CHECK-NEXT:          "children": [
CHECK-NEXT:            {
CHECK-NEXT:              "detail": "identifier",
CHECK-NEXT:              "kind": 14,
CHECK-NEXT:              "name": "a",
CHECK-NEXT:              "range": {
CHECK-NEXT:                "end": {
CHECK-NEXT:                  "character": 22,
CHECK-NEXT:                  "line": 3
CHECK-NEXT:                },
CHECK-NEXT:                "start": {
CHECK-NEXT:                  "character": 21,
CHECK-NEXT:                  "line": 3
CHECK-NEXT:                }
CHECK-NEXT:              },
CHECK-NEXT:              "selectionRange": {
CHECK-NEXT:                "end": {
CHECK-NEXT:                  "character": 22,
CHECK-NEXT:                  "line": 3
CHECK-NEXT:                },
CHECK-NEXT:                "start": {
CHECK-NEXT:                  "character": 21,
CHECK-NEXT:                  "line": 3
CHECK-NEXT:                }
CHECK-NEXT:              }
CHECK-NEXT:            }
CHECK-NEXT:          ],
CHECK-NEXT:          "detail": "lambda",
CHECK-NEXT:          "kind": 12,
CHECK-NEXT:          "name": "a",
CHECK-NEXT:          "range": {
CHECK-NEXT:            "end": {
CHECK-NEXT:              "character": 22,
CHECK-NEXT:              "line": 3
CHECK-NEXT:            },
CHECK-NEXT:            "start": {
CHECK-NEXT:              "character": 18,
CHECK-NEXT:              "line": 3
CHECK-NEXT:            }
CHECK-NEXT:          },
CHECK-NEXT:          "selectionRange": {
CHECK-NEXT:            "end": {
CHECK-NEXT:              "character": 19,
CHECK-NEXT:              "line": 3
CHECK-NEXT:            },
CHECK-NEXT:            "start": {
CHECK-NEXT:              "character": 18,
CHECK-NEXT:              "line": 3
CHECK-NEXT:            }
CHECK-NEXT:          }
CHECK-NEXT:        }
CHECK-NEXT:      ],
CHECK-NEXT:      "detail": "attribute",
CHECK-NEXT:      "kind": 8,
CHECK-NEXT:      "name": "namedLambda",
CHECK-NEXT:      "range": {
CHECK-NEXT:        "end": {
CHECK-NEXT:          "character": 22,
CHECK-NEXT:          "line": 3
CHECK-NEXT:        },
CHECK-NEXT:        "start": {
CHECK-NEXT:          "character": 4,
CHECK-NEXT:          "line": 3
CHECK-NEXT:        }
CHECK-NEXT:      },
CHECK-NEXT:      "selectionRange": {
CHECK-NEXT:        "end": {
CHECK-NEXT:          "character": 15,
CHECK-NEXT:          "line": 3
CHECK-NEXT:        },
CHECK-NEXT:        "start": {
CHECK-NEXT:          "character": 4,
CHECK-NEXT:          "line": 3
CHECK-NEXT:        }
CHECK-NEXT:      }
CHECK-NEXT:    },
CHECK-NEXT:    {
CHECK-NEXT:      "children": [
CHECK-NEXT:        {
CHECK-NEXT:          "detail": "integer",
CHECK-NEXT:          "kind": 16,
CHECK-NEXT:          "name": "1",
CHECK-NEXT:          "range": {
CHECK-NEXT:            "end": {
CHECK-NEXT:              "character": 15,
CHECK-NEXT:              "line": 4
CHECK-NEXT:            },
CHECK-NEXT:            "start": {
CHECK-NEXT:              "character": 14,
CHECK-NEXT:              "line": 4
CHECK-NEXT:            }
CHECK-NEXT:          },
CHECK-NEXT:          "selectionRange": {
CHECK-NEXT:            "end": {
CHECK-NEXT:              "character": 15,
CHECK-NEXT:              "line": 4
CHECK-NEXT:            },
CHECK-NEXT:            "start": {
CHECK-NEXT:              "character": 14,
CHECK-NEXT:              "line": 4
CHECK-NEXT:            }
CHECK-NEXT:          }
CHECK-NEXT:        },
CHECK-NEXT:        {
CHECK-NEXT:          "detail": "float",
CHECK-NEXT:          "kind": 16,
CHECK-NEXT:          "name": "2.000000",
CHECK-NEXT:          "range": {
CHECK-NEXT:            "end": {
CHECK-NEXT:              "character": 21,
CHECK-NEXT:              "line": 4
CHECK-NEXT:            },
CHECK-NEXT:            "start": {
CHECK-NEXT:              "character": 18,
CHECK-NEXT:              "line": 4
CHECK-NEXT:            }
CHECK-NEXT:          },
CHECK-NEXT:          "selectionRange": {
CHECK-NEXT:            "end": {
CHECK-NEXT:              "character": 21,
CHECK-NEXT:              "line": 4
CHECK-NEXT:            },
CHECK-NEXT:            "start": {
CHECK-NEXT:              "character": 18,
CHECK-NEXT:              "line": 4
CHECK-NEXT:            }
CHECK-NEXT:          }
CHECK-NEXT:        }
CHECK-NEXT:      ],
CHECK-NEXT:      "detail": "attribute",
CHECK-NEXT:      "kind": 8,
CHECK-NEXT:      "name": "numbers",
CHECK-NEXT:      "range": {
CHECK-NEXT:        "end": {
CHECK-NEXT:          "character": 21,
CHECK-NEXT:          "line": 4
CHECK-NEXT:        },
CHECK-NEXT:        "start": {
CHECK-NEXT:          "character": 4,
CHECK-NEXT:          "line": 4
CHECK-NEXT:        }
CHECK-NEXT:      },
CHECK-NEXT:      "selectionRange": {
CHECK-NEXT:        "end": {
CHECK-NEXT:          "character": 11,
CHECK-NEXT:          "line": 4
CHECK-NEXT:        },
CHECK-NEXT:        "start": {
CHECK-NEXT:          "character": 4,
CHECK-NEXT:          "line": 4
CHECK-NEXT:        }
CHECK-NEXT:      }
CHECK-NEXT:    },
CHECK-NEXT:    {
CHECK-NEXT:      "children": [
CHECK-NEXT:        {
CHECK-NEXT:          "detail": "string",
CHECK-NEXT:          "kind": 15,
CHECK-NEXT:          "name": "1",
CHECK-NEXT:          "range": {
CHECK-NEXT:            "end": {
CHECK-NEXT:              "character": 16,
CHECK-NEXT:              "line": 7
CHECK-NEXT:            },
CHECK-NEXT:            "start": {
CHECK-NEXT:              "character": 13,
CHECK-NEXT:              "line": 7
CHECK-NEXT:            }
CHECK-NEXT:          },
CHECK-NEXT:          "selectionRange": {
CHECK-NEXT:            "end": {
CHECK-NEXT:              "character": 16,
CHECK-NEXT:              "line": 7
CHECK-NEXT:            },
CHECK-NEXT:            "start": {
CHECK-NEXT:              "character": 13,
CHECK-NEXT:              "line": 7
CHECK-NEXT:            }
CHECK-NEXT:          }
CHECK-NEXT:        }
CHECK-NEXT:      ],
CHECK-NEXT:      "detail": "attribute",
CHECK-NEXT:      "kind": 8,
CHECK-NEXT:      "name": "string",
CHECK-NEXT:      "range": {
CHECK-NEXT:        "end": {
CHECK-NEXT:          "character": 16,
CHECK-NEXT:          "line": 7
CHECK-NEXT:        },
CHECK-NEXT:        "start": {
CHECK-NEXT:          "character": 4,
CHECK-NEXT:          "line": 7
CHECK-NEXT:        }
CHECK-NEXT:      },
CHECK-NEXT:      "selectionRange": {
CHECK-NEXT:        "end": {
CHECK-NEXT:          "character": 10,
CHECK-NEXT:          "line": 7
CHECK-NEXT:        },
CHECK-NEXT:        "start": {
CHECK-NEXT:          "character": 4,
CHECK-NEXT:          "line": 7
CHECK-NEXT:        }
CHECK-NEXT:      }
CHECK-NEXT:    },
CHECK-NEXT:    {
CHECK-NEXT:      "children": [
CHECK-NEXT:        {
CHECK-NEXT:          "detail": "string",
CHECK-NEXT:          "kind": 15,
CHECK-NEXT:          "name": "(dynamic string)",
CHECK-NEXT:          "range": {
CHECK-NEXT:            "end": {
CHECK-NEXT:              "character": 31,
CHECK-NEXT:              "line": 8
CHECK-NEXT:            },
CHECK-NEXT:            "start": {
CHECK-NEXT:              "character": 14,
CHECK-NEXT:              "line": 8
CHECK-NEXT:            }
CHECK-NEXT:          },
CHECK-NEXT:          "selectionRange": {
CHECK-NEXT:            "end": {
CHECK-NEXT:              "character": 31,
CHECK-NEXT:              "line": 8
CHECK-NEXT:            },
CHECK-NEXT:            "start": {
CHECK-NEXT:              "character": 14,
CHECK-NEXT:              "line": 8
CHECK-NEXT:            }
CHECK-NEXT:          }
CHECK-NEXT:        }
CHECK-NEXT:      ],
CHECK-NEXT:      "detail": "attribute",
CHECK-NEXT:      "kind": 8,
CHECK-NEXT:      "name": "string2",
CHECK-NEXT:      "range": {
CHECK-NEXT:        "end": {
CHECK-NEXT:          "character": 31,
CHECK-NEXT:          "line": 8
CHECK-NEXT:        },
CHECK-NEXT:        "start": {
CHECK-NEXT:          "character": 4,
CHECK-NEXT:          "line": 8
CHECK-NEXT:        }
CHECK-NEXT:      },
CHECK-NEXT:      "selectionRange": {
CHECK-NEXT:        "end": {
CHECK-NEXT:          "character": 11,
CHECK-NEXT:          "line": 8
CHECK-NEXT:        },
CHECK-NEXT:        "start": {
CHECK-NEXT:          "character": 4,
CHECK-NEXT:          "line": 8
CHECK-NEXT:        }
CHECK-NEXT:      }
CHECK-NEXT:    },
CHECK-NEXT:    {
CHECK-NEXT:      "children": [
CHECK-NEXT:        {
CHECK-NEXT:          "deprecated": true,
CHECK-NEXT:          "detail": "identifier",
CHECK-NEXT:          "kind": 13,
CHECK-NEXT:          "name": "x",
CHECK-NEXT:          "range": {
CHECK-NEXT:            "end": {
CHECK-NEXT:              "character": 17,
CHECK-NEXT:              "line": 9
CHECK-NEXT:            },
CHECK-NEXT:            "start": {
CHECK-NEXT:              "character": 16,
CHECK-NEXT:              "line": 9
CHECK-NEXT:            }
CHECK-NEXT:          },
CHECK-NEXT:          "selectionRange": {
CHECK-NEXT:            "end": {
CHECK-NEXT:              "character": 17,
CHECK-NEXT:              "line": 9
CHECK-NEXT:            },
CHECK-NEXT:            "start": {
CHECK-NEXT:              "character": 16,
CHECK-NEXT:              "line": 9
CHECK-NEXT:            }
CHECK-NEXT:          }
CHECK-NEXT:        }
CHECK-NEXT:      ],
CHECK-NEXT:      "detail": "attribute",
CHECK-NEXT:      "kind": 8,
CHECK-NEXT:      "name": "undefined",
CHECK-NEXT:      "range": {
CHECK-NEXT:        "end": {
CHECK-NEXT:          "character": 17,
CHECK-NEXT:          "line": 9
CHECK-NEXT:        },
CHECK-NEXT:        "start": {
CHECK-NEXT:          "character": 4,
CHECK-NEXT:          "line": 9
CHECK-NEXT:        }
CHECK-NEXT:      },
CHECK-NEXT:      "selectionRange": {
CHECK-NEXT:        "end": {
CHECK-NEXT:          "character": 13,
CHECK-NEXT:          "line": 9
CHECK-NEXT:        },
CHECK-NEXT:        "start": {
CHECK-NEXT:          "character": 4,
CHECK-NEXT:          "line": 9
CHECK-NEXT:        }
CHECK-NEXT:      }
CHECK-NEXT:    },
CHECK-NEXT:    {
CHECK-NEXT:      "children": [
CHECK-NEXT:        {
CHECK-NEXT:          "detail": "integer",
CHECK-NEXT:          "kind": 16,
CHECK-NEXT:          "name": "1",
CHECK-NEXT:          "range": {
CHECK-NEXT:            "end": {
CHECK-NEXT:              "character": 9,
CHECK-NEXT:              "line": 1
CHECK-NEXT:            },
CHECK-NEXT:            "start": {
CHECK-NEXT:              "character": 8,
CHECK-NEXT:              "line": 1
CHECK-NEXT:            }
CHECK-NEXT:          },
CHECK-NEXT:          "selectionRange": {
CHECK-NEXT:            "end": {
CHECK-NEXT:              "character": 9,
CHECK-NEXT:              "line": 1
CHECK-NEXT:            },
CHECK-NEXT:            "start": {
CHECK-NEXT:              "character": 8,
CHECK-NEXT:              "line": 1
CHECK-NEXT:            }
CHECK-NEXT:          }
CHECK-NEXT:        }
CHECK-NEXT:      ],
CHECK-NEXT:      "detail": "attribute",
CHECK-NEXT:      "kind": 8,
CHECK-NEXT:      "name": "x",
CHECK-NEXT:      "range": {
CHECK-NEXT:        "end": {
CHECK-NEXT:          "character": 9,
CHECK-NEXT:          "line": 1
CHECK-NEXT:        },
CHECK-NEXT:        "start": {
CHECK-NEXT:          "character": 4,
CHECK-NEXT:          "line": 1
CHECK-NEXT:        }
CHECK-NEXT:      },
CHECK-NEXT:      "selectionRange": {
CHECK-NEXT:        "end": {
CHECK-NEXT:          "character": 5,
CHECK-NEXT:          "line": 1
CHECK-NEXT:        },
CHECK-NEXT:        "start": {
CHECK-NEXT:          "character": 4,
CHECK-NEXT:          "line": 1
CHECK-NEXT:        }
CHECK-NEXT:      }
CHECK-NEXT:    }
CHECK-NEXT:  ]
```

```json
{"jsonrpc":"2.0","method":"exit"}
```
