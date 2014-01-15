{

	qrzForth bootloader
	
	- Receives commands from the serial line.
	- converts hex numbers and stores in memory
	- additional one character commands

		h: help
		y: dump
		g: go, run program from start address
		r: reset address to start address
		
		14.1.2014 chris
		
}


: startadr h# 1800 ;



: ! ( n adr -- ) popadr memwrite ; \ write memory
: @ ( adr -- n ) popadr memread ;  \ read memory

: emit 
		begin	hwuart_txf 0=	while repeat \ wait for transmitter empty
 		hwuart_txd ;										 \ send char
 		
 \ carriage return line feed		
: cr ( -- ) h# 0d emit 0a emit ;

: space h# 20 emit ;

\ Comparisons
: < 		swap > ;
: >=    < invert ;

\ Arithmetic
: negate    invert 1+ ;
: -         negate + ;
: 2* _shl ;
: 2/ ( n -- n/2 ) _asr ;

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


: c@  ( c-adr -- n )  dup 2/ @ swap d# 1 and if d# 8 rshift else d# 255 and then ;

: type ( adr n -- )
    d# 0do
        dup c@ emit
        1+
    loop
    drop
;

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

\ check if the input character is a digit and get the other keys
\ or return the character and false
: number? ( c -- n/c t/f )
	dup
	digit?
	if
		begin
		key digit? while
			swap 4 lshift
			or
		repeat
		0<> rot drop
	else
	  drop
		0 								\ return false
	then
;		 

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
					s" g: go" 				type cr
					s" r: adr=start" 	type cr
				cr
;



: resetadr ( adr c -- 0 c )
	s" reset address" type cr
	swap drop startadr swap
;

\ jump to code
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

: .		( n -- ) 1 external ; \ call external c function to print number on console


