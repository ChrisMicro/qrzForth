{
	some forth words found in the Forth standard

	Some words are from nuc.fs from the j1-Forth cpu
	http://www.excamera.com/sphinx/fpga-j1.html
	
}

: false 0 ;
: true  ( 6.2.2298 ) h# FFFF ;

\ qrz vm specific
: rot ( a b c -- b c a ) 
	>r					\ a b [r]=c
	swap				\ b a
	r>					\ b a c
	swap				\ b c a 
; 
: -rot ( a b c -- c a b ) rot rot ;
: tuck  swap over ;
: 2drop drop drop ;
: ?dup  dup if dup then ;

: split                     ( a m -- a&m a&~m )
    over                    \ a m a
    and                     \ a a&m
    tuck                    \ a&m a a&m
    xor                     \ a&m a&~m
;

: merge ( a b m -- m?b:a )
    >r          \ a b
    over xor    \ a a^b
    r> and      \ a (a^b)&m
    xor         \ ((a^b)&m)^a
;

: 2/ ( n -- n/2 ) _asr ;
: 2* ( n -- 2n ) _shl ;

\ qrz vm specific
: c@  ( c-adr -- n )  dup 2/ @ swap d# 1 and if d# 8 rshift else d# 255 and then ;
: c!  ( u c-adr )
		dup 					( u c-adr c-adr)
		2/						( u c-adr adr )
		dup						( u c-adr adr adr )
		rot						( u adr adr c-adr )
		d# 1 and 	 			( u adr adr flag )
		if						( u adr adr )
			-rot					( adr u adr )
			@						( adr u n )
			swap d# 8 lshift		( adr n hu )
		else
			-rot					( adr u adr )
			@						( adr u n )
		then
		h# FF00 merge			( adr n )
		swap					( n adr )
		!						( --  )
;
: c!be d# 1 xor c! ;
\ Stack
: 2dup  over over ;
: +!    tuck @ + swap ! ;

\ Comparisons
: < 		swap > ;
: 0<>       0= invert ;
: 0<        d# 0 < ;
: 0>=       0< invert ;
: 0> 		0 > ;
: >=        < invert ;
: <=        > invert ;

\ Arithmetic
: negate    invert 1+ ;
: -         negate + ;
: abs       dup 0< if negate then ;
: ?:        ( xt xf f -- xt | xf) if drop else nip then ;
: min       2dup < ?: ; 

: max       2dup > ?: ;

: c+!       tuck c@ + swap c! ;
\ COUNT converts a string array address to the address-length representation of a counted string.
\ : count     dup 1+ swap c@ ;
: /string   dup >r - swap r> + swap ;
: aligned   1+ h# fffe and ;

\ qrz vm specific
: pick ( nx ... n p -- nx ... n np ) 

	pregs		\  ( nx ... n1 n p adr )
	2dup		\  ( nx ... n1 n p adr p adr )
	!			\  ( nx ... n1 n p adr )		 p stored in pregs[0]
	1+			\  ( nx ... n1 n p adr+1 )
	swap		\  ( nx ... n1 n adr+1 p )
	0do			\  ( nx ... n1 n adr+1 )
		dup		\  ( nx ... n1 n adr+1 adr+1 )
		-rot	\  ( nx ... n1 adr+1 n adr+1 )
		!		\  ( nx ... n1 adr+1 )			 top value stored in pregs[m]
		1+		\  ( nx ... n1 adr+2 )
	loop		\  ( adr+2 )
	
	pregs @		\  ( adr+2 p)					 get p
	
	0do			\  ( adr+2 )
		1-		\  ( adr+1 )
		dup		\  ( adr+1 adr+1 )
		@		\  ( adr+1 nx )					 restore stack
		swap	\  ( nx adr+1 )
	loop		\  ( nx ... n1 adr+1 )
		
	1- dup 		\  ( nx ... n1 adr adr)
	@			\  ( nx ... n1 adr p)
	+			\  ( nx ... n1 adr+p)
	@			\  ( nx ... n1 nx)
;
\ qrz vm specific
: u> ( ua ub -- flag )
	2dup 						\ ( ua ub ua ub )
	0> 	if 						\ ( ua ub ua )
								\ ub>0
			0> 	if				\ ( ua ub )
								\ ub>0, ua>0
					>
				else			\ ub>0, ua<=0
					2drop
					false	
				then
		else 0> if 				\ ( ua ub )
								\ ub<=0, ua>0
					2drop
					true
				else			\ ub<=0, ua<=0
					>
				then
		then
;
: u< ( ua ub -- flag ) swap u> ;
\ ========================================================================
\ Double
\ ========================================================================

: d=                        ( a b c d -- f )
    >r                      \ a b c
    rot xor                 \ b a^c
    swap r> xor             \ a^c b^d
    or 0=
;

: 2@                        ( ptr -- lo hi )
    dup @ swap 2+ @
;

: 2!                        ( lo hi ptr -- )
    rot over                \ hi ptr lo ptr
    ! 2+ !
;

\ : 2over >r >r 2dup r> r> ;fallthru
: 2over ( ln hn a b -- ln hn a b ln hn ) 4 pick 4 pick ;

: 2swap rot >r rot r> ;
: 2nip rot drop rot drop ;



\ qrz vm specific
\ attention - dirty hack , this is a virtual return stack
: >R ( a -- )
	rpointer @	\ ( a adr )
	dup 1+		\ ( a adr adr+1 )
	rpointer !  \ ( a adr )
	!
