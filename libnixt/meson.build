nix_main = dependency('nix-main')
nix_expr = dependency('nix-expr')
nix_cmd = dependency('nix-cmd')

libnixtDeps = [ nix_expr, nix_main, nix_cmd, boost, libnixbc ]

libnixdInc = include_directories('include')

libnixt = library('nixt',
                  'lib/ASTReader.cpp',
                  'lib/Displacement.cpp',
                  'lib/HookExpr.cpp',
                  'lib/Kinds.cpp',
                  'lib/ParentMap.cpp',
                  include_directories: libnixdInc,
                  dependencies: libnixtDeps,
                  install: true
)

pkgconfig.generate(name: 'libnixt',
                   version: 'nightly',
                   description: 'nix tooling',
                   subdirs: [ 'nixt' ],
                   libraries: libnixt
)


nixt = declare_dependency(include_directories: libnixdInc,
                          dependencies: libnixtDeps,
                          link_with: libnixt
)

libnixt_test_exe = executable('test-libnixt',
                              'test/Kinds.cpp',
                              dependencies: [ gtest_main, nixt ])

test('unit/libnixt', libnixt_test_exe)
