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

\ set reset vector to test3
main test3

\ save code
save



