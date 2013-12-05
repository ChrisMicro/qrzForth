{
	this is just a test
}

: i r> r> dup -rot >r >r ; \ i: loop index inside for or 0do
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
\ COUNT converts a string array address to the address-length representation of a counted string.
\ get string length by searching 0 terminated string
: count ( adr -- adr len )
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
: key 3 external drop ;

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
\ store digit in pad from top address pad downwards
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

\ converts a digit to its numeric value according to the current base
: digit? ( c base -- u flag )
    >R 48 - 9 over <
    if 7 - dup 10 < or then dup R> u< ;
{
: NUMBER? ( a -- n T | a F )

  BASE @ >R  0 OVER COUNT ( a 0 b n)

  OVER C@ 36 =

  IF HEX SWAP 1 + SWAP 1 - THEN ( a 0 b' n')

  OVER C@ 45 = >R ( a 0 b n)

  SWAP R@ - SWAP R@ + ( a 0 b" n") ?DUP

  IF 1 - ( a 0 b n)

    FOR DUP >R C@ BASE @ DIGIT?

      WHILE SWAP BASE @ * +  R> 1 +

    NEXT DROP R@ ( b ?sign) IF NEGATE THEN SWAP

      ELSE R> R> ( b index) 2DROP ( digit number) 2DROP 0

      THEN DUP

  THEN R> ( n ?sign) 2DROP R> BASE ! ;

: number? ( a -- n T | a F )
    base @ >R 0 over count ( a 0 b n)
    over c@ 36 = 

    if hex swqp 1 + swap 1 - then   \ ( a 0 b' n')
    over c@ 45 = >R                 \ ( a 0 b n)
    swap R@ - swap R@ +             \ ( a 0 b" n") 
    ?dup
    if 1-                           \ ( a 0 b n)

    FOR DUP >R C@ BASE @ DIGIT?

      WHILE SWAP BASE @ * +  R> 1 +

    NEXT DROP R@ ( b ?sign) IF NEGATE THEN SWAP

      ELSE R> R> ( b index) 2DROP ( digit number) 2DROP 0

      THEN DUP

  THEN R> ( n ?sign) 2DROP R> BASE ! ;
}
1 drop
\ ************************************************************

: init rregs rpointer ! h# scrstart 1+ scrpos ! dec ;
init
: delay d# 3 0do h# fff fff * drop loop ;
: multest ( n -- ) 3 spaces dup str type space s" square:" type dup * str type cr ;
: hello s" if you see this, qrzVM is running calc square" type cr ;

: millis ( time in milli seconds ) 4 external drop ;
variable tstart
: mess millis tstart ! multest s" duration: " type millis tstart @ - str type cr ;
: doloop millis tstart ! d# 10000 0do loop s" 10000 do loop duration: " type millis tstart @ - str type cr ;
: test init hello doloop d# 0 100 0do 1+ dup mess loop ;

\ screen address access on PC makes screeen visible
: show h# 6000 @ 6000 ! ;
\ time
: tt 4 external . . ;

\ pseudo random 
variable RND                           ( seed )
: random ( -- n, a random number within 0 to 65536 )
        RND @ 31421 *                   ( RND*31421 )
        6927 +                          ( RND*31421+6926, mod 65536)
        dup RND !                       ( refresh he seed )
        ;

main test    \ set main jmp instructionTest

\ save	     \   save memory to disk

: kk 10 5 begin 1+ dup . over over = until ;
\ : DO   HERE@ >R >R  ; IMMEDIATE
\ : LOOP  R> R> 1+ 2DUP = 0BRANCH 2DROP ; IMMEDIATE
\ : WHILE   ' 0BRANCH , HERE@ 0 , ; IMMEDIATE
\ : REPEAT   ' BRANCH , HERE@ 1+ SWAP ! , ; IMMEDIATE