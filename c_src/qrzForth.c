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
#include <time.h>

#define STSIZE 0x10000

uint8_t Memory_u8[STSIZE];
uint8_t Memory_bigEndian_u8[STSIZE]; // big endian memory for other processors than PC

// reserve at least 2 bytes for the jmp to main at address 0
// this it the reset location of the VM

#define CODESTARTADDRESS 		0x0002
#define RAMHEAPSTART 			0x1800 // location, where FORTH variables are stored, at this address the VM must provide RAM
#define DICTIONARYSTARTADDRESS 	0x2000
// separate code from directory
// this is used to save memory for code only programs
// in small microcontrollers where you don't need the
// directory information
#define CODE_SEPARATED_FROM_DICTIONARY

uint16_t RamHeap=RAMHEAPSTART;
typedef uint16_t Index_t; // index into Forth Memory_u8
Index_t CurrentIndex=DICTIONARYSTARTADDRESS; // start dictionary at address 2
Index_t CodePointer=CODESTARTADDRESS;

#define true (1==1)
#define false (!true)

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

	// allignment to 16 bit because the char position can be odd
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
		if((nextCodeIndex & 1)==1)nextCodeIndex++; // allignment to 16 bit
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
uint8_t Base=10; // startup forth with decimal base
// tbd: only basw 10 or 16 allowed at the moment
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
	while(*str)
	{
		if(!isDigitOfBase(*str))
			return false;
		str++;
	}
  return true;
}

/**************************************************************************

	getCharFromStream()

	Get a char from the stream. The input can be from a file or from
	the keyboard.
	This function is useful to compile words stored in a file.

*************************************************************************/
#define MAXWORDLENGTH 16
char WordBuffer[MAXWORDLENGTH+1];

// states of the input stream state machine
#define GETKEY 0
#define OPENFILE 1
#define READFROMFILE 2

uint8_t InputStreamState=GETKEY; // default is input from keyboard

static FILE *InputFile;

