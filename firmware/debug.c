#include "debug.h"





void debugBegin() {
#ifdef PIN_DEBUG_OUT
		P1OUT |= PIN_DEBUG_OUT;
#endif
}

void debugEnd() {
#ifdef PIN_DEBUG_OUT
   		P1OUT &= ~PIN_DEBUG_OUT;
#endif
}





