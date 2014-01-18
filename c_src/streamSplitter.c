/****************************************************************************

  streamSplitter

  This module is used to extract words from a data Stream.
  The input of the data stream can be either keyboard or
  a file.

  19.1.2014 chris

 ****************************************************************************/

#include "systemout.h"
#include "streamSplitter.h"

/**************************************************************************

	getCharFromStream()

	Get a char from the stream. The input can be from a file or from
	the keyboard.
	This function is useful to compile words stored in a file.

*************************************************************************/
#define WITHFILESYSTEM

char WordBuffer[MAXWORDLENGTH+1];

uint8_t InputStreamState=GETKEY; // default is input from keyboard

static FILE *InputFile;

char getCharFromStream()
{
	char c;

#ifdef WITHFILESYSTEM

	if(InputStreamState==GETKEY)c=SYSTEMGETCHAR();
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
	if(InputStreamState==READFROMFILE)
	{
		c=fgetc(InputFile);
		if(c==EOF)
		{
			fclose(InputFile);
			InputStreamState=GETKEY; // switch to keyboard
			c=' ';
		}
	}
#else
	c=SYSTEMGETCHAR();
#endif

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
/**************************************************************************

	void getWordFromStreamWithOutComments()

	This ignores comments within the stream

	The syntax for comments is:
	{ a comment in braces must be terminated with a brace }
	( also this )
	\ this comment is terminated by a carriage return

*************************************************************************/
void getWordFromStreamWithOutComments()
{
	char c;
	uint8_t repeatFlag=true;

	while(repeatFlag)
	{
		getWordFromStream();
		repeatFlag=false;

		if(strcmp(WordBuffer,"{")==0)
		{
			do
			{
				c=getCharFromStream();
			}while(c!='}');
			repeatFlag=true;
		}else
		if( strcmp(WordBuffer,"\\")==0) // "\\" is '\'
		{
			do
			{
				c=getCharFromStream();
			}while((c!='\r')&&(c!='\n'));
			repeatFlag=true;
		}else
		if(strcmp(WordBuffer,"(")==0)
		{
			do
			{
				c=getCharFromStream();
			}while(c!=')');
			repeatFlag=true;
		}
	}
}

