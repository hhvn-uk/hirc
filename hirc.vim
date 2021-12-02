" hirc config syntax highlighting
" Colours comments and formats.
" I haven't done any autocmd stuff,
" so you'll likely want to tell vim
" to use this syntax (:help modeline) */

if exists("b:current_syntax")
	finish
endif

syn match hircComment "^[^/].*"
syn region hircFormatA start="^/format" end=/$/ contains=hircStyle,hircVariable,hircNickStyle
syn region hircFormatB start="^/set format." end=/$/ contains=hircStyle,hircVariable,hircNickStyle
syn match hircStyle "%{[^}]*}" contained
" hircNickStyle is a bit hacky and should be replaced 
" by a region, but I don't know syn enough for that
syn match hircNickStyle "%{nick:\${[^}]*}}" contained contains=hircVariable
syn match hircVariable "\${[^}]*}" contained

hi link hircComment comment
hi link hircStyle preproc
hi link hircNickStyle hircStyle
hi link hircVariable variable
