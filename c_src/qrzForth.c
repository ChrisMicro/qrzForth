/*
 ============================================================================

 Name        : chForth.c
 Author      : chris_
 Version     :
 Copyright   : GPL license 3
 	 	 	 ( chris (at) roboterclub-freiburg.de )
 Date        : 7 November 2013

 Description : qrzForth - FORTH interpreter in C, Ansi-style

 ============================================================================
 */
/*
 * This is a forth compiler and interpreter written in C.
 * It produces code and runs the code on a virtual machine.
 * The virtual machine is kept very small therefore it can
 * be implemented on microcontrollers with very little resources.
 * Also this machine can be implemented on a FPGA.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "qrzVm.h"
#include "systemout.h"
#include "streamSplitter.h"

uint8_t Base=10; // startup forth with decimal base

extern uint8_t Memory_u8[];
extern uint8_t Memory_bigEndian_u8[]; // big endian memory for other processors than PC
/*
#define STSIZE 0x10000

uint8_t Memory_u8[STSIZE];
uint8_t Memory_bigEndian_u8[STSIZE]; // big endian memory for other processors than PC
*/
// reserve at least 2 bytes for the jmp to main at address 0
// this it the reset location of the VM

//#define CODESTARTADDRESS 		0x0002
//#define RAMHEAPSTART 			0x1800 // location, where FORTH variables are stored, at this address the VM must provide RAM
//#define DICTIONARYSTARTADDRESS 	0x2000
// separate code from directory
// this is used to save memory for code only programs
// in small microcontrollers where you don't need the
// directory information
#define CODE_SEPARATED_FROM_DICTIONARY

uint16_t RamHeap=RAMHEAPSTART;
//typedef uint16_t Index_t; // index into Forth Memory_u8
Index_t CurrentIndex=DICTIONARYSTARTADDRESS; // start dictionary at address 2
Index_t CodePointer=CODESTARTADDRESS;

#define true (1==1)
#define false (!true)
/*
//#define DEBUGCPU
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
#define SYSTEMOUT(text) { puts(text);};
#define SYSTEMOUT_(text) { printf(text);};
#define SYSTEMOUTCR {puts("");};
#define SYSTEMOUTHEX(text,value) { printf(text);printf("%04x ",value);};
#define SYSTEMOUTHEX2(text,value) { printf(text);printf("%02x ",value);};
#define SYSTEMOUTDEC(text,value) { printf(text);printf("%d ",value);};
#define SYSTEMOUTCHAR(value) { putchar(value);};
#define SYSTEMOUTPRINTNUMBER(value) { printf(" %d ",value);};
#define SYSTEMOUTPRINTHEXNUMBER(value) { printf(" %4x ",value);};
*/
/***************************************************************************
  FORTH Dictionary Definition

  if the word CODE_SEPARATED_FROM_DICTIONARY ist not defined in this
  software a dictionary entry has the following format:

  word0:  address of next word, first instruction of word 0, "name", code ...
  word1:  address of next word, first instruction of word 1, "name", code ...
  .....

  Where as the size is as follows:

  16 bit address, 16 bit instruction, 0 terminated string, 16 bit instructions

  If the string end falls on a odd address the string end is extended by 0.
  This ensures a 16bit alignment of the code.

  This implementation uses a virtual machine. This machine has "virtual"
  assembler instructions.
  For assembler instructions in the dictionary the first instruction is the
  assembler instruction. The code field is empty.

  	type the following "see +"

  	adr hexval
	5e    66    	<= index of next entry
	60  8010  + 	<= code ... this is the assembler instruction
	62    2b  + 	<= text
	64     0    	<= text delimiter ( 0 )

  For compound FORTH words the first instruction is a call to the code
  field of the word. The end of the code field has therefore to be a "rts"
  instruction.

  example: "see rshift"

	adr: 332   350  <= index of next entry
	adr: 334  c33e  CALL 33e   (rshift)
	adr: 336  7372  rs
	adr: 338  6968  hi
	adr: 33a  7466  ft
	adr: 33c     0
	===== code =======
	adr: 33e  8020  >r
	adr: 340  800c  _asr
	adr: 342  8021  r>
	adr: 344  8004  1-
	adr: 346  8022  dup
	adr: 348  8000  0=
	adr: 34a  a33e  JMPZ  33e
	adr: 34c  8023  drop
	adr: 34e  803e  rts

****************************************************************************/

/***************************************************************************
 directory structure
 **************************************************************************/
typedef struct{
	Index_t indexOfNextEntry;
	Index_t code;
	uint16_t flags;
	char firstCharOfName;
}DirEntry_t;
/****************************************************************************

 DirEntry_t* getDirEntryPointer(Index_t index)

 Convert a index into the memory into a pointer to the directory entry.

 input:  Index into memory
 output: Pointer to dictionary entry

****************************************************************************/
DirEntry_t* getDirEntryPointer(Index_t index)
{
	DirEntry_t * de;
	de=(DirEntry_t *)&Memory_u8[index];
	return de;
}

