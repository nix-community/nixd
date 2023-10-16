#------------------------------------------------------------------------------#
# Parser & Lexer stuff
flex = find_program('flex')
bison = find_program('bison')

# Args are specified in:
# https://github.com/NixOS/nix/blob/a1cf16563f681b5cb3026f2bbca629996ed36d86/src/libexpr/local.mk#L31

lexer = custom_target('lexer'
, input : [ 'Lexer/Lexer.l' ]
, output : [ '@BASENAME@.tab.cpp', '@BASENAME@.tab.h' ]
, command : [ flex, '--outfile=@OUTPUT0@', '--header-file=@OUTPUT1@', '@INPUT@' ]
)

parser = custom_target('parser'
, input : [ 'Parser/Parser.y' ]
, output : [ '@BASENAME@.tab.cpp', '@BASENAME@.tab.h' ]
, command : [ bison, '@INPUT@', '--defines=@OUTPUT1@', '--output=@OUTPUT0@' ]
)

#------------------------------------------------------------------------------#
libnixdSyntaxDeps = [ nix_all, nixdBasic, nixdExpr, nixdSupport ]
libnixdSyntaxInc = [ nixd_inc, './' ]
libnixdSyntax = library('nixdSyntax'
, lexer
, parser
, include_directories: libnixdSyntaxInc
, dependencies: libnixdSyntaxDeps
, install: true
)

nixdSyntax = declare_dependency( include_directories: libnixdSyntaxInc
                               , sources: [ lexer
                                          , parser
                                          ]
                               , link_with: libnixdSyntax
                               , dependencies: libnixdSyntaxDeps
                               )
