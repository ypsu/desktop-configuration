" ypsu's helper functions

" This is a hack to recreate vim's tmpdir in case it was deleted.
function! RecreateTmpdir()
	call mkdir(fnamemodify(tempname(), ":p:h"), "p", 0700)
endfunction

function! ToggleHex()
	" hex mode should be considered a read-only operation
	" save values for modified and read-only for restoration later,
	" and clear the read-only flag for now
	let l:modified=&mod
	let l:oldreadonly=&readonly
	let &readonly=0
	let l:oldmodifiable=&modifiable
	let &modifiable=1
	if !exists("b:editHex") || !b:editHex
		" save old options
		let b:oldft=&ft
		let b:oldbin=&bin
		" set new options
		setlocal binary " make sure it overrides any textwidth, etc.
		let &ft="xxd"
		" set status
		let b:editHex=1
		" switch to hex editor
		%!xxd
	else
		" restore old options
		let &ft=b:oldft
		if !b:oldbin
			setlocal nobinary
		endif
		" set status
		let b:editHex=0
		" return to normal editing
		%!xxd -r
	endif
	" restore values for modified and read only state
	let &mod=l:modified
	let &readonly=l:oldreadonly
	let &modifiable=l:oldmodifiable
endfunction

command -bar Hexmode call ToggleHex()

function! Mailmode()
	set textwidth=80
	set formatoptions+=tc
	set spell
	set spelllang=en,hu
	set filetype=mail
endfunction

command Mailmode call Mailmode()

execute("sign define fixme text=!! linehl=StatusLine texthl=Error")

function! SignFixme()
	execute(":sign place ".line(".")." line=".line(".")." name=fixme file=".expand("%:p"))
endfunction

function! UnSignFixme()
	execute(":sign unplace ".line("."))
endfunction

function! ToggleLongLineMatch()
	if exists('w:long_line_match')
		silent! call matchdelete(w:long_line_match)
		unlet w:long_line_match
	else
		let l:m = matchadd('ErrorMsg', '\%>' . &textwidth . 'v.\+', -1)
		let w:long_line_match = l:m
	endif
endfunction

function! InitLongLineMatch()
	if !exists('w:llm_toggled')
		let w:llm_toggled = 1
		call ToggleLongLineMatch()
	endif
endfunction

autocmd VimEnter * call InitLongLineMatch()
autocmd WinEnter * call InitLongLineMatch()

function! SelectFile()
	let s:cmd = "silent !file_selector >/tmp/.fsel_fname -u " . v:count
	execute s:cmd
	let s:result = readfile("/tmp/.fsel_fname")
	if len(s:result) > 0
		execute "edit " . s:result[0]
	endif
	redraw!
endfunction

function! SelectFileFromCurrent()
	let s:cmd = "silent !cd %:h; file_selector >/tmp/.fsel_fname -u " . v:count
	execute s:cmd
	let s:result = readfile("/tmp/.fsel_fname")
	if len(s:result) > 0
		call RecreateTmpdir()
		let l:fullpath = expand("%:h") . "/" . s:result[0]
		let l:shortpath = fnamemodify(fullpath, ":p")
		" Remove the common path prefix.
		let l:nop = system("f=" .l:shortpath . "; echo -n ${f#$(pwd)/}")
		execute "edit " . l:nop
	endif
	redraw!
endfunction

function! SelectBuffer()
	let s:cmd = "silent !file_selector >/tmp/.fsel_fname"
	let l:i = 1
	while l:i <= bufnr('$')
		if bufloaded(l:i)
			let s:cmd .= " " . bufname(l:i)
		endif
		let l:i += 1
	endwhile
	execute s:cmd
	let s:result = readfile("/tmp/.fsel_fname")
	if len(s:result) > 0
		execute "edit " . s:result[0]
	endif
	redraw!
endfunction

function! OpenFromClipboard()
	call RecreateTmpdir()
	let l:fname = system("f=$(tmux save-buffer -); echo -n ${f#$(pwd)/}")
	execute "edit " . l:fname
endfunction

function! ToggleSyntaxHighlight()
	if exists("g:syntax_on")
		syntax off
	else
		syntax enable
	endif
	hi Visual ctermfg=4 ctermbg=0
	hi SpellBad ctermbg=1
	hi SpellCap ctermbg=4
endfunction

function! SaveCount()
	let s:count = v:count
endfunction

function! RemoteMan(word)
	call RecreateTmpdir()
	let s:cmd = "rcmd_man "
	if s:count > 0
		let s:cmd .= s:count . " "
	endif
	let s:cmd .= a:word
	call system(s:cmd)
endfunction

" Jump to the next or previous line that has the same level or a lower
" level of indentation than the current line.
"
" exclusive (bool): true: Motion is exclusive
" false: Motion is inclusive
" fwd (bool): true: Go to next line
" false: Go to previous line
" lowerlevel (bool): true: Go to line with lower indentation level
" false: Go to line with the same indentation level
" skipblanks (bool): true: Skip blank lines
" false: Don't skip blank lines
function! NextIndent(exclusive, fwd, lowerlevel, skipblanks)
	let line = line('.')
	let column = col('.')
	let lastline = line('$')
	let indent = indent(line)
	let stepvalue = a:fwd ? 1 : -1
	while (line > 0 && line <= lastline)
		let line = line + stepvalue
		if ( ! a:lowerlevel && indent(line) == indent ||
					\ a:lowerlevel && indent(line) < indent)
			if (! a:skipblanks || strlen(getline(line)) > 0)
				if (a:exclusive)
					let line = line - stepvalue
				endif
				execute('normal ' . line . 'G')
				return
			endif
		endif
	endwhile
endfunction