/**************************************************************************

	Index_t findEmptyDirEntryIndex()

	Steps through the directory until the last entry is passed and
	returns the first free index.

	return:
		- index of the first free entry

*************************************************************************/
Index_t findEmptyDirEntryIndex()
{
	Index_t index=DICTIONARYSTARTADDRESS,tmp_idx;
	DirEntry_t * de;
	do
	{
		tmp_idx=index;
		de=getDirEntryPointer(index);
		index=de->indexOfNextEntry;
	}while(index!=0);

	return tmp_idx;
}
/****************************************************************************

 void addDirEntry(char * string,Index_t code)

 Add a new FORTH word to the dictionary.
 This instruction is used to add a word with a single assembler instruction
 to the dictionary. The main code field is empty.

****************************************************************************/
void addDirEntry(char * string,Index_t code)
{
	DirEntry_t * de;
	CurrentIndex=findEmptyDirEntryIndex();
	de=getDirEntryPointer(CurrentIndex);
	de->code=code;

	// copy the name string into the memory
	char * s1;
	s1=&(de->firstCharOfName);
	strcpy(s1,string);

	// set current index to next entry
	CurrentIndex+=strlen(s1)+sizeof(DirEntry_t);

	// alignment to 16 bit because the char position can be odd
	if((CurrentIndex & 1)==1)CurrentIndex++;

	de->indexOfNextEntry=CurrentIndex;
	de=getDirEntryPointer(CurrentIndex);

	// set next entry to zero to mark entry as not initialized
	de->indexOfNextEntry=0;
}
/**************************************************************************

	Index_t setEntryName(char * string)

	Set the name in the current dictionary entry and return the start
    of the code area.
    This function is used to make compound FORTH words with multiple
    instructions in the code field.

**************************************************************************/
Index_t setEntryName(char * string)
{
    Index_t nextCodeIndex;	
    DirEntry_t * de;
    CurrentIndex=findEmptyDirEntryIndex();
	de=getDirEntryPointer(CurrentIndex);

	char * s1=&(de->firstCharOfName);
	strcpy(s1,string);

	#ifndef CODE_SEPARATED_FROM_DICTIONARY
		nextCodeIndex=CurrentIndex+strlen(s1)+sizeof(DirEntry_t)-1;// set current index to next entry
		if((nextCodeIndex & 1)==1)nextCodeIndex++; // alignment to 16 bit
		de->code=nextCodeIndex;
	#else
		de->code=CodePointer;
		nextCodeIndex=CodePointer;
	#endif

	// tbd:magic number not allowed, include vm-instructions definition at the beginning
	de->code|=0xC000; // call to code
    return nextCodeIndex; 
}

/**************************************************************************

	Index_t appendEntryCode(Index_t place, Index_t indexToCodeWord)

	Append the current word with the code of the code word given.
	This function is used during compile time.

	inputs
		place : location in forth memory
		indexToForthWord: word which should be added
	output
		updated forth memory location

**************************************************************************/
Index_t appendEntryCode(Index_t place, Index_t indexToForthWord)
{
	 DirEntry_t * de;
	 de=getDirEntryPointer(indexToForthWord);
	 Memory_u8[place++]=(de->code)&0xFF;
	 Memory_u8[place++]=(de->code)>>8;
	 //CodePointer+=2;
	 return place;
}
/**************************************************************************

	Index_t appendCode(Index_t place, Index_t vmInstruction)

	Append a virtual machine instruction to the current code word.
	This function is mainly used for immediate forth words like
	loop, if, variable ...

	inputs
		place : location in forth memory
		vmInstruction: instruction which should be added
	output
		updated forth memory location

**************************************************************************/
Index_t appendCode(Index_t place, Index_t vmInstruction)
{
	 Memory_u8[place++]=vmInstruction&0xFF;
	 Memory_u8[place++]=vmInstruction>>8;
	 //CodePointer+=2;
	 return place;
}
/**************************************************************************

	void nextEntry(Index_t place)

	close entry and set new CurrentIndex

	input
		place : location in forth memory

**************************************************************************/
void nextEntry(Index_t place)
{
    DirEntry_t * de;
	#ifndef CODE_SEPARATED_FROM_DICTIONARY
		de=getDirEntryPointer(CurrentIndex);
		CurrentIndex=place; // next 16bit value
		de->indexOfNextEntry=CurrentIndex;
		de=getDirEntryPointer(CurrentIndex);
		de->indexOfNextEntry=0; // set next entry to zero to mark entry as not initialized
	#else
		CodePointer=place;
		de=getDirEntryPointer(CurrentIndex);
		CurrentIndex=CurrentIndex+strlen(&(de->firstCharOfName))+sizeof(DirEntry_t); // next 16bit value
		if((CurrentIndex & 1)==1)CurrentIndex++; // allignment to 16 bit
		de->indexOfNextEntry=CurrentIndex;
		de=getDirEntryPointer(CurrentIndex);
		de->indexOfNextEntry=0; // set next entry to zero to mark entry as not initialize
	#endif
}

// show one entry in the dictionary
Index_t showDirEntry(Index_t index)
{
	SYSTEMOUTHEX("adr:",index);
	DirEntry_t * de=getDirEntryPointer(index);
	if(de->indexOfNextEntry!=0)
	{
		SYSTEMOUT_(" code: ");
		SYSTEMOUTHEX(" ",de->code);
		SYSTEMOUT_(" - ");
		SYSTEMOUT(&(de->firstCharOfName));
	}
	return de->indexOfNextEntry;
}

void showDictionary()
{
	CurrentIndex=DICTIONARYSTARTADDRESS;
	do
	{
		CurrentIndex=showDirEntry(CurrentIndex);
	}while (CurrentIndex!=0);
}
/**************************************************************************

	Index_t findCode(Index_t code)

	Use the instruction code to find the dictionary index of the instruction.
	This routine can be used for a disassembler

	input: instruction code
	output: dictionary index

**************************************************************************/
Index_t findCode(Index_t code)
{
	DirEntry_t * de;
	uint8_t exit=false;
	Index_t index;
	CurrentIndex=DICTIONARYSTARTADDRESS;
	do
	{
		index=CurrentIndex;
		de=getDirEntryPointer(CurrentIndex);
		if(de->code==code) exit=true;
		CurrentIndex=de->indexOfNextEntry;
	}while ((CurrentIndex!=0)&&!exit);
	return index;
}

