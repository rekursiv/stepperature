
#ifndef DEBUG_H_
#define DEBUG_H_

#define DEBUG

#define PIN_DEBUG_OUT BIT7   // output B

void inline debugBegin() {
#ifdef PIN_DEBUG_OUT
		P1OUT |= PIN_DEBUG_OUT;
#endif
}

void inline debugEnd() {
#ifdef PIN_DEBUG_OUT
   		P1OUT &= ~PIN_DEBUG_OUT;
#endif
}




#endif /* DEBUG_H_ */
