/*
 * platform.c
 *
 *  Created on: 30.11.2013
 *      Author: christoph
 */

#include "platform.h"
#include "qrzVm.h"
#include "systemout.h"
#include "qrzCode.h"

#define SCREENADDRESS 	0x6000

extern uint8_t Base;
uint16_t RamMemory[RAMSIZE];

char SysMessage[SYSMESSAGELENGTH]={0};
UartReceive_t UartReceiveState;

void writeMemory(Cpu_t *cpu,uint16_t wordAddress, uint16_t value)
{
	#ifdef ARDUINO_PLATFORM
	  //SYSTEMOUTHEX("memwrite: ",byteAddress);
	  if( wordAddress>=SCREENADDRESS) qrzPutChar(value&0xff);// serial print char
	  else
	  if( ( wordAddress>=RAMHEAPSTART ) && ( wordAddress<(RAMHEAPSTART+RAMSIZE) ) )
	  {
		RamMemory[wordAddress-RAMHEAPSTART]=value;
	  } else SYSTEMOUTHEX("memwrite error: ",wordAddress);
	  //if(wordAddress==SCREENADDRESS) showScreen(cpu);
	#endif

	#ifdef PC_PLATFORM
	  uint16_t * p;
	  p=(uint16_t * )Memory_u8;
	  p[wordAddress]=value;
		//cpu->memory[wordAddress]=value;
		if(wordAddress==SCREENADDRESS) showScreen(cpu);
	#endif
}

uint16_t readMemory(Cpu_t *cpu, uint16_t wordAddress)
{
	//SYSTEMOUTHEX("memread: ",wordAddress);
	uint16_t value=0;

	#ifdef ARDUINO_PLATFORM
	  if( wordAddress<RAMHEAPSTART ) value=pgm_read_word_near(programMemory+wordAddress);
	  else  if( ( wordAddress>=RAMHEAPSTART ) && ( wordAddress<(RAMHEAPSTART+RAMSIZE) ) )
	  {
		value=RamMemory[wordAddress-RAMHEAPSTART];
	  }else SYSTEMOUTHEX("memread error: ",wordAddress);
	#endif

	#ifdef PC_PLATFORM
		//value= cpu->memory[wordAddress];
		value=(uint16_t)Memory_u8[wordAddress];
		//SYSTEMOUTHEX("memread : ",value);
	#endif

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

char * getSysMessage()
{
  return SysMessage;
}


void qrzPutChar(char c)
{
  SysMessage[0]=c;
  SysMessage[1]=0;
}

void resetMessage()
{
  SysMessage[0]=0;
}
uint8_t ArduinoKeyPressed()
{
  //SYSTEMOUT("ArduinoKeyPressed?");
  //if(UartReceiveState.ArduinoKeyFlag)  SYSTEMOUTHEX("ArduinoKeyPressed",UartReceiveState.ArduinoKeyFlag);
  return UartReceiveState.ArduinoKeyFlag;
}
char ArduinoGetKey()
{
  char str[2];
  str[1]=0;

  //UartReceiveState.ArduinoKeyFlag=0;
  UartReceiveState.VmUartReceiveReadyFlag=1;
  str[0]=UartReceiveState.ArduinoKeyValue;
  //SYSTEMOUT(str);
  return UartReceiveState.ArduinoKeyValue;
}
// time measurement
uint32_t updateSysCounter() {
/*    static clock_t start;
    if(start){
    	// printf("difftime %d  ",difftime(clock(),start));
        return difftime(clock(),start) ;
    } else {
        start=clock();*/
        return 0;
    //}
 }
void cpuExternalCall(Cpu_t *cpu)
{
	int32_t command=pop(cpu);
	uint16_t value;
	//SYSTEMOUTHEX("command",command);
	//SYSTEMOUTHEX("value",value);
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
				//SYSTEMOUTPRINTNUMBER(value);
			}
			//else SYSTEMOUTPRINTHEXNUMBER(value);
		}break;
		case EXTERNAL_C_SETBASE: // set the calculation base
		{
			value=pop(cpu);
			//Base=value;
		}break;
		default: SYSTEMOUT("external command not found");break;
	}
}