/**************************************************************************

	uint8_t isEntry(char * string,Index_t index)

	Check if the name of the entry at "index" equals current word.

	return value:
		true: current index equals string
		false: no match

*************************************************************************/
uint8_t isEntry(char * string,Index_t index)
{
	DirEntry_t * de=(DirEntry_t *)&Memory_u8[index];
	if(strcmp( &(de->firstCharOfName), string )==0) return true;
	else return false;
}

/**************************************************************************

 	 Index_t searchWord(char * string)

 	 Searches the word in the directory and returns its index.

 	 return value:
 	 	 0: word not found
 	 	 indexPosition where the word is found

*************************************************************************/
Index_t searchWord(char * string)
{
	Index_t index=DICTIONARYSTARTADDRESS;
	uint8_t notFound=true;
	DirEntry_t * de;

	while(notFound)
	{
		de=getDirEntryPointer(index);
		notFound=!isEntry(string,index);
		if(notFound)index=de->indexOfNextEntry;
		if(index==0)notFound=false; // it is not found, but index 0 means exit
	}
	return index;
}
/**************************************************************************

 	uint8_t isDigitOfBase(char *chr, uint8_t base)

 	 check if the char is digit of the given base
 	 base 10 : 0..9
	 base 16 : 0..9 a..f A..F

 	 return
 	  true: char is a hex digit
 	  false: not

*************************************************************************/

// tbd: only base 10 or 16 allowed at the moment
uint8_t isDigitOfBase(char chr)
{
	uint8_t flag=false;

	if(Base==16)
	{
		if((chr>='a')&&(chr<='f'))flag=true;
		if((chr>='A')&&(chr<='F'))flag=true;
	}
	if((chr>='0')&&(chr<='9'))flag=true;

	return flag;
}
/**************************************************************************

 	 uint8_t isnumeric(char *str)

 	 check if the given string represents a number.

 	 return
 	  true: string is a number
 	  false: not

*************************************************************************/
uint8_t isnumeric(char *str)
{
	if(*str=='-') str++; // skip "minus", it can be a negative number
	if(*str==0) return false; // if end of string reached
	while(*str)
	{
		if(!isDigitOfBase(*str))
			return false;
		str++;
	}
  return true;
}
/**************************************************************************

	void disassmbleWord(Index_t code)

	Disassemble code word and display it on the console.
	This function my be used by the forth word "see" and for "debug"

*************************************************************************/
void disassmbleWord(Index_t code)
{
	DirEntry_t * de;
	Index_t index;
	index=findCode(code);
	de=getDirEntryPointer(index);

	if((code&COMMANDGROUP2)==0){SYSTEMOUT_(" push\t\t ");}
	else
	if((code&COMMANDGROUP2MASK )==JMPZ)
	{
		SYSTEMOUTHEX(" JMPZ ",code&~JMPZ);
		SYSTEMOUT_("\t");
	}
	else
	if((code&COMMANDGROUP2MASK )==CALL)
	{
		SYSTEMOUTHEX(" CALL ",code&~CALL);
		SYSTEMOUT_("  (");
		SYSTEMOUT_((char *)&de->firstCharOfName);
		SYSTEMOUT_(")  ");
	}
	else
	if((code&COMMANDGROUP2MASK )==JMP){SYSTEMOUTHEX(" JMP",code&~JMP);}
	else
	if((code&COMMANDGROUP2)==COMMANDGROUP2)
	{
		if(de->code!=0)
		{
			SYSTEMOUT_(" ");
			SYSTEMOUT_((char *)&de->firstCharOfName);
			SYSTEMOUT_("\t\t ");
		}
	}
}


/**************************************************************************

	CPU simulator ( or virtual machine )

*************************************************************************/
//typedef Index_t Command_t;



/**************************************************************************

	compile

*************************************************************************/
// definition for words outside FORTH
#define BYE            0xF001
#define WORDS          0xF002
#define COMPILE        0xF003 // :
#define SEE	            0xF004 // show word contents
#define DEBUGSIM       0xF005 // y
#define POINT          0xF007 // emit top stack ( . )
#define SAVEBIN	        0xF008 // save memory as binary file
#define MAKEMAIN       0xF009 // store jmp to main at location 0 in memory
#define INCLUDE	        0xF00A // load and compile forth file
#define DUMPSTACK      0xF00B // in forth .s
#define DEBUGWORD	    0xF00C // debug word
#define RESTART        0xF00D // debug word

// immediate words
#define COMPILESTOP 0xF100 // ;
#define FORTH_IF	0xF101
#define FORTH_THEN	0xF102
#define FORTH_ELSE	0xF103
#define FORTH_BEGIN 0xF104
#define FORTH_UNTIL 0xF105
#define FORTH_0DO	0xF106
#define FORTH_LOOP	0xF107
#define BASEDEC		0xF108 // set base to decimal, forth command d#
#define BASEHEX		0xF109 // set base to hex, forth command h#
#define FORTH_CHAR	0xF10A // the next word should be a char that is converted into a constant
#define FORTH_CONSTANT 0xF10B // define a constant ( only in interpreter mode )
#define FORTH_VARIABLE 0xF10C // define a varible, address taken from RAMHEAP
#define FORTH_ALLOT 0xF10D // increase the RamHeap
#define FORTH_STRING 0xF10E // s" in forth ( set a string in code memory )
#define FORTH_FOR	0xF10F
#define FORTH_NEXT	0xF110
#define FORTH_WHILE	0xF111
#define FORTH_REPEAT 0xF112
#define FORTH_IMMEDIATE 0xF113

