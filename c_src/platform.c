/*
 * platform.c
 *
 *  Created on: 30.11.2013
 *      Author: christoph
 */

#include "platform.h"
#include "qrzVm.h"
#include "systemout.h"
#include <time.h>

#define SCREENADDRESS 	0x6000

extern uint8_t Base;
uint16_t RamMemory[RAMSIZE];
uint8_t Memory_u8[STSIZE];
uint8_t Memory_bigEndian_u8[STSIZE]; // big endian memory for other processors than PC

void writeMemory(Cpu_t *cpu,uint16_t wordAddress, uint16_t value)
{
	  uint16_t * p;
	  p=(uint16_t * )Memory_u8;
	  p[wordAddress]=value;
	  if(wordAddress==SCREENADDRESS) showScreen(cpu);
}

uint16_t readMemory(Cpu_t *cpu, uint16_t wordAddress)
{
	uint16_t value=0;

	value=(uint16_t)Memory_u8[wordAddress*2];
	value+=(uint16_t)(Memory_u8[wordAddress*2+1]<<8);

	return value;
}

// tbd: move display memory outside the cpu
#define COLUMNS 		100
#define ROWS			10
#define SCREENADDRESS 	0x6000

void showScreen(Cpu_t *cpu)
{
	/*
	uint16_t col,row;
	uint16_t adr=SCREENADDRESS;
	for(row=0;row<ROWS;row++)
	{
		for(col=0;col<COLUMNS;col++)
		{
			char c= cpu->memory[adr++];
			if(c>' ') { SYSTEMOUTCHAR(c); }
			else { SYSTEMOUTCHAR(' '); }
		}
		SYSTEMOUTCR;
	}
	*/
}


void qrzPutChar(char c)
{
	#ifdef ARDUINO_PLATFORM
		SysMessage[0]=c;
		SysMessage[1]=0;
	#endif
}
void resetMessage()
{
	#ifdef ARDUINO_PLATFORM
    	SysMessage[0]=0;
	#endif
}
// time measurement
uint32_t updateSysCounter() {
    static clock_t start;
    if(start){
    	// printf("difftime %d  ",difftime(clock(),start));
        return difftime(clock(),start) ;
    } else {
        start=clock();
        return 0;
    }
 }
void cpuExternalCall(Cpu_t *cpu)
{
	int32_t command=pop(cpu);
	uint16_t value;

	switch(command)
	{
		case EXTERNAL_C_GETMSTIME:
		{
			push(cpu,updateSysCounter()&0xFFFF); // low counts
			push(cpu,updateSysCounter()>>16); // high counts
		}break;

		case EXTERNAL_C_GETKEY:
		{
			push(cpu,SYSTEMGETCHAR());
			push(cpu,0xFFFF); // true flag for char received
		}break;

		case EXTERNAL_C_PRINTCHAR:
		{
			value=pop(cpu);
			SYSTEMOUTCHAR(value);
		}break;
		case EXTERNAL_C_PRINTNUMBER:
		{
			value=pop(cpu);
			if(Base==10)
			{
				value=(value>0x8000L)? value-(int32_t) 0x10000L:value;
				SYSTEMOUTPRINTNUMBER(value);
			}
			else SYSTEMOUTPRINTHEXNUMBER(value);
		}break;
		case EXTERNAL_C_SETBASE: // set the calculation base
		{
			value=pop(cpu);
			Base=value;
		}break;
		default: SYSTEMOUT("external command not found");break;
	}
}
