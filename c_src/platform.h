/*
 * platform.h
 *
 *  Created on: 30.11.2013
 *      Author: christoph
 */

#ifndef PLATFORM_H_
#define PLATFORM_H_

	#define PC_PLATFORM

	#include <stdio.h>
	#include <stdint.h>
	#include "qrzVm.h"

	#ifdef PC_PLATFORM

		#define STSIZE 0x10000

	#endif

	#define EXTERNAL_C_PRINTCHAR 	0
	#define EXTERNAL_C_PRINTNUMBER 	1
	#define EXTERNAL_C_SETBASE 		2
	#define EXTERNAL_C_GETKEY		3
	#define EXTERNAL_C_GETMSTIME	4

	/***************************************************************************************
	  Function Prototypes
	***************************************************************************************/
	void writeMemory(Cpu_t *,uint16_t, uint16_t );
	uint16_t readMemory(Cpu_t *, uint16_t );
	void cpuExternalCall(Cpu_t *);
	void showScreen(Cpu_t *);

#endif /* PLATFORM_H_ */