// the label stack is only used by the compiler for
// nested loops. e.g. if-else-then, begin-until
#define LABELSTACKSIZE 64

void compile(Index_t compileIndex)
{
	Index_t id;

	uint16_t labelStackPointer=0; // label stack pointer. Use for conditional loops.
	Index_t labelStack[LABELSTACKSIZE];

    uint8_t exitFlag=false;
    do{
    	//getWordFromStream();
    	getWordFromStreamWithOutComments();
    	id=searchWord(WordBuffer);
        if(id==0) // if the word is not found, it could be a number
        {
			if(isnumeric(WordBuffer))
			{
				int16_t number = strtol(WordBuffer, NULL, Base);
				if(number<0)
				{
					number=~number;
					compileIndex= appendCode(compileIndex,number); // add constant to code
					compileIndex= appendCode(compileIndex,_NOT | COMMANDGROUP2); // add constant to code
				}else compileIndex= appendCode(compileIndex,number); // add constant to code
			}
			else
			{
				SYSTEMOUTCR;
				SYSTEMOUT("error: word not found !");
				SYSTEMOUT(WordBuffer);
				exitFlag=true;
			}
        }else
        {
            Index_t forthwordCode=getDirEntryPointer(id)->code,tmp;

            switch(forthwordCode)
            {


            	// forth string start word: s"
            	case FORTH_STRING:{ // make a string in code memory
					// the new label is the address of the jump
					labelStack[labelStackPointer]=compileIndex;
					// set a JMP witch will be modified by the end of string "
					compileIndex= appendCode(compileIndex,(Index_t) 0); // place holder for jump
            		char c;
            		c=getCharFromStream();
            		uint8_t flag=0;
            		uint16_t count=0;
            		uint16_t tmp;
					while(c!='"')
					{
						count++;
						if(flag==0)
						{
							tmp=c;
							flag++;
						}else
						{
							flag=0;
							compileIndex= appendCode(compileIndex,(uint16_t) c<<8 | tmp);
						}
						c=getCharFromStream();
					}
					if(flag==1)compileIndex= appendCode(compileIndex,(uint16_t) tmp);

            		compileIndex= appendCode(compileIndex,(Index_t) 0); // 0 terminaton of string
					// modify the jmp location of "label". This may be the address from "if" or "else"
					appendCode(labelStack[labelStackPointer],JMP | compileIndex);
					// push string start address
					compileIndex=appendCode(compileIndex,labelStack[labelStackPointer]+2);
					 // push string length
					compileIndex=appendCode(compileIndex,count);
            	}break;
            	// forth word: [char]
            	case FORTH_CHAR:{
            		getWordFromStreamWithOutComments();
            		uint16_t value=WordBuffer[0];
            		compileIndex= appendCode(compileIndex,(Index_t) value);
            	}break;

				// forth word: if
				case FORTH_IF: // tbd: stack for nested if's
				{
					//SYSTEMOUT("_IF_");
					labelStack[labelStackPointer++]=compileIndex;
					// the jump has to be modified late on by FORTH_THEN
					// conditional jump to "then"
					compileIndex= appendCode(compileIndex,(Index_t) JMPZ |COMMANDGROUP2);
				}break;
				// forth word: else
				case FORTH_ELSE:
				{
					//SYSTEMOUT("_ELSE_");
					// set jmp address from "if" to this location+2 ( compileIndex )
					tmp=(Index_t ) Memory_u8[labelStack[labelStackPointer-1]]+(Memory_u8[labelStack[labelStackPointer-1]+1]<<8);
					appendCode(labelStack[labelStackPointer-1],tmp | (compileIndex+2));
					// the new label is the address of "else"
					labelStack[labelStackPointer-1]=compileIndex;
					// set a JMP witch will be modified by "then"
					compileIndex= appendCode(compileIndex,(Index_t) JMP |COMMANDGROUP2);
				}break;
				// forth word: then
				case FORTH_THEN:
				{
					//SYSTEMOUT("_THEN_");
					// modify the jmp location of "label". This may be the address from "if" or "else"
					labelStackPointer--;
					tmp=(Index_t ) Memory_u8[labelStack[labelStackPointer]]+(Memory_u8[labelStack[labelStackPointer]+1]<<8);
					appendCode(labelStack[labelStackPointer],tmp | compileIndex);
				}break;
				// forth word: begin
				case FORTH_BEGIN:
				{
					//SYSTEMOUT("_BEGIN_");
					labelStack[labelStackPointer++]=compileIndex;
				}break;
				// forth word: until
				case FORTH_UNTIL:
				{
					//SYSTEMOUT("_UNTIL_");
					compileIndex= appendCode(compileIndex,(Index_t) JMPZ |labelStack[--labelStackPointer]|COMMANDGROUP2);
				}break;
				case FORTH_WHILE:
				{
					// push current addres to stack
					labelStack[labelStackPointer++]=compileIndex;
					compileIndex= appendCode(compileIndex,(Index_t) JMPZ |COMMANDGROUP2);
				}break;
				case FORTH_REPEAT:
				{
					//SYSTEMOUT("_REPEAT_");
					// set "while" jmpz address to this location+2 ( compileIndex )
					appendCode(labelStack[--labelStackPointer],JMPZ | (compileIndex+2));
					// jump to the "begin" statement
					compileIndex= appendCode(compileIndex,(Index_t) JMP |labelStack[--labelStackPointer]|COMMANDGROUP2);
				}break;
				// forth word: 0do ( this is an abbreviation of 0 do )
				case FORTH_0DO:
				{
					//SYSTEMOUT("_0DO_");
					// : TEST   10 0DO  CR ." Hello "  LOOP ;

					labelStack[labelStackPointer++]=compileIndex;
					// store loop count on rsp
					compileIndex= appendCode(compileIndex,(Index_t) DSP2RSP | COMMANDGROUP2);

				}break;
				// forth word: loop
				case FORTH_LOOP:
				{
					// pop rsp ( get loop count )
					compileIndex= appendCode(compileIndex,(Index_t) RSP2DSP | COMMANDGROUP2);
					// 1-
					compileIndex= appendCode(compileIndex,(Index_t) MINUS1 | COMMANDGROUP2);
					// dup
					compileIndex= appendCode(compileIndex,(Index_t) DUP | COMMANDGROUP2);
					// 0=
					compileIndex= appendCode(compileIndex,(Index_t) CMPZA |COMMANDGROUP2);
					// JMPZ label
					compileIndex= appendCode(compileIndex,(Index_t) JMPZ |labelStack[--labelStackPointer]|COMMANDGROUP2);
					// drop
					compileIndex= appendCode(compileIndex,(Index_t) DROP | COMMANDGROUP2);
				}break;
				case FORTH_FOR:
				{
					labelStack[labelStackPointer++]=compileIndex;
					// store loop count on rsp
					compileIndex= appendCode(compileIndex,(Index_t) DSP2RSP | COMMANDGROUP2);
				}break;
				case FORTH_NEXT:
				{
					// pop rsp ( get loop count )
					compileIndex= appendCode(compileIndex,(Index_t) RSP2DSP | COMMANDGROUP2);
					// 1-
					compileIndex= appendCode(compileIndex,(Index_t) MINUS1 | COMMANDGROUP2);
					// dup
					compileIndex= appendCode(compileIndex,(Index_t) DUP | COMMANDGROUP2);
					// invert, ( invert ffff = 0 )
					compileIndex= appendCode(compileIndex,(Index_t) _NOT |COMMANDGROUP2);
					// 0=
					compileIndex= appendCode(compileIndex,(Index_t) CMPZA |COMMANDGROUP2);
					// JMPZ label
					compileIndex= appendCode(compileIndex,(Index_t) JMPZ |labelStack[--labelStackPointer]|COMMANDGROUP2);
					// drop
					compileIndex= appendCode(compileIndex,(Index_t) DROP | COMMANDGROUP2);
				}break;
				// forth word: #d ( switch to dec base )
				case BASEDEC: // set console input/output base
				{
					Base=10;
				}break;
				// forth word: #h ( switch to hex base )
				case BASEHEX:
				{
					Base=16;
				}break;
				// if it was not an immediate word, add the code to the new forth word
				default:
				{
	            	compileIndex= appendEntryCode(compileIndex, id);
				}break;

				// forth word: ;
            	case COMPILESTOP:
				{
	            	if(labelStackPointer==0)
	            	{
	            		compileIndex= appendCode(compileIndex, RTS|COMMANDGROUP2); // lastEntry=return command of cpu
	            		nextEntry(compileIndex);
	            		SYSTEMOUT_(" ");
	            	}else SYSTEMOUT(" error: conditional not closed");
	                exitFlag=true;
				}break;

            }
        }
    }
    while(!exitFlag);
}
#define ROWS_OF_DUMP 8
void dumpMemory(uint16_t address, uint16_t length)
{
	uint16_t n;

	for(n=0;n<length;n++)
	{
		if((n%ROWS_OF_DUMP)==0) SYSTEMOUTCR;
		uint16_t value;
		value=((uint16_t)Memory_u8[address+1]<<8)+Memory_u8[address];
		address+=2;
		SYSTEMOUTHEX("",value);
		// SYSTEMOUT_(" ");
	}
	SYSTEMOUTCR;
	SYSTEMOUTCR;
}
/**********************************************************************
 *
 * void saveBinFile()
 *
 * save the memory content to a binary file.
 * This file can be used as code for other VMs without compiler
 *
 **********************************************************************/
