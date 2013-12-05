{
	this is just a test
}

: cr ( -- ) h# 0d emit 0a emit ;
: space  ( -- )		d# 32 emit ; \ write space to console

: spaces ( n -- ) 	begin space 1- dup 0= until drop ;

\ from http://www.excamera.com/sphinx/fpga-j1.html

: hex1 d# 15 and dup d# 10 < if d# 48 else d# 55 then + emit ;
: hex2
    dup 
    d# 4 rshift
    hex1 hex1
;
: hex4
    dup
    d# 8 rshift
    hex2 hex2 ;

: hex8 hex4 hex4 ;

: type ( adr n -- )
    d# 0do
        dup c@ emit
        1+
    loop
    drop
;
: dump
    ( addr u )
    0do
        dup d# 15 and 0= if dup cr hex4 [char] : emit space space then
        dup c@ hex2 space 1+
    loop
    cr drop
;
: dump16
    ( addr u )
    cr
    0do
        dup 2* hex4 [char] : emit space dup @ hex4 cr 1+
    loop
    drop
;
\ end 
\ from http://www.excamera.com/sphinx/fpga-j1.html

\ get string length by searching 0 terminated string
: strlen ( adr -- adr len )
    dup
    -1              \ ( adr adr -1 ) init len=-1   
    begin
        swap dup    \ ( adr len adr adr )      
        c@          \ ( adr len adr val )
        0=          \ ( adr len adr flag )
        -rot        \ ( adr flag len adr )
        1+          \ ( adr flag len adr+1 )          
        swap        \ ( adr flag ad+1 len )
        1+          \ ( adr flag adr+1 len+1 )
        rot         \ ( adr adr+1 len+1 flag )
    until           \ ( adr adr+1 len+1 )
    nip             \ ( adr len )
;

variable base

\ ************************************************************
\
\ number conversion
\
\ from eForth Overview
\ http://www.offete.com/files/zeneForth.htm
\
\ modified for c! with byte address and ! with word address
\
\ ************************************************************
variable hld 32 allot       \ text pointer
variable pad                \ text buffer

\ number to ascii digit
: digit ( u -- c ) 9 over < 7 and + 48 + ;
\ convert digit relating to base
: extract ( n base -- n c ) 0 swap um/mod swap digit ;
: <# ( -- ) pad 2* hld ! ;
\ store digit in pad
: hold ( c -- ) hld @ 1- dup hld ! c! ;
\ convert one digit and store in pad
: # ( u -- u ) base @ extract hold ;
\ convert all digits
: #s ( u -- 0 ) begin # dup 0= until ;
\ add sign
: sign ( n -- ) 0< if 45 hold then ;
\ calculate string length
: #> ( w -- b u ) drop hld @ pad 2* over - ;
\ convert a signed integer to a numeric string
: str ( n -- b u ) dup >R abs <# #s R> sign #> ;
: hex ( -- ) d# 16 base ! ;
: dec ( -- ) d# 10 base ! ;
\ ************************************************************

: init rregs rpointer ! h# scrstart 1+ scrpos ! dec ;
init
: delay d# 3 0do h# fff fff * drop loop ;
: multest ( n -- ) dup str type space s" square:" type dup * str type cr ;
: hello s" if you see this, qrzVM is running calc square" type cr ;
: test init hello d# 0 100 0do 1+ dup multest loop ;

\ screen address access on PC makes screeen visible
: show h# 6000 @ 6000 ! ;

main test    \ set main jmp instructionTest

\ save	     \   save memory to disk

