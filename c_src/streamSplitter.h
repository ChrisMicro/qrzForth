/*
 *
 * StreamSplitter
 *
 */
#ifdef __cplusplus
extern "C"
{
#endif

#ifndef __STREAMSPLITTER__
  #define __STREAMSPLITTER__

	#define true (1==1)
	#define false (!true)

	#define MAXWORDLENGTH 16

	// states of the input stream state machine
	#define GETKEY 0
	#define OPENFILE 1
	#define READFROMFILE 2

	extern char WordBuffer[MAXWORDLENGTH+1];
	extern uint8_t InputStreamState; // default is input from keyboard

	char getCharFromStream(void);
	void getWordFromStream(void);
	void getWordFromStreamWithOutComments();

#endif

#ifdef __cplusplus
}
#endif
