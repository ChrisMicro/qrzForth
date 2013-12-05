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

  // #define DEBUGCPU

  #define SYSMESSAGELENGTH 32 
  char SysMessage[SYSMESSAGELENGTH]={0};
  // because it is not possible to access Serial.print from Arduino
  // outside the loop function, we copy the messages in a buffer
  // and write the buffer in the loop to the serial port
  #define SYSTEMOUT(text) { strcpy(SysMessage,text); }; 
  #define SYSTEMOUT_(text) { strcpy(SysMessage,text); };
  #define SYSTEMOUTHEX(text,value) { sprintf(SysMessage,"%s %04x ",text, value);};
  
  #ifdef DEBUGCPU
  	#define CPU_DEBUGOUT(text)           SYSTEMOUT(text)
  	#define CPU_DEBUGOUT_(text)          SYSTEMOUT_(text)
  	#define CPU_DEBUGOUTHEX(text,value)  SYSTEMOUTHEX(text,value)
  #else
  	#define CPU_DEBUGOUT(text)
  	#define CPU_DEBUGOUT_(text)
  	#define CPU_DEBUGOUTHEX(text,value)
  #endif
#endif // __SYSTEMOUT__

#ifdef __cplusplus
}
#endif

