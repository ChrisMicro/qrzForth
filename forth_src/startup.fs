{

	qrzForth bootloader
	
	.... work in progress ....
	
}

: ! ( n adr -- ) popadr memwrite ; \ write memory
: @ ( adr -- n ) popadr memread ;  \ read memory

: emit 
		begin	hwuart_txf 0=	while repeat \ wait for transmitter empty
 		hwuart_txd ;										 \ send char
 		
 \ carriage return line feed		
: cr ( -- ) h# 0d emit 0a emit ;

\ Comparisons
: < 		swap > ;
: >=    < invert ;

\ Arithmetic
: negate    invert 1+ ;
: -         negate + ;
: 2* _shl ;

: rshift ( x1 u -- x2 ) 0do _asr loop ; \ shifts bits of x1 u places to the left, putting zeros in the empty places on the right.
: lshift ( x1 u -- x2 ) 0do _shl loop ; \ shifts bits of x1 u places to the right, putting zeroes in the into empty places on the left.

: hex1 d# 15 and dup d# 10 < if d# 48 else d# 55 then + emit ;
: hex2
    dup 
    d# 4 rshift
    hex1 hex1 ;

: hex4
    dup
    d# 8 rshift
    hex2 hex2 ;
    	
\ check receiver flag
: key? ( -- flag ) hwuart_rxf 0<> ;  \ check receiver flag

\ get key
: key ( -- c ) 
			begin key? 0= while repeat \ wait for receiver
			hwuart_rxd ; 							 \ get received char

: 2/ ( n -- n/2 ) _asr ;

: c@  ( c-adr -- n )  dup 2/ @ swap d# 1 and if d# 8 rshift else d# 255 and then ;

: type ( adr n -- )
    d# 0do
        dup c@ emit
        1+
    loop
    drop
;

: test 10 begin	dup 
					0<>	while 
								dup hex4 cr
								1-
					repeat ;

: test2 begin -1 while \ endless loop
					begin key [char] a <> while [char] _ emit repeat 
					test 
					repeat ;
					
: hallo s" hallo" type cr ;
					
: test3 	1 hex1 2 hex1 3 hex1 cr 
					hallo
					begin -1 while \ endless loop
						key dup hex4 cr
						dup [char] 1 = if s" eins" type then
						dup [char] 2 = if s" zwei" type then						
						drop
					repeat ;	

: .		( n -- ) 1 external ; \ call external c function to print number on console


\ convert character to hex value
: digit? ( c -- n f ) 
	[char] 0 - 
	dup d# 9 > 							
	if											\ if no digit, it could be A,B ... 
		7 - 
		dup h# f > 
			if d# 32 - then     \ if not A,B .. it could be a,b, ..			
	then
	dup h# fff0 and 0= 			\ check if some upper bits are set
;

: keydig
	key digit? ( n f )
;

: num
	0
	begin
		keydig while
			swap 4 lshift
			or
		repeat
;
\ check if the input character is a digit and get the other keys
\ or return the character and false
: number? ( c -- n/c t/f )
	dup
	digit?
	if
		begin
		keydig while
			swap 4 lshift
			or
		repeat
		0<> rot drop
	else
	  drop
		0 								\ return false
	then
;		 


: space h# 20 emit ;

\ dump words
: dump16 ( addr c - adr c)
    swap
	    dup 2* hex4 [char] : emit space 		\ show address
	    
	    d# 8 																\ 8 columns
	    
	    0do
	        dup @ hex4 space
	        1+
	    loop
	    cr
    swap
;

\ show help
: help 	cr
					s" h: help" 			type cr
					s" y: dump" 			type cr
					s" g:go   " 			type cr
					s" r: adr=start" 	type cr
				cr
;

: startadr h# 800 ;

: resetadr ( adr c -- 0 c )
	s" reset address" type cr
	swap drop startadr swap
;
: runcode startadr 2* >r ;
	 				
: bootloader 	
					s" bootloader" type cr 
					help																		\ show help
					startadr 																\ set start adr

					begin -1 while 													\ endless loop
					
							key 			( adr c ) 
							number? 	( adr n/c t/f )
							if 				( adr n ) 						
	
									over ! 															\ store value in address
									1+ 																	\ increment word address
							
							else 			( adr c )
							
									dup 	[char] y = if dump16	 	then
									dup 	[char] r = if resetadr 	then
									dup 	[char] h = if help 			then		
									dup 	[char] g = if runcode		then				
									drop
							
							then

					repeat 
;				
				
\ set reset vector to bootloader
main bootloader

\ save code
save



