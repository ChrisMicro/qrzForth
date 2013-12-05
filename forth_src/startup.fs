: ! ( n adr -- ) popadr memwrite ; \ write memory
: @ ( adr -- n ) popadr memread ;  \ read memory

        variable scrpos
h# 6000 constant scrstart \ address of the screen buffer 
h# scrstart 1+ scrpos !   \ initialize pointer to screen

\ emit character to screen buffer

: emit		( n -- ) 0 external ; \ emit char on console
1 drop
{
: emit ( c -- ) 
    scrpos @ ! 
    scrpos @ 1+
    dup
    h# 6c01 > if drop scrstart then \ 
    scrpos !
;
}
: .		( n -- ) 1 external ; \ call external c function to print number on console

include basewords.fs