void saveBinFile()
{
    uint32_t k;

	FILE *f,*f2;
    f=fopen("forth.bin","w");
    if(f==NULL)
    {
    	SYSTEMOUT("could not open forth.bin for to write");
    }else
    {
        f2=fopen("beforth.bin","w");
    	if(f2==NULL)
        {
        	SYSTEMOUT("could not open forth.bin for to write");
        }
		else
		{
			for (k=0;k<STSIZE; k++)
			{
				fputc(Memory_u8[k],f); // little endian version
				fputc(Memory_bigEndian_u8[k],f2); // big endian version
			}
	        fclose(f);
	        fclose(f2);
	        SYSTEMOUT("bin files saved");
		}
    }
    SYSTEMOUTDEC("codepointer",CodePointer);
    dumpMemory(0,(CodePointer/ROWS_OF_DUMP/2+2)*ROWS_OF_DUMP);
}
/**********************************************************************

	void save_qrzCode_Atmega()

 	Header file with qrzForth-Code for an AVR controllers.
 	May be use in Arduino qrz-VM.

 **********************************************************************/
void save_qrzCode_Avr()
{
    uint32_t k;

	FILE *f;
	char filenameAndPath[]="../qrzVmBase/qrzCode.h";
    //f=fopen("qrzCode.h","w");
	f=fopen(filenameAndPath,"w");
    if(f==NULL)
    {
    	SYSTEMOUT("could not open forth.bin for to write");
    }else
    {
    	fprintf(f,"#include <avr/pgmspace.h>\n\r");
    	fprintf(f,"#define FORTHCODESIZE %d // words\n\r",CodePointer/2);
    	fprintf(f,"PROGMEM uint16_t programMemory[] ={");
    	for (k=0;k<CodePointer; k+=2)
		{
    		if(k>0)fprintf(f,",");
    		if((k%16)==0)fprintf(f,"\n\r");
    		uint16_t val=((uint16_t)Memory_u8[k+1]<<8) + Memory_u8[k];
			fprintf(f,"0x%04x", val);
		}
    	fprintf(f,"\n\r};\n\r");
        fclose(f);

        SYSTEMOUT("bin files saved to ");
        SYSTEMOUT(filenameAndPath);
    }
}
/**********************************************************************
 *
 * void makeBigEndian()
 *
 * Some mikrocontrollers have a big endian format.
 * To be able to run the code on this machines, we have to reorder it.
 *
 **********************************************************************/
