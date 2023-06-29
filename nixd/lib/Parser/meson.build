#------------------------------------------------------------------------------#
# Parser & Lexer stuff
flex = find_program('flex')
bison = find_program('bison')

# Args are specified in:
# https://github.com/NixOS/nix/blob/a1cf16563f681b5cb3026f2bbca629996ed36d86/src/libexpr/local.mk#L31

lexer = custom_target('lexer'
, input : [ 'Lexer.l' ]
, output : [ '@BASENAME@.tab.cpp', '@BASENAME@.tab.h' ]
, command : [ flex, '--outfile=@OUTPUT0@', '--header-file=@OUTPUT1@', '@INPUT@' ]
)

parser = custom_target('parser'
, input : [ 'Parser.y' ]
, output : [ '@BASENAME@.tab.cpp', '@BASENAME@.tab.h' ]
, command : [ bison, '@INPUT@', '--defines=@OUTPUT1@', '--output=@OUTPUT0@' ]
)

#------------------------------------------------------------------------------#
libnixdParserDeps = [ nix_all ]

libnixdParser = library('nixdParser'
, lexer
, parser
, link_with: [ libnixdExpr
             ]
, include_directories: nixd_inc
, dependencies: libnixdParserDeps
, install: true
)

nixdParser = declare_dependency( include_directories: nixd_inc
                               , sources: [ lexer
                                          , parser
                                          ]
                               , link_with: libnixdParser
                               , dependencies: libnixdParserDeps
                               )
