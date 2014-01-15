/*
 ============================================================================
 Name        : systemout.h
 Author      : chris
 Version     :
 Copyright   : GPL lizense 3
               ( chris (at) roboterclub-freiburg.de )
 Description : wrapper macros for message and debugging outputs
 ============================================================================
 */
#ifdef __cplusplus
extern "C"
{
#endif

#ifndef __SYSTEMOUT__
  #define __SYSTEMOUT__
	#include "platform.h"

	// #define DEBUGCPU
	#ifdef ARDUINO_PLATFORM
		#define SYSMESSAGELENGTH 32
		char SysMessage[SYSMESSAGELENGTH]={0};
		// because it is not possible to access Serial.print from Arduino
		// outside the loop function, we copy the messages in a buffer
		// and write the buffer in the loop to the serial port
		#define SYSTEMOUT(text) { strcpy(SysMessage,text); };
		#define SYSTEMOUT_(text) { strcpy(SysMessage,text); };
		#define SYSTEMOUTHEX(text,value) { sprintf(SysMessage,"%s %04x ",text, value);};
		#define SYSTEMOUTCR {puts("");};

		#ifdef DEBUGCPU
		#define CPU_DEBUGOUT(text)           SYSTEMOUT(text)
		#define CPU_DEBUGOUT_(text)          SYSTEMOUT_(text)
		#define CPU_DEBUGOUTHEX(text,value)  SYSTEMOUTHEX(text,value)
		#else
		#define CPU_DEBUGOUT(text)
		#define CPU_DEBUGOUT_(text)
		#define CPU_DEBUGOUTHEX(text,value)
		#endif
	#endif
	#ifdef PC_PLATFORM
		#ifdef DEBUGCPU
			#define CPU_DEBUGOUT(text) { puts(text);};
			#define CPU_DEBUGOUT_(text) { printf(text);};
			#define CPU_DEBUGOUTHEX(text,value) { printf(text);printf(" %x\n",value);};
		#else
			#define CPU_DEBUGOUT(text)
			#define CPU_DEBUGOUT_(text)
			#define CPU_DEBUGOUTHEX(text,value)
		#endif

		#define SYSTEMGETCHAR() getchar()
        #define SYSTEMKEYPRESSED() 1 // on a PC we can not ask for dynamic keys
        #define SYSTEMGETKEY() getchar()
		#define SYSTEMOUT(text) { puts(text);};
		#define SYSTEMOUT_(text) { printf("%s",text);};
		#define SYSTEMOUTCR {puts("");};
		#define SYSTEMOUTHEX(text,value) { printf("%s",text);printf("%04x ",value);};
		#define SYSTEMOUTHEX2(text,value) { printf(text);printf("%02x ",value);};
		#define SYSTEMOUTDEC(text,value) { printf(text);printf("%d ",value);};
		#define SYSTEMOUTCHAR(value) { putchar(value);};
		#define SYSTEMOUTPRINTNUMBER(value) { printf(" %d ",value);};
		#define SYSTEMOUTPRINTHEXNUMBER(value) { printf(" %4x ",value);};
	#endif


#endif // __SYSTEMOUT__

#ifdef __cplusplus
}
#endif

