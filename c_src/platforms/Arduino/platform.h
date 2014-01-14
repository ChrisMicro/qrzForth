/*
 * platform.h
 *
 *  Created on: 30.11.2013
 *      Author: christoph
 */

#ifndef PLATFORM_H_
#define PLATFORM_H_

	//#define PC_PLATFORM
	#define ARDUINO_PLATFORM

	#include <stdio.h>
	#include <stdint.h>
	#include "qrzVm.h"
	#include <avr/pgmspace.h>

	#ifdef PC_PLATFORM

		#define STSIZE 0x10000

	#endif
	#ifdef ARDUINO_PLATFORM

		#define STSIZE 0x400

	#endif

	#define EXTERNAL_C_PRINTCHAR 	0
	#define EXTERNAL_C_PRINTNUMBER 	1
	#define EXTERNAL_C_SETBASE 		2
	#define EXTERNAL_C_GETKEY		3
	#define EXTERNAL_C_GETMSTIME	4

	//extern PROGMEM uint16_t programMemory[] ;
	extern char SysMessage[];

	typedef struct{
	    uint8_t ArduinoKeyFlag;
	    char ArduinoKeyValue;
	    uint8_t VmUartReceiveReadyFlag;
	}UartReceive_t;
	extern UartReceive_t UartReceiveState;

	/***************************************************************************************
	  Function Prototypes
	***************************************************************************************/
	void writeMemory(Cpu_t *,uint16_t, uint16_t );
	uint16_t readMemory(Cpu_t *, uint16_t );
	void cpuExternalCall(Cpu_t *);
	void showScreen(Cpu_t *);
	void qrzPutChar(char c);
	char * getSysMessage();

#endif /* PLATFORM_H_ */
