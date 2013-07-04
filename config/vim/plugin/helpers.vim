" ypsu's helper functions

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
	set textwidth=72
	set formatoptions+=tc
	set spell
	set spelllang=hu
	syntax on
endfunction

command Mailmode call Mailmode()

execute("sign define fixme text=!! linehl=StatusLine texthl=Error")

function! SignFixme()
	execute(":sign place ".line(".")." line=".line(".")." name=fixme file=".expand("%:p"))
endfunction

function! UnSignFixme()
	execute(":sign unplace ".line("."))
endfunction

function! SelectFile()
	let s:cmd = "silent !file-selector 2>/tmp/.fsel-fname -u " . v:count
	execute s:cmd
	let s:result = readfile("/tmp/.fsel-fname")
	if len(s:result) > 0
		execute "edit " . s:result[0]
	endif
	redraw!
endfunction

function! SelectBuffer()
	let s:cmd = "silent !file-selector 2>/tmp/.fsel-fname"
	let s:i = 1
	while bufexists(s:i)
		if bufloaded(s:i)
			let s:cmd .= " " . bufname(s:i)
		endif
		let s:i += 1
	endwhile
	execute s:cmd
	let s:result = readfile("/tmp/.fsel-fname")
	if len(s:result) > 0
		execute "edit " . s:result[0]
	endif
	redraw!
endfunction

function! ToggleSyntaxHighlight()
	if exists("g:syntax_on")
		syntax off
	else
		syntax enable
	endif
endfunction

function! SaveCount()
	let s:count = v:count
endfunction

function! RemoteMan(word)
	let s:cmd = "rcmd-man "
	if s:count > 0
		let s:cmd .= s:count . " "
	endif
	let s:cmd .= a:word
	let s:dummy =  system(s:cmd)
endfunction