;
: R> ( -- a )
	rpointer @			\ ( adr )
	1-					\ ( adr-1 )
	dup					\ ( adr-1 adr-1 )
	rpointer !			\ ( adr-1 )
	@					\ ( adr-2 a )
;

: R@ ( -- a )
	rpointer @
	1-
	@		
;
	
: 2>r ( a b -- ) 
	rpointer @  		\ ( a b adr )
	dup					\ ( a b adr adr )
	2+					\ ( a b adr adr+2 )
	rpointer !			\ ( a b adr )
	swap				\ ( a adr b )
	over 				\ ( a adr b adr )
	!					\ ( a adr )
	1+					\ ( a adr+1 )
	!			
;
	
: 2r> ( -- a b )
	rpointer @			\ ( adr )
	2-					\ ( adr-2 )
	dup					\ ( adr-2 adr-2 )
	rpointer !			\ ( adr-2 )
	dup 1+				\ ( adr-2 adr-1 )
	@					\ ( adr-2 a )
	swap				\ ( a adr-2 )
	@					\ ( a b )
;
\ **** end dirty hack **********************
	
: 2rot ( d1 d2 d3 -- d2 d3 d1 ) 2>r 2swap 2r> 2swap ;
: 2pick
    2* 1+ dup 1+            \  lo hi ... 2k+1 2k+2
    pick                    \  lo hi ... 2k+1 lo
    swap                    \  lo hi ... lo 2k+1
    pick                    \  lo hi ... lo hi
;
: d+                              ( augend . addend . -- sum . )
    rot + >r                      ( augend addend)
    over +                        ( augend sum)
    dup rot                       ( sum sum augend)
    u< if                         ( sum)
        r> 1+
    else
        r>
    then                          ( sum . )
;
: +h ( u1 u2 -- u1+u2/2**16 )
    over +     ( a a+b )
    u> d# 1 and
;

: +1c   \ one's complement add, as in TCP checksum
    2dup +h + +
;

: s>d dup 0< ;
: d1+ d# 1 0 d+ ;
: dnegate
    invert swap invert swap
    d1+
;
: dabs ( d -- ud )  dup 0< if dnegate then ; \ ( 8.6.1.1160 )
\ Write zero to double
: dz d# 0 dup rot 2! ;

: dxor              \ ( a b c d -- e f )
    rot xor         \ a c b^d
    -rot xor        \ b^d a^c
    swap
;

: dand      rot and -rot and swap ;
: dor       rot or  -rot or  swap ;

: dinvert  invert swap invert swap ;
: d<            \ ( al ah bl bh -- flag )
    rot         \ al bl bh ah
    2dup =
    if
        2drop u<
    else
        2nip >
    then
;

: d> 2swap d< ;
: d<= d> invert ;
: d0<= d# 0 0 d<= ;
: d>= d< invert ;
: d0= or 0= ;
: d0< d# 0 0 d< ;
: d0<> d0= invert ;
: d<> d= invert ;
: d2* 2dup d+ ;
: d2/ dup d# 15 lshift >r 2/ swap 2/ r> or swap ;
: dmax       2over 2over d< if 2swap then 2drop ;

: d1- h# ffff ffff d+ ;
: d+!                   ( v. addr -- )
    dup >r
    2@
    d+
    r>
    2!
;

: move ( addr1 addr2 u -- )
    d# 0do
        over @ over !
        2+ swap 2+ swap
    loop
    2drop
;

: cmove ( c-addr1 c-addr2 u -- )
    d# 0do
        over c@ over c!
        1+ swap 1+ swap
    loop
    2drop
;

: bounds ( a n -- a+n a ) over + swap ;

\ qrz vm specific
: fill ( c-addr u char -- ) 
	rot 		\ ( u char c-addr )
	rot 		\ ( char c-addr u )
	0do 		\ ( char c-addr )
		2dup	\ ( char c-addr char c-addr )
		c!		\ ( char c-addr )
		1+		\ ( char c-addr+1 )
	loop
	2drop 
;
\ ========================================================================
\ math
\ ========================================================================
: um*  ( u1 u2 -- ud )
    scratch !
    d# 0 0
    d# 16 0do
        2dup d+
        rot dup 0< if
            2* -rot
            scratch @ d# 0 d+
        else
            2* -rot
        then
    loop
    rot drop
;
: *         um* drop ;
: abssgn    ( a b -- |a| |b| negf )
        2dup xor 0< >r abs swap abs swap r> ;

: m*    abssgn >r um* r> if dnegate then ;
: divstep
    ( divisor dq hi )
    2*
    over 0< if 1+ then
    swap 2* swap
    rot                     ( dq hi divisor )
    2dup >= if
        tuck                ( dq divisor hi divisor )
        -
        swap                ( dq hi divisor )
        rot 1+              ( hi divisor dq )
        rot                 ( divisor dq hi )
    else
        -rot
    then
    ;
    
\ ( 6.1.2370 )
: um/mod ( ud u1 -- u2 u3 ) 
    -rot 
    divstep divstep divstep divstep
    divstep divstep divstep divstep
    divstep divstep divstep divstep
    divstep divstep divstep divstep
    rot drop swap
;

\ ( 6.1.2214 ) ( symmetric ) 
: sm/rem ( d n -- r q ) 
  over >R >R  dabs R@ abs um/mod
  R> R@ xor 0< if negate then R> 0< if >R negate R> then ;
: /mod  >r s>d r> sm/rem ;  
: mod /mod drop ;
: /     /mod nip ;
: */mod >R m* R> sm/rem ;
: */    */mod nip ;

: t2* over >r >r d2*
    r> 2* r> 0< d# 1 and + ;
    
include test.fs