char getCharFromStream()
{
	char c;
	if(InputStreamState==GETKEY)c=SYSTEMGETCHAR();
	if(InputStreamState==READFROMFILE)
	{
		c=fgetc(InputFile);
		if(c==EOF)
		{
			close(InputFile);
			InputStreamState=GETKEY; // switch to keyboard
			c=' ';
		}
	}
	return c;
}
/**************************************************************************

	void getWordFromStream()

	- eliminate leading spaces
	- collect chars ( shall not overrun buffer )
	- repeat until word separation ( space, newline, tab )

*************************************************************************/
void getWordFromStream()
{
	uint8_t charIndex=0;
	uint8_t exitFlag=false;
	char c;

	if(InputStreamState==OPENFILE)
	{
		//InputFile = fopen("test.fs", "r");
		InputFile = fopen(WordBuffer, "r");
		if (InputFile == NULL) {
		    SYSTEMOUT_("file not found: ");
		    SYSTEMOUT_(WordBuffer);
		    InputStreamState=GETKEY;
		}else InputStreamState=READFROMFILE;
	}
	// eliminate leading separation chars
	while(!exitFlag)
	{
		c=getCharFromStream();
		exitFlag=true;
		if(c==' ')exitFlag=false;
		if(c=='\n')exitFlag=false;
		if(c=='\r')exitFlag=false;
		if(c=='\t')exitFlag=false;
	}
	exitFlag=false;
	// record word until separation char is reached
	while(!exitFlag)
	{
		if(c==' ')exitFlag=true;
		if(c=='\n')exitFlag=true;
		if(c=='\r')exitFlag=true;
		if(c=='\t')exitFlag=true;
		if(!exitFlag)
		{
			WordBuffer[charIndex++]=c;
			c=getCharFromStream();
		}
		else WordBuffer[charIndex]=0; // set string terminator at the end ( c-style )
	}
}
void getWordFromStreamWithOutComments()
{
	char c;
	getWordFromStream();
	uint8_t exitFlag=false;

	if(strcmp(WordBuffer,"(")==0)
	{
		do
		{
			c=getCharFromStream();
		}while(c!=')');
		getWordFromStream(); // get next word after comment end marker
	}else
	if( strcmp(WordBuffer,"\\")==0) // "\\" is '\'
	{
		while(strcmp(WordBuffer,"\\")==0)
		{
			exitFlag=false;
			while(!	exitFlag)
			{
				c=getCharFromStream();
				//printf("-%c",c);
				if(c=='\n')exitFlag=true;
				if(c=='\r')exitFlag=true;
			}
			getWordFromStream(); // get next word after comment end marker
		}
	}else
	if(strcmp(WordBuffer,"{")==0)
	{
		while(strcmp(WordBuffer,"{")==0)
		{
			exitFlag=false;
			while(!	exitFlag)
			{
				c=getCharFromStream();
				if(c=='}')exitFlag=true;
			}
			getWordFromStream(); // get next word after comment end marker
		}
	}
}

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
/*
* qrz Instructions
 *
 * 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
 *  0 <---------- 15 bit constant push ---------->
 *  1  0  0  x  x  x  x  x  x  x  0  0  0  0  0  0   8000  CMPZ  A = A == 0
 *  1  0  0  x  x  x  x  x  x  x  0  0  0  0  0  1   8001  1+    A = A + 1
 *  1  0  0  x  x  x  x  x  x  x  0  0  0  0  1  0   8002  2+    A = A + 2
 *  1  0  0  x  x  x  x  x  x  x  0  0  0  0  1  1   8003  CMPNZ A = A != 0
 *  1  0  0  x  x  x  x  x  x  x  0  0  0  1  0  0   8004  1-    A = A - 1
 *  1  0  0  x  x  x  x  x  x  x  0  0  0  1  0  1   8005  2-    A = A - 2
 *  1  0  0  x  x  x  x  x  x  x  0  0  1  0  0  0   8008  NOT   A = A - 2
 *  1  0  0  x  x  x  x  x  x  x  0  0  1  0  0  1   8009  ROL   A = {c} A>> 1
 *  1  0  0  x  x  x  x  x  x  x  0  0  1  0  1  0   800A  ROR   A = A <<1{c}
 *  1  0  0  x  x  x  x  x  x  x  0  0  1  0  1  1   800B  SHL   A = A << 1
 *  1  0  0  x  x  x  x  x  x  x  0  0  1  0  1  1   800C  ASR   A = A >> 1
 *  1  0  0  x  x  x  x  x  x  x  0  1  0  0  0  0   8010  ADD A = A + B
 *  1  0  0  x  x  x  x  x  x  x  0  1  0  0  0  1   8011  SUB A = A - B
 *  1  0  0  x  x  x  x  x  x  x  0  1  0  0  1  0   8012  AND A = A & B
 *  1  0  0  x  x  x  x  x  x  x  0  1  0  0  1  1   8013  OR  A = A | B
 *  1  0  0  x  x  x  x  x  x  x  0  1  0  1  0  0   8014  XOR A = A ^ B
 *  1  0  0  x  x  x  x  x  x  x  0  1  0  1  0  1   8015  =
 *  1  0  0  x  x  x  x  x  x  x  0  1  0  1  1  0   8016  !=
 *  1  0  0  x  x  x  x  x  x  x  0  1  0  1  1  1   8017  >
 *  1  0  0  x  x  x  x  x  x  x  1  0  0  0  0  0   8020  >r
 *  1  0  0  x  x  x  x  x  x  x  1  0  0  0  0  1   8021  r>
 *  1  0  0  x  x  x  x  x  x  x  1  0  0  0  1  0   8022  dup
 *  1  0  0  x  x  x  x  x  x  x  1  0  0  0  1  1   8023  drop
 *  1  0  0  x  x  x  x  x  x  x  1  1  0  0  0  0   8030  mem write
 *  1  0  0  x  x  x  x  x  x  x  1  1  0  0  0  1   8031  mem write and inc addr
 *  1  0  0  x  x  x  x  x  x  x  1  1  0  0  1  0   8032  mem read
 *  1  0  0  x  x  x  x  x  x  x  1  1  0  0  1  1   8033  mem read and inc addr
 *  1  0  0  x  x  x  x  x  x  x  1  1  1  1  0  1   803D  pop address
 *  1  0  0  x  x  x  x  x  x  x  1  1  1  1  1  0   803E  rts
 *  1  0  1  a  a  a  a  a  a  a  a  a  a  a  a  a   A/B000 jmpz
 *  1  1  0  a  a  a  a  a  a  a  a  a  a  a  a  a   C/D000 call
 *  1  1  1  a  a  a  a  a  a  a  a  a  a  a  a  a   E/F000 JMP
 */

