/*
 ============================================================================
 Name        : qrzVm.h
 Author      : chris
 Version     :
 Copyright   : GPL lizense 3
               ( chris (at) roboterclub-freiburg.de )
 Description : qrz Virtual Machine definitions and instructions 
 ============================================================================
 */
#ifdef __cplusplus
extern "C"
{
#endif

#ifndef __QRZVM__
  #define __QRZVM__
  
    #include <stdint.h>
    #define CODESTARTADDRESS 		0x0002
    #define RAMHEAPSTART 			0x1800 // location, where FORTH variables are stored, at this address the VM must provide RAM
    #define DICTIONARYSTARTADDRESS 	0x2000
    
    #define RAMSIZE 0x200 // 1kb for Atmega328

    typedef uint16_t Index_t; // index into Forth Memory
    typedef Index_t Command_t;
    
    #define DATASTACKSIZE 16
    #define RETURNSTACKSIZE 32
    
    typedef struct {
    	uint16_t datasp; // data stack pointer
    	uint16_t retsp;  // return	stack pointer
    	uint16_t dataStack[DATASTACKSIZE];
    	uint16_t returnStack[RETURNSTACKSIZE];
    	uint16_t regpc;  // programm counter
    	uint16_t regadr; // adress register, used to hold address for memread/memwrite
    }Cpu_t;
    
/******************************************************************************************
 *
 * qrz virtual machine
 *
 * This is a virtual machine for small microcontrollers.
 * The virtual machine can interpret the qrz-Code
 *
 * Where it comes from:
 * The instruction format was developed in collaboration with ALE who wants implement
 * a cpu on a FPGA. I proposed Forth as a higher level language than assembler
 * because a Forth-compiler is much easier to develop than a C-compiler.
 * So he makes the FPGA cpu and I make the Forth-compiler.
 * We wanted to name our code "qwertz" ( from the german keyboard ), because we had no
 * other idea.
 * But that was to long, so we reduced it to "qrz".
 * The qrz-Code is used from qrzForth.
 *
 * p.s.: the code format is still under construction and may be changing in the
 *       next versions
 *
 *****************************************************************************************/
  // Instructions for the qrz Machine
  // developed in collaboration with Ale and his FPGA implementation
  #define CONST         0x0000	// if the highest bit is not set, a constant is pushed on stack
  #define CMPZA	  	0x00 	// 8000  CMPZ  A = A == 0
  #define PLUS1   	0x42 	// 8001  1+    A = A + 1
  #define PLUS2   	0x82 	// 8002  2+    A = A + 2
  #define CMPNZA	0x01 	// 8003  CMPNZ A = A != 0
  #define MINUS1  	0x43 	// 8004  1-    A = A - 1
  #define MINUS2  	0x83 	// 8005  2-    A = A - 2
  #define _NOT    	0x08 	// 8008  NOT   A =~ A
  #define _SHL    	0x0E 	// 800B  SHL   A = A << 1
  #define _ASR    	0x0F 	// 800C  ASR   A = A >> 1
  #define APLUSB  	0x10 	// 8010  ADD A = A + B
  #define AMINUSB 	0x11 	// 8011  SUB A = A - B
  #define AANDB   	0x12 	// 8012  AND A = A & B
  #define AORB    	0x13 	// 8013  OR  A = A | B
  #define AXORB   	0x14 	// 8014  XOR A = A ^ B
  #define AEQUALB 	0x15 	// 8015  =
  #define ANOTEQUALB 	0x16 	// 8016  !=
  #define AGREATERB  	0x17  	// 8017  >
  #define DSP2RSP 	0x20 	// 8020  >r
  #define RSP2DSP 	0x21 	// 8021  r>
  #define DUP 		0x22 	// 8022  dup
  #define DROP 		0x23	// 8023  drop
  #define SWAP 		0x24	// 8024  swap
  #define OVER		0x25	// 8025  over
  #define ROT 		0x26	// 8026  rot
  #define MEMWRITE 	0x30	// 8030  mem write
  #define MEMREAD 	0x32 	// 8032  mem read
  #define POPADR 	0x3D 	// 803D  pop address ( pop to addr register A )
  #define RTS 		0x3F 	// 803E   rts
  
  #define JMPZ 		0xA000 	// A/B000 jmpz
  #define CALL 		0xC000 	// C/D000 call
  #define JMP 		0xE000 	// E/F000 JMP
  
  #define EXTERNAL_C	0x80	// call c function outside simulator

  #define COMMANDGROUP2 	 0x8000
  #define COMMANDGROUP2MASK 0xE000
  #define COMMANDHWREG		 0xE03F
  #define COMMANDHWREAD     0x803E
  #define COMMANDHWWRITE    0x803D
  #define hwuart_txd        ((0x20	<<6)|COMMANDHWWRITE)
  #define hwuart_txf        ((0x21	<<6)|COMMANDHWREAD)
  #define hwuart_rxd	     ((0x22 <<6)|COMMANDHWREAD)
  #define hwuart_rxf	     ((0x23	<<6)|COMMANDHWREAD)

/***************************************************************************************
  Function Prototypes
***************************************************************************************/

  void simulatorReset(Cpu_t *);
  uint16_t pop(Cpu_t *);
  uint16_t popReturnStack(Cpu_t *);
  void push(Cpu_t *,uint16_t);
  void push2ReturnStack(Cpu_t *,uint16_t);
  void executeVm(Cpu_t *, Command_t);
  char * getSysMessage(void);
  void resetMessage(void);
  uint16_t readMemory(Cpu_t *, uint16_t);
  void showCpu(Cpu_t *);

#endif

#ifdef __cplusplus
}
#endif