void makeBigEndian()
{
    uint32_t k;
    Index_t index;
	DirEntry_t * de;
	DirEntry_t * bede;
	// swap byte order
    for(k=0;k<STSIZE;k+=2)
    {
    	Memory_bigEndian_u8[k]=Memory_u8[k+1]; // high order byte to call main
    	Memory_bigEndian_u8[k+1]=Memory_u8[k]; // high order byte to call main
    }

	CurrentIndex=DICTIONARYSTARTADDRESS;
    do
    {
    	index=CurrentIndex;
    	de=getDirEntryPointer(index);
    	bede=(DirEntry_t *)&Memory_bigEndian_u8[index];
		char *c=&de->firstCharOfName;
		char *c2=&bede->firstCharOfName;
		//printf("==> %s\n",c);
		strcpy(c2,c);
    		CurrentIndex = de->indexOfNextEntry;
    	}while (CurrentIndex!=0);
}

void dumpStack(Cpu_t *cpu)
{
	uint16_t sp;
	for(sp=0; sp<cpu->datasp;sp++)SYSTEMOUTHEX(" ",cpu->dataStack[sp]);
}

uint8_t forth()
{
  DirEntry_t *de;
  uint16_t k;
  uint8_t debugWord=false;
  uint32_t instructionCounter=0;
  uint8_t restartFlag=false;

  Cpu_t cpu;
  simulatorReset(&cpu);

  addDirEntry("none",0);
  // CPU words
  addDirEntry("0=",       CMPZA           |COMMANDGROUP2);
  addDirEntry("1+",       PLUS1           |COMMANDGROUP2);
  addDirEntry("2+",       PLUS2           |COMMANDGROUP2);
  addDirEntry("0<>",      CMPNZA          |COMMANDGROUP2);
  addDirEntry("1-",       MINUS1          |COMMANDGROUP2);
  addDirEntry("2-",       MINUS2          |COMMANDGROUP2);
  addDirEntry("invert",   _NOT            |COMMANDGROUP2);
  addDirEntry("_shl",     _SHL            |COMMANDGROUP2);
  addDirEntry("_asr",     _ASR            |COMMANDGROUP2);
  addDirEntry("+",        APLUSB          |COMMANDGROUP2);
  // tbd: rework this _aminusb cpu instruction, it's the wrong order for forth
  addDirEntry("_aminusb",     AMINUSB         |COMMANDGROUP2);
  addDirEntry("and",      AANDB           |COMMANDGROUP2);
  addDirEntry("or",       AORB            |COMMANDGROUP2);
  addDirEntry("xor",      AXORB           |COMMANDGROUP2);
  addDirEntry("=",        AEQUALB         |COMMANDGROUP2);
  addDirEntry("<>",       ANOTEQUALB      |COMMANDGROUP2);
  addDirEntry(">",        AGREATERB       |COMMANDGROUP2);
  addDirEntry(">r",       DSP2RSP         |COMMANDGROUP2);
  addDirEntry("r>",       RSP2DSP         |COMMANDGROUP2);
  addDirEntry("dup",      DUP             |COMMANDGROUP2);
  addDirEntry("drop",     DROP            |COMMANDGROUP2);
  addDirEntry("swap",     SWAP            |COMMANDGROUP2);
  addDirEntry("over",     OVER            |COMMANDGROUP2);
  addDirEntry("rot",      ROT             |COMMANDGROUP2);
  addDirEntry("memwrite", MEMWRITE        |COMMANDGROUP2);
  //#define MEMWRITEINC 0x31  // 8031  mem write and inc addr
  addDirEntry("memread",  MEMREAD         |COMMANDGROUP2);
  //#define MEMREADINC    0x33    // 8033  mem read and inc addr
  addDirEntry("popadr",   POPADR          |COMMANDGROUP2);
  addDirEntry("hwuart_txd",   (0x20   <<6)|COMMANDHWWRITE);
  addDirEntry("hwuart_txf",   (0x21   <<6)|COMMANDHWREAD);
  addDirEntry("hwuart_rxd",   (0x22   <<6)|COMMANDHWREAD);
  addDirEntry("hwuart_rxf",   (0x23   <<6)|COMMANDHWREAD);

  addDirEntry("rts",      RTS             |COMMANDGROUP2);
  addDirEntry("jmpz",     JMPZ            |COMMANDGROUP2);
  addDirEntry("call",     CALL            |COMMANDGROUP2);
  addDirEntry("external", EXTERNAL_C      |COMMANDGROUP2);

  // chForth local system words
  addDirEntry("bye",BYE);     // exit forth
  addDirEntry("words",WORDS); // list words
  addDirEntry(":",COMPILE);   // compile
  addDirEntry("see",SEE);     // see word definition
  addDirEntry("y",DEBUGSIM);  // debug simulator
  addDirEntry("save",SAVEBIN);    // save memory as binary file
  addDirEntry("main",MAKEMAIN);   //store jmp to main at location 0 in memory
  addDirEntry("include",INCLUDE); // load and compile new forth ( *.fs ) file
  addDirEntry(".s",DUMPSTACK); // preliminary because the cpu can not read  the stack pointer
  addDirEntry("debug",DEBUGWORD); // debug FORTH word
  addDirEntry("restart",RESTART); // debug FORTH word
  // immediate words
  addDirEntry(";",COMPILESTOP);
  addDirEntry("if",FORTH_IF);
  addDirEntry("then",FORTH_THEN);
  addDirEntry("else",FORTH_ELSE);
  addDirEntry("begin",FORTH_BEGIN);
  addDirEntry("until",FORTH_UNTIL);
  addDirEntry("while",FORTH_WHILE);
  addDirEntry("repeat",FORTH_REPEAT);
  addDirEntry("0do",FORTH_0DO);
  addDirEntry("loop",FORTH_LOOP);
  addDirEntry("for",FORTH_FOR);
  addDirEntry("next",FORTH_NEXT);
  addDirEntry("d#",BASEDEC);
  addDirEntry("h#",BASEHEX);
  addDirEntry("[char]",FORTH_CHAR);
  addDirEntry("constant",FORTH_CONSTANT);
  addDirEntry("variable",FORTH_VARIABLE);
  addDirEntry("allot",FORTH_ALLOT);
  addDirEntry("s\"",FORTH_STRING);
  addDirEntry("immediate",FORTH_IMMEDIATE);

  addDirEntry("endBuiltInWords",5);

  showDictionary();

  Index_t id;

  uint8_t exitFlag=false;
  while(!exitFlag)
  {
      //getWordFromStream();
      getWordFromStreamWithOutComments();

      id=searchWord(WordBuffer);

      if(id==0) // 0 if word not found. It may be a number
      {
          if(isnumeric(WordBuffer)) // check if it is a number
          {
              // the number can be in decimal or hexadecimal format
              int16_t number = strtol(WordBuffer, NULL, Base);
              if(number<0) // unfortunately the cpu can only push 15bit constants
              {
                  // we use a trick to push 16 bit
                  number=~number;
                  executeVm(&cpu,number&0x7fff); // push constant on stack
                  executeVm(&cpu,_NOT | COMMANDGROUP2); // invert
              }else executeVm(&cpu,number&0x7fff); // push constant on stack
          }
          else
              {
                  SYSTEMOUT_ ("don't know ");
                  SYSTEMOUT (WordBuffer);
              }
      }
      else
      {
#ifdef DEBUG
          showDirEntry(id);
#endif
          switch(getDirEntryPointer(id)->code)
          {
              case RESTART:
              {
                restartFlag=true;
                exitFlag=true;
              }break;

              case FORTH_IMMEDIATE:
              {
                  CurrentIndex=findEmptyDirEntryIndex();
                  appendCode(CurrentIndex++,0xAAAA);
                  nextEntry(CurrentIndex);
              }break;

              case FORTH_CHAR:
              {
                  getWordFromStreamWithOutComments();
                  uint16_t value=WordBuffer[0];
                  executeVm(&cpu,value); // push char value on stack
              }break;

              case DEBUGWORD:
              {
                  debugWord=true;
              }break;

              case DUMPSTACK:
              {
                  dumpStack(&cpu);
              }break;

              case BASEDEC:
              {
                  Base=10;
              }break;

              case BASEHEX:
              {
                  Base=16;
              }break;

              case BYE: { // leave forth interpreter
                  exitFlag=true;
                  restartFlag=false;
              }break;

              case WORDS: {
                  SYSTEMOUT("words");
                  showDictionary();
              }break;

              case COMPILE: {
                  SYSTEMOUT_("COMPILE ");
                  getWordFromStreamWithOutComments();
                  SYSTEMOUT_(WordBuffer);
                  CurrentIndex=findEmptyDirEntryIndex();
                  Index_t compileIndex;
                  compileIndex=setEntryName(WordBuffer);
                  compile(compileIndex);
              }break;

              case INCLUDE: {
                  getWordFromStreamWithOutComments(); // get file name
                  InputStreamState=OPENFILE; // switch to file input
              }break;

              case FORTH_CONSTANT: {
                  getWordFromStreamWithOutComments();
                  uint16_t k=pop(&cpu);
                  if((k&0x8000)!=0) SYSTEMOUT("Warning: negative constants will be interpreted as VM commands in this FORTH version");
                  addDirEntry(WordBuffer,k);
              }break;

              case FORTH_VARIABLE: {
                  getWordFromStreamWithOutComments();
                  addDirEntry(WordBuffer,RamHeap++); // store the free address of the RamHeap and increase the RamHeap
              }break;

              case FORTH_ALLOT: {
                  RamHeap+=pop(&cpu);
              }break;

              // set reset vector to forth word given
              // to set the reset vector is mandatory for code running
              // on stand alone VMs
              case MAKEMAIN: {
                      getWordFromStreamWithOutComments();
                      id=searchWord(WordBuffer);
                      if(id!=0) // if word found
                      {
                          de=getDirEntryPointer(id);
                          id=de->code;
                          // id|=JMP; // JMP to main
                          // the first location in memory is the reset address
                          id&=~COMMANDGROUP2MASK; // remove call from address
                          SYSTEMOUTCR;
                          SYSTEMOUTHEX("reset vector set to address: ",id);
                          SYSTEMOUTCR;
                          SYSTEMOUT_("ok>");
                          id|=JMP; // replace by a jmp
                          Memory_u8[0]=id&0xff;
                          Memory_u8[1]=id>>8;
                      }
              }break;

              // SEE: forth disassembler command
              case SEE: {
                  getWordFromStreamWithOutComments();
                  id=searchWord(WordBuffer);

                  SYSTEMOUTHEX("word id: ",id);
                  SYSTEMOUT("");
                  if(id==0)break;
                  de=getDirEntryPointer(id);
                  for(k=id;k<de->indexOfNextEntry;k++) SYSTEMOUTHEX2(" ",Memory_u8[k]);
                  SYSTEMOUT("\n");
                  Index_t codestart;
                  if(((de->code)&COMMANDGROUP2MASK)==CALL)
                  {
                      codestart=(de->code)&~CALL;
                  }
                  else codestart=de->code;
                  for(k=id;k<de->indexOfNextEntry;)
                  {
                      //if(k==codestart){SYSTEMOUT("===== code =======");}
                      SYSTEMOUTHEX("adr:",k);
                      uint16_t cc=(uint16_t) (Memory_u8[k+1]<<8)+Memory_u8[k];
                      SYSTEMOUTHEX(" ",cc);
                      if(k==id){SYSTEMOUT_(" <= index of next entry");}
                      if(k==id+2)disassmbleWord(cc);
                      if(k>(id+3))
                      {
                          if((Memory_u8[k]==0)||( (Memory_u8[k+1]<<8)==0) )
                          {
                              k=de->indexOfNextEntry; // end of string, stop output
                          }
                          else
                          {
                              SYSTEMOUTCHAR(' ');
                              SYSTEMOUTCHAR(Memory_u8[k]);
                              SYSTEMOUTCHAR(Memory_u8[k+1]);
                          }
                      }
                      SYSTEMOUT("");
                      k+=2;
                  }
                  // disassemble code
                  if(((de->code)&COMMANDGROUP2MASK)==CALL)
                  {
                      SYSTEMOUT("===== code =======");
                      codestart=(de->code)&~CALL;
                      k=codestart;
                      uint8_t cnt=0;
                      uint16_t cc;
                      do
                      {
                          cc=(uint16_t) (Memory_u8[k+1]<<8)+Memory_u8[k];
                          SYSTEMOUTHEX("adr:",k);
                          SYSTEMOUTHEX(" ",cc);
                          k+=2;
                          disassmbleWord(cc);
                          SYSTEMOUT("");
                          cnt++;
                      }while((cc!=(RTS|COMMANDGROUP2))&&(cnt<100));
                  }
              }break;

              case SAVEBIN: // save binary file
              {
                  makeBigEndian();
                  saveBinFile();
                  save_qrzCode_Avr();
              }break;

              case DEBUGSIM: // 'y'
              {
                  showCpu(&cpu);
                  SYSTEMOUTDEC("instructions needed: ",instructionCounter);
                  SYSTEMOUT("");
              }break;

              // if no build in command is detected
              // search for the command in the dictionary and execute it
              default:
              {
                  Command_t command;
                  uint16_t callCount=0;

                  command=getDirEntryPointer(id)->code;
                  //SYSTEMOUT_("         ");
                  instructionCounter=1;
                  executeVm(&cpu,command);
                  if(debugWord)
                  {
                      disassmbleWord(command);
                      if((command&COMMANDGROUP2MASK)==CALL) callCount++;
                      SYSTEMOUT_("\t\t stack: ");
                      dumpStack(&cpu);
                      SYSTEMOUTCR;
                  }
#ifdef DEBUG
                  SYSTEMOUTHEX("\ninstr: ",getDirEntryPointer(id)->code);
                  showCpu(&cpu);
#endif
                  uint8_t n=200; // max instruction until stop ( for debugging )

                  while((cpu.retsp!=0)&(n>0)) // run until return stack is empty
                  {
                      command=Memory_u8[cpu.regpc+1]<<8;
                      command|=Memory_u8[cpu.regpc];
                      //SYSTEMOUTHEX("hex",command);
                      instructionCounter++;
                      if(debugWord&&(callCount<2))
                      {
                          {SYSTEMOUTHEX("adr:",cpu.regpc);}
                          executeVm(&cpu,command);
                          disassmbleWord(command);
                          SYSTEMOUT_("\t\t stack: ");
                           dumpStack(&cpu);
                          SYSTEMOUTCR;
                          /*
                          if(instructionCounter++==20)
                          {
                              instructionCounter=0;
                              SYSTEMOUT("press space ==> next ");
                              GETCHAR;
                          }*/
                      }else executeVm(&cpu,command);
                      if((command&COMMANDGROUP2MASK)==CALL) callCount++;
                      if(command==(COMMANDGROUP2|RTS)) --callCount;

#ifdef DEBUG
                      SYSTEMOUTHEX("\ninstr ",command);
                      showCpu(&cpu);
#endif
                      //n--; // enable this to limit number of instructions ( for debugging )
                  }
                  debugWord=false;
              }break;
          }
      }           if(InputStreamState==GETKEY) SYSTEMOUT("ok>");
  }

  return restartFlag;
}
/**************************************************************************

	main

*************************************************************************/
int main(void)
{
  uint8_t restart=true;

  while(restart)
  {
    // initialize global varialbles
    Base=10;
    RamHeap=RAMHEAPSTART;
    CurrentIndex=DICTIONARYSTARTADDRESS; // start dictionary at address 2
    CodePointer=CODESTARTADDRESS;
    uint32_t n;
    for(n=0;n<STSIZE;n++)Memory_u8[STSIZE]=0;

    strcpy(WordBuffer,"startup.fs"); // start up forth file will be loaded first
    InputStreamState=OPENFILE; // load basic words

    SYSTEMOUTCR;
    SYSTEMOUT("!!!chForth!!!");

    restart=forth();

  }

  SYSTEMOUT("good bye");
  return 0;
}