// Instructions
#define CONST 		0x0000	// if the highest bit is not set, a constant is pushed on stack
#define CMPZA	  	0x00 	// 8000  CMPZ  A = A == 0
#define PLUS1   	0x01 	// 8001  1+    A = A + 1
#define PLUS2   	0x02 	// 8002  2+    A = A + 2
#define CMPNZA	 	0x03 	// 8003  CMPNZ A = A != 0
#define MINUS1  	0x04 	// 8004  1-    A = A - 1
#define MINUS2  	0x05 	// 8005  2-    A = A - 2
#define _NOT    	0x08 	// 8008  NOT   A =~ A
//#define _ROL    	0x09 	// 8009  ROL   A = {c} A>> 1
//#define _ROR    	0x0A 	// 800A  ROR   A = A <<1{c}
#define _SHL    	0x0B 	// 800B  SHL   A = A << 1
#define _ASR    	0x0C 	// 800C  ASR   A = A >> 1
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
//8025 over
#define ROT			0x26 	// 8026 roll ( rot in Forth )
#define MEMWRITE 	0x30	// 8030  mem write
//#define MEMWRITEINC 0x31 	// 8031  mem write and inc addr
#define MEMREAD 	0x32 	// 8032  mem read
//#define MEMREADINC 	0x33 	// 8033  mem read and inc addr
#define POPADR 		0x3D 	// 803D  pop address ( pop to addr register A )
#define RTS 		0x3E 	// 803E   rts

#define JMPZ 		0xA000 		// A/B000 jmpz
#define CALL 		0xC000 		// C/D000 call
#define JMP 		0xE000 		// E/F000 JMP

#define EXTERNAL_C	0x80	// call c function outside simulator


#define COMMANDGROUP2 		0x8000
#define COMMANDGROUP2MASK 	0xE000
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

/**************************************************************************

	CPU simulator ( or virtual machine )

*************************************************************************/
typedef Index_t Command_t;

#define DATASTACKSIZE 16
#define RETURNSTACKSIZE 32
#define RETSP_INITVAL 0 // return stack origin

typedef struct {
	uint16_t datasp; // data stack pointer
	uint16_t retsp;  // return	stack pointer
	uint16_t dataStack[DATASTACKSIZE];
	uint16_t returnStack[RETURNSTACKSIZE];
	uint16_t regpc;  // programm counter
	uint16_t regadr; // adress register, used to hold address for memread/memwrite
	uint16_t *memory;
}Cpu_t;

// tbd: move display memory outside the cpu
#define COLUMNS 		100
#define ROWS			10
#define SCREENADDRESS 	0x6000

void showScreen(Cpu_t *cpu)
{
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
}

void simulatorReset(Cpu_t *cpu)
{
	cpu->datasp=0;
	cpu->retsp=RETSP_INITVAL;
	cpu->regpc=0;
	cpu->regadr=0;
	cpu->memory=( uint16_t* )Memory_u8;
}

void showCpu(Cpu_t *cpu)
{
	SYSTEMOUTHEX("regpc: ",cpu->regpc);
	SYSTEMOUTHEX("datasp: ",cpu->datasp);
	SYSTEMOUTHEX("retsp: ",cpu->retsp);
	SYSTEMOUTHEX("regadr: ",cpu->regadr);
	SYSTEMOUT("");
}

void showMemory(Cpu_t *cpu,uint16_t count)
{
	uint16_t n;
	for(n=0;n<count; n++) CPU_DEBUGOUTHEX(" ",cpu->stacks[n]);
}

void writeMemory(Cpu_t *cpu,uint16_t wordAddress, uint16_t value)
{
	cpu->memory[wordAddress]=value;
	if(wordAddress==SCREENADDRESS) showScreen(cpu);
}

