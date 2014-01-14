/****************************************************************************
 *
 * qrz Virtual Machine
 *
 ****************************************************************************/
/*
 ============================================================================
 Name        : qrzVm.c
 Author      : chris
 Version     :
 Copyright   : GPL lizense 3
               ( chris (at) roboterclub-freiburg.de )
 Description : qrz Virtual Machine in C, Ansi-style
 ============================================================================
 */

#include "qrzVm.h"
#include "platform.h"
#include "systemout.h"

//extern uint16_t RamMemory[];

void simulatorReset(Cpu_t *cpu)
{
  cpu->datasp=0;
  cpu->retsp=0;
  cpu->regpc=0;
  cpu->regadr=0;
}

void showCpu(Cpu_t *cpu)
{
  SYSTEMOUTHEX("regpc: ",cpu->regpc);
/*	
  SYSTEMOUTHEX("datasp: ",cpu->datasp);
  SYSTEMOUTHEX("retsp: ",cpu->retsp);
  SYSTEMOUTHEX("regadr: ",cpu->regadr);
  SYSTEMOUT("");
*/
}

void push(Cpu_t *cpu,uint16_t value)
{
	if((cpu->datasp)<DATASTACKSIZE) cpu->dataStack[cpu->datasp++]=value; // push constant
	else
	{
		SYSTEMOUT("data stack overflow");
		SYSTEMOUT("reset vm");
		simulatorReset(cpu);
	}
}

void push2ReturnStack(Cpu_t *cpu,uint16_t value)
{
	if((cpu->retsp)<(RETURNSTACKSIZE)) cpu->returnStack[cpu->retsp++]=value; // push constant
	else
	{
		SYSTEMOUT("return stack overflow");
		SYSTEMOUT("reset vm");
		simulatorReset(cpu);
	}
}

uint16_t pop(Cpu_t *cpu)
{
	uint16_t value=0;
	if((cpu->datasp)>0) value=cpu->dataStack[--(cpu->datasp)];
	else
	{
		SYSTEMOUT("data stack empty");
		SYSTEMOUT("reset vm");
		simulatorReset(cpu);
	}
	return value;
}

uint16_t popReturnStack(Cpu_t *cpu)
{
	uint16_t value=0;
	if((cpu->retsp)>0) value=cpu->returnStack[--(cpu->retsp)];
	else
	{
		SYSTEMOUT("return stack empty");
		SYSTEMOUT("reset vm");
		simulatorReset(cpu);
	}
	return value;
}

