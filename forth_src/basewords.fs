\ system variables
\ we need some helper registers due to the restriction of the VM

variable reg0
variable reg1
variable pregs 32 allot
variable scratch
variable rpointer 
variable rregs 16 allot

rregs rpointer ! \ attention: on stand alone controllers, ad this to init

\ : swap	( a b -- b a ) reg0 ! reg1 ! reg0 @ reg1 @ ;  
: - ( x y -- x-y ) swap _aminusb ;
: over	( a b - a b a ) reg0 ! dup reg0 @ swap ;
: nip	( a b - b ) swap drop ;

\ : 2dup	( a b -- a b a b ) over over ;
: 2drop ( a b -- ) drop drop ;

: rshift ( x1 u -- x2 ) 0do _asr loop ; \ shifts bits of x1 u places to the left, putting zeros in the empty places on the right.
: lshift ( x1 u -- x2 ) 0do _shl loop ; \ shifts bits of x1 u places to the right, putting zeroes in the into empty places on the left.

include nucleus.fs