uint16_t readMemory(Cpu_t *cpu, uint16_t wordAddress)
{
	return cpu->memory[wordAddress];
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
/**************************************************************************

	void cpuExternalCall(uint16_t command, uint16_t value)

	The simulator may call external functions outside of FORTH
	This functions are implemented in this C-Code.

	parameters
		command: first value from datasp
		value: second value from datasp

*************************************************************************/
#define EXTERNAL_C_PRINTCHAR 	0
#define EXTERNAL_C_PRINTNUMBER 	1
#define EXTERNAL_C_SETBASE 		2
#define EXTERNAL_C_GETKEY		3
#define EXTERNAL_C_GETMSTIME	4

void cpuExternalCall(Cpu_t *cpu)
{
	//SYSTEMOUTHEX("command",command);
	//SYSTEMOUTHEX("value",value);
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
		default: SYSTEMOUT("external command not found");
	}
}
// main routine vor the cpu ( or vm )
void executeVm(Cpu_t *cpu, Command_t command)
{
#ifdef DEBUGCPU
	showCpu(cpu);
#endif
	CPU_DEBUGOUTHEX(" code",command);
#ifdef DEBUGCPU
	CPU_DEBUGOUT_(" forth:");
	disassmbleWord(command);
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
			// tbd
			//#define _ROL    	0x09 	// 8009  ROL   A = {c} A>> 1
			//#define _ROR    	0x0A 	// 800A  ROR   A = A <<1{c}
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
			case DSP2RSP:{CPU_DEBUGOUT("dsp2rsp");
				push2ReturnStack(cpu,pop(cpu));
			}break;

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

			// 8031  mem write and inc addr
			/*
			case MEMWRITEINC:{CPU_DEBUGOUT("MEMWRITEINC");
				cpu->memory[(cpu->regadr)++]=pop(cpu);
			}break;
			*/
			// 8032  mem read
			case MEMREAD:{CPU_DEBUGOUT("memrd");
				push(cpu,readMemory(cpu,cpu->regadr));
				//push(cpu,cpu->memory[(cpu->regadr)]);
			}break;

			// 8033  mem read and inc addr
			/*
			case MEMREADINC:{CPU_DEBUGOUT("MEMREADINC");
				push(cpu,cpu->memory[(cpu->regadr++)]);
			}break;
			*/

			// 8037  pop address
			case POPADR:{ CPU_DEBUGOUT("popadr");
				cpu->regadr=pop(cpu);
			}break;

			// this funktion needs 2 data stack elements
			case EXTERNAL_C:{ CPU_DEBUGOUT("external c");
				cpuExternalCall(cpu);
			}break;
			default: SYSTEMOUT("cpu cmd error");
		}
		cpu->regpc+=2; // regpc addresses bytes

	}
}

/**************************************************************************

	compile

*************************************************************************/
// definition for words outside FORTH
#define BYE         0xF001
#define WORDS       0xF002
#define COMPILE     0xF003 // :
#define SEE			0xF004 // show word contents
#define DEBUGSIM	0xF005 // y
#define POINT		0xF007 // emit top stack ( . )
#define SAVEBIN		0xF008 // save memory as binary file
#define MAKEMAIN	0xF009 // store jmp to main at location 0 in memory
#define INCLUDE		0xF00A // load and compile forth file
#define DUMPSTACK	0xF00B // in forth .s
#define DEBUGWORD	0xF00C // debug word

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
// the label stack is only used by the compiler for
// nested loops. e.g. if-else-then, begin-until
#define LABELSTACKSIZE 64

