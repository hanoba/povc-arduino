#include <Arduino.h>
#include "trace.h"
#include "bt.h"

extern uint32_t rotationCounter;

//------------------------------------------------------------------
  void Trace::log(char tag, int value)
//------------------------------------------------------------------
{
	if (!stopped) {
        unsigned long us = micros();
		//noInterrupts();
		rotcnt[index] = rotationCounter;
		timestamps[index] = us;
        tags[index] = tag;
        values[index] = value;
        index = index==SIZE-1 ? 0 : index+1;
        n_entries = n_entries==SIZE? SIZE : n_entries+1;
		//interrupts();
	}
}

//------------------------------------------------------------------
  void Trace::print(void)
//------------------------------------------------------------------
{
    char text[80];
    int n = n_entries;
    int i = index;
	int sec, ms, us;
    
	btWriteString("\n Time in us RotCnt T Value\n");
	btWriteString(  "----------------------------------\n");
	//                59.371.227    924 R 924
    while (n--) {
        i--;
        if (i<0) i = SIZE-1;
		us = timestamps[i];
		ms = us / 1000;
		sec = ms / 1000;
		ms = ms % 1000;
		us = us % 1000;
        sprintf(text, "%3d.%03d.%03d %6lu %c %d\n", sec, ms, us, rotcnt[i], tags[i], values[i]);
        btWriteString(text);
    }
}