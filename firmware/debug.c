#include <msp430.h>
#include "debug.h"
#include "serial.h"




void debugBegin() {
#ifdef PIN_DEBUG_OUT
		P1OUT |= PIN_DEBUG_OUT;
#endif
	serialBegin();
}

void debugEnd() {
#ifdef PIN_DEBUG_OUT
   		P1OUT &= ~PIN_DEBUG_OUT;
#endif
	serialEnd();
}

void debugChar(const unsigned char data) {
	serialAddByte(data);
}



