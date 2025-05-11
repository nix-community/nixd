libnixt_deps = [ boost ]

if get_option('nix_impl') == 'nix'
    libnixt_deps += [
        dependency('nix-main'),
        dependency('nix-expr'),
        dependency('nix-cmd'),
        dependency('nix-flake'),
    ]
else
    libnixt_deps += [
        dependency('lix-main'),
        dependency('lix-expr'),
        dependency('lix-cmd'),
    ]
endif



libnixd_inc = include_directories('../include')

libnixt = library('nixt',
                  'Value.cpp',
                  include_directories: libnixd_inc,
                  dependencies: libnixt_deps,
                  install: true
)

pkgconfig.generate(name: 'nixt',
                   version: 'nightly',
                   description: 'nix compatible layer',
                   subdirs: [ 'nixt' ],
                   libraries: libnixt,
)


nixt = declare_dependency(include_directories: libnixd_inc,
                          dependencies: libnixt_deps,
                          link_with: libnixt
)

install_subdir('../include/nixt', install_dir: 'include')