// here is the qrzVm core
void executeVm(Cpu_t *cpu, Command_t command)
{
#ifdef DEBUGCPU
	showCpu(cpu);
#endif
	//CPU_DEBUGOUTHEX(" code",command);
        // SYSTEMOUTHEX(" code",command);

#ifdef DEBUGCPU
	CPU_DEBUGOUT_(" forth:");
	//disassmbleWord(command);
	CPU_DEBUGOUT_(" #define:");
#endif
	if((command & 0x8000)==0)
	{
		CPU_DEBUGOUT("push number");
		push(cpu,command&0x7FFF);
		cpu->regpc+=2; // regpc addresses bytes
	}
	else if ((command&COMMANDGROUP2MASK)==CALL)
	{
		CPU_DEBUGOUT("call");
		cpu->regpc+=2; // regpc addresses bytes
		push2ReturnStack(cpu,cpu->regpc);
		cpu->regpc=command&~COMMANDGROUP2MASK; // use lower bits as destination address
	}
	else if ((command&COMMANDGROUP2MASK)==JMP)
	{
		CPU_DEBUGOUT("jmp");
		//SYSTEMOUT("jmp");
		CPU_DEBUGOUTHEX("com",command);
		cpu->regpc=command&~COMMANDGROUP2MASK; // use lower bits as destination address
	}
	else if ((command&COMMANDGROUP2MASK)==JMPZ)
	{
		CPU_DEBUGOUT("JMPZ");
		if(pop(cpu)==0)
		{
			cpu->regpc=command&~COMMANDGROUP2MASK; // use lower bits as destination address
		}else cpu->regpc+=2;
	}
	else if((command & COMMANDHWREG) == COMMANDHWREAD)
	{
		switch (command)
		{
			case hwuart_txf: {push(cpu,1);}break;

			case hwuart_rxf: {push(cpu, (uint8_t) SYSTEMKEYPRESSED());}break;
            case hwuart_rxd: {push(cpu, (int8_t ) SYSTEMGETKEY());}break;
            default: SYSTEMOUTHEX("error - COMMANDHWREAD:",command);
		}
		cpu->regpc+=2;
	}
	else if((command & COMMANDHWREG) == COMMANDHWWRITE)
		{

			switch (command)
			{
			    // 803D  pop address
                case (POPADR+COMMANDGROUP2):{ CPU_DEBUGOUT("popadr");
                    cpu->regadr=pop(cpu);
                }break;
				case hwuart_txd: {
					SYSTEMOUTCHAR(pop(cpu));
				} break;
	            default: SYSTEMOUTHEX("error - COMMANDHWWRITE:",command);
			}
			cpu->regpc+=2;
		}
		else if((command & COMMANDGROUP2MASK )== COMMANDGROUP2 )
			{
				uint8_t cmd=command&~COMMANDGROUP2MASK;

				switch(cmd){

					case RTS:{
						CPU_DEBUGOUT("rts");
						cpu->regpc=popReturnStack(cpu);
						cpu->regpc-=2;
					}break;

					// 8000  CMPZ  A = A == 0
					case CMPZA:{
						CPU_DEBUGOUT("CMPZA");
						uint16_t val=(pop(cpu)==0)? 0xFFFF : 0;
						push(cpu,val);
					}break;

					// 8001  1+    A = A + 1
					case PLUS1:{
						CPU_DEBUGOUT("PLUS1");
						push(cpu,pop(cpu)+1);
					}break;

					// 8002  2+    A = A + 2
					case PLUS2:{
						CPU_DEBUGOUT("PLUS2");
						push(cpu,pop(cpu)+2);
					}break;

					// 8003  CMPNZ A = A != 0
					case CMPNZA:{
						CPU_DEBUGOUT("CMPNZA");
						uint16_t val=(pop(cpu)!=0)? 0xFFFF : 0;
						push(cpu,val);
					}break;

					// 8004  1-    A = A - 1
					case MINUS1:{
						CPU_DEBUGOUT("MINUS1");
						push(cpu,pop(cpu)-1);
					}break;

					// 8005  2-    A = A - 2
					case MINUS2:{
						CPU_DEBUGOUT("MINUS2");
						push(cpu,pop(cpu)-2);
					}break;

					// 8008  NOT   A =~ A
					case _NOT:{
						CPU_DEBUGOUT("_NOT");
						push(cpu,~pop(cpu));

					}break;
					case _SHL:{
						CPU_DEBUGOUT("_SHL");
						push(cpu,pop(cpu)<<1);
					}break;

					case _ASR:{	CPU_DEBUGOUT("_ASR");
						push(cpu,pop(cpu)>>1);
					}break;

					// 8010  ADD A = A + B
					case APLUSB:{
						CPU_DEBUGOUT("APLUSB");
						push(cpu,pop(cpu)+pop(cpu));
					}break;

					// 8011  SUB A = A - B
					case AMINUSB:{
						CPU_DEBUGOUT("AMINUSB");
						// attention: this function may be implemented inverted in FPGA
						int16_t b=pop(cpu);
						int16_t a=pop(cpu);
						push(cpu,b-a);
					}break;

					// 8012  AND A = A & B
					case AANDB:{
						CPU_DEBUGOUT("AANDB");
						push(cpu,pop(cpu)&pop(cpu));
					}break;

					// 8013  OR  A = A | B
					case AORB:{
						CPU_DEBUGOUT("AORB"); push(cpu,pop(cpu)|pop(cpu));
					}break;

					// 8014  XOR A = A ^ B
					case AXORB:{
						CPU_DEBUGOUT("AXORB"); push(cpu,pop(cpu)^pop(cpu));
					}break;

					// 8015  =
					case AEQUALB:{
						CPU_DEBUGOUT("AEQUALB");
						uint16_t val=(pop(cpu)==pop(cpu))? 0xFFFF:0 ;
						push(cpu,val);
					}break;

					// 8016  !=
					case ANOTEQUALB:{
						CPU_DEBUGOUT("ANOTEQUALB");
						uint16_t val=(pop(cpu)!=pop(cpu))? 0xFFFF:0 ;
						push(cpu,val);
					}break;

					// 8017  > ( signed b > signed a )
					case AGREATERB:{
						CPU_DEBUGOUT("AGREATERB");
						uint16_t val=((int16_t)pop(cpu)<(int16_t)pop(cpu))? 0xFFFF:0 ;
						push(cpu,val);
					}break;

					// 8020  >r , push value to return stack
					case DSP2RSP:
					{
						CPU_DEBUGOUT("dsp2rsp");
						push2ReturnStack(cpu,pop(cpu));
					}
					break;

					// 8021 r> , pop value from return stack an push on data stack
					case RSP2DSP:{ CPU_DEBUGOUT("rsp2dsp");
						push(cpu,popReturnStack(cpu));
					}break;

					// 8022  dup
					case DUP:{ CPU_DEBUGOUT("DUP");
						uint16_t val=pop(cpu);
						push(cpu,val);push(cpu,val);
					}break;

					// 8023  drop
					case DROP:{ CPU_DEBUGOUT("DROP");
						pop(cpu);
					}break;
					// 8024  swap
					case SWAP:{ CPU_DEBUGOUT("SWAP");
						uint16_t x1=pop(cpu);
						uint16_t x2=pop(cpu);
						push(cpu,x1);
						push(cpu,x2);
					}break;
					// 8025  over
					case OVER:{ CPU_DEBUGOUT("OVER");
						uint16_t x1=pop(cpu);
						uint16_t x2=pop(cpu);
						push(cpu,x2);
						push(cpu,x1);
						push(cpu,x2);
					}break;
					// 8026  rot ( a b c -- b c a )
					case ROT:{ CPU_DEBUGOUT("ROT");
						uint16_t x1=pop(cpu);
						uint16_t x2=pop(cpu);
						uint16_t x3=pop(cpu);
						push(cpu,x2);
						push(cpu,x1);
						push(cpu,x3);
					}break;
					// 8030  mem write
					// pop value from stack and write to memory location [regadr]
					case MEMWRITE:{ CPU_DEBUGOUT("memwr");
						writeMemory(cpu,cpu->regadr,pop(cpu));
					}break;

					// 8032  mem read
					case MEMREAD:{CPU_DEBUGOUT("memrd");
						push(cpu,readMemory(cpu,cpu->regadr));
						//push(cpu,cpu->memory[(cpu->regadr)]);
					}break;

					// this funktion needs 2 data stack elements
					case EXTERNAL_C:{ CPU_DEBUGOUT("external c");
						cpuExternalCall(cpu);
					}break;
					default: SYSTEMOUTHEX("error - vm code:",command);
				}
		cpu->regpc+=2; // regpc addresses bytes
	}

}

