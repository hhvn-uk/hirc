" Vim syntax file
" Language:	hirc configuration
" Maintainer:	hhvn <dev@hhvn.uk>
" Last Change:	2022-02-08
" License:	This file is placed in the public domain.

if exists("b:current_syntax")
	finish
endif

syn match hircComment "^[^/].*"
syn region hircFormatA start="^/format" end=/$/ contains=hircStyle,hircVariable,hircNickStyle
syn region hircFormatB start="^/set format." end=/$/ contains=hircStyle,hircVariable,hircNickStyle
syn match hircStyle "%{[^}]*}" contained
syn region hircNickStyle matchgroup=hircStyle start='%{nick:' end='}' contains=hircStyle,hircVariable
syn match hircVariable "\${[^}]*}" contained

hi link hircComment comment
hi link hircStyle preproc
hi link hircVariable variable