void compile()
{
	Index_t id,compileIndex;

	uint16_t labelStackPointer=0; // label stack pointer. Use for conditional loops.
	Index_t labelStack[LABELSTACKSIZE];

    SYSTEMOUT_("COMPILE ");
    getWordFromStreamWithOutComments();
    SYSTEMOUT_(WordBuffer);
    CurrentIndex=findEmptyDirEntryIndex();
    compileIndex=setEntryName(WordBuffer);

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
    f=fopen("qrzCode.h","w");
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

        SYSTEMOUT("bin files saved");
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
/**************************************************************************

	main

*************************************************************************/
int main(void) {

	DirEntry_t *de;
	uint16_t k;
	uint8_t debugWord=false;
	uint32_t instructionCounter=0;

	strcpy(WordBuffer,"startup.fs"); // start up forth file will be loaded first
	InputStreamState=OPENFILE; // load basic words

	SYSTEMOUT("!!!chForth!!!"); /* prints !!!Hello World!!! */
	Cpu_t cpu;
	simulatorReset(&cpu);

	addDirEntry("none",0);
	// CPU words
	addDirEntry("0=",		CMPZA			|COMMANDGROUP2);
	addDirEntry("1+",		PLUS1			|COMMANDGROUP2);
	addDirEntry("2+",		PLUS2			|COMMANDGROUP2);
	addDirEntry("0<>",		CMPNZA			|COMMANDGROUP2);
	addDirEntry("1-",		MINUS1			|COMMANDGROUP2);
	addDirEntry("2-",		MINUS2			|COMMANDGROUP2);
	addDirEntry("invert",	_NOT			|COMMANDGROUP2);
	addDirEntry("_shl",		_SHL			|COMMANDGROUP2);
	addDirEntry("_asr",		_ASR			|COMMANDGROUP2);
	addDirEntry("+",		APLUSB			|COMMANDGROUP2);
	// tbd: rework this _aminusb cpu instruction, it's the wrong order for forth
	addDirEntry("_aminusb",		AMINUSB			|COMMANDGROUP2);
	addDirEntry("and",		AANDB			|COMMANDGROUP2);
	addDirEntry("or",		AORB			|COMMANDGROUP2);
	addDirEntry("xor",		AXORB			|COMMANDGROUP2);
	addDirEntry("=",		AEQUALB			|COMMANDGROUP2);
	addDirEntry("<>",		ANOTEQUALB		|COMMANDGROUP2);
	addDirEntry(">",		AGREATERB		|COMMANDGROUP2);
	addDirEntry(">r",		DSP2RSP			|COMMANDGROUP2);
	addDirEntry("r>",		RSP2DSP			|COMMANDGROUP2);
	addDirEntry("dup",		DUP				|COMMANDGROUP2);
	addDirEntry("drop",		DROP			|COMMANDGROUP2);
	addDirEntry("swap",		SWAP			|COMMANDGROUP2);
	addDirEntry("rot",		ROT				|COMMANDGROUP2);
	addDirEntry("memwrite",	MEMWRITE		|COMMANDGROUP2);
	//#define MEMWRITEINC 0x31 	// 8031  mem write and inc addr
	addDirEntry("memread",	MEMREAD			|COMMANDGROUP2);
	//#define MEMREADINC 	0x33 	// 8033  mem read and inc addr
	addDirEntry("popadr",	POPADR			|COMMANDGROUP2);
	addDirEntry("rts",		RTS				|COMMANDGROUP2);
	addDirEntry("jmpz",		JMPZ			|COMMANDGROUP2);
	addDirEntry("call",		CALL			|COMMANDGROUP2);
	addDirEntry("external",	EXTERNAL_C		|COMMANDGROUP2);

	// chForth local system words
	addDirEntry("bye",BYE);		// exit forth
	addDirEntry("words",WORDS); // list words
	addDirEntry(":",COMPILE); 	// compile
	addDirEntry("see",SEE); 	// see word definition
	addDirEntry("y",DEBUGSIM); 	// debug simulator
	addDirEntry("save",SAVEBIN); 	// save memory as binary file
	addDirEntry("main",MAKEMAIN); 	//store jmp to main at location 0 in memory
	addDirEntry("include",INCLUDE); // load and compile new forth ( *.fs ) file
	addDirEntry(".s",DUMPSTACK); // preliminary because the cpu can not read  the stack pointer
	addDirEntry("debug",DEBUGWORD); // debug FORTH word
	// immediate words
	addDirEntry(";",COMPILESTOP);
	addDirEntry("if",FORTH_IF);
	addDirEntry("then",FORTH_THEN);
	addDirEntry("else",FORTH_ELSE);
	addDirEntry("begin",FORTH_BEGIN);
	addDirEntry("until",FORTH_UNTIL);
	addDirEntry("0do",FORTH_0DO);
	addDirEntry("loop",FORTH_LOOP);
	addDirEntry("d#",BASEDEC);
	addDirEntry("h#",BASEHEX);
	addDirEntry("[char]",FORTH_CHAR);
	addDirEntry("constant",FORTH_CONSTANT);
	addDirEntry("variable",FORTH_VARIABLE);
	addDirEntry("allot",FORTH_ALLOT);
	addDirEntry("s\"",FORTH_STRING);

	addDirEntry("endBuildInWords",5);

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
				case BYE: {
					//SYSTEMOUT("good bye");
					exitFlag=true;
				}break;
				case WORDS: {
					SYSTEMOUT("words");
					showDictionary();
				}break;
				case COMPILE: {
                    compile();
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

					/*	Index_t compileIndex;
				    getWordFromStreamWithOutComments();
				    CurrentIndex=findEmptyDirEntryIndex();
				    compileIndex=setEntryName(WordBuffer);
	            	compileIndex= appendCode(compileIndex, RamHeap++); // store the free address of the RamHeap and increase the RamHeap
	            	compileIndex= appendCode(compileIndex, RTS|COMMANDGROUP2); // lastEntry=return command of cpu
	            	nextEntry(compileIndex);
					 */
				}break;
				case FORTH_ALLOT: {
			  		RamHeap+=pop(&cpu);
				}break;

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
					uint8_t n=1000; // max instruction until stop ( for debugging )

					while((cpu.retsp!=RETSP_INITVAL)&(n>0)) // run until return stack is empty
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
		}			if(InputStreamState==GETKEY) SYSTEMOUT("ok>");
	}

	SYSTEMOUT("good bye");
	return 0;
}

