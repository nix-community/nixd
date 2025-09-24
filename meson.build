project('nixd', ['c', 'cpp'], default_options : ['cpp_std=gnu++20'], version : 'nightly')

# Wire up the component projects via subproject fallbacks so the entire tree can
# still be built from the repository root.
nixd_common = dependency('nixd-common', fallback : ['common', 'nixd_common'])
nixf = dependency('nixf', fallback : ['libnixf', 'nixf'])
nixt = dependency('nixt', fallback : ['libnixt', 'nixt'])

# Configure the language server itself. This instantiates the standalone nixd
# project without duplicating library builds thanks to the overrides above.
subproject('nixd')

# Keep references alive to avoid "unused variable" warnings in Meson.
_ = [nixd_common, nixf, nixt]
