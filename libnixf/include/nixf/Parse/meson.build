# Generated here, so that it lands in the build tree at nixf/Parse/Tokens.h: the
# path it is installed at, and the one public headers include.
python3 = find_program('python3')

tokens_h = custom_target(
    input: ['../../../src/Parse/Tokens.h.py', '../../../src/Parse/tokens.py'],
    output: 'Tokens.h',
    command: [python3, '@INPUT0@', '@OUTPUT@'],
    install: true,
    install_dir: 'include/nixf/Parse/'
)
