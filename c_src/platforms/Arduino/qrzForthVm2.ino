/*
 ============================================================================
 Name        : chForth.c
 Author      : chris
 Version     :
 Copyright   : GPL lizense 3
               ( chris (at) roboterclub-freiburg.de )
 Description : qrzForth - FORTH interpreter in C, Ansi-style
 Date        : 5 November 2013
 ============================================================================
 */
/*
 * Forth Virtual Machine for Arduino Uno
 *
 * This virtual machine is meant to run code which is
 * produced from the qrzForth compiler.
 *
 * The code is in a 16bit array in the file "qrzForthCode.h".
 *
 * The virtual machine is kept small. Therefore it can
 * be implemented on microcontrollers with very little resources.
 * Also this machine can be implemented on a FPGA.
 *
 */
#include <stdint.h>
#include <avr/pgmspace.h> 
#include "qrzVm.h"

void setup() {                
  Serial.begin(9600);
}
/***************************************************************************
  
***************************************************************************/

Cpu_t Cpu;// create one cpu instance

void printSysMessage()
{
  static uint8_t n=0;
  char * str;
  str=getSysMessage(); // from the qrz machine
  
  if(str[0]!=0) // check if string is not empty
  {
    Serial.print(str);
    resetMessage();
    n++;
    // if(n==100){n=0;Serial.println("");}; // carriage return after 100 rows
  }
}

void loop() 
{
  Serial.println("start qrz VM");
  delay(1000);              // wait for a second
  static uint8_t n=0;

  Cpu_t * cpu = &Cpu; 
  simulatorReset(cpu);

  while(true)
  {      
     uint16_t command=readMemory(cpu, cpu->regpc/2);

     executeVm(&Cpu, command);
     
     printSysMessage();
     
  }
}
