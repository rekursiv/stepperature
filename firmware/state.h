

#ifndef STATE_H_
#define STATE_H_

// state flags
#define S_SM_MASK (BIT0|BIT1)   // mask for step mode state
#define S_SM_QUAD 0				// A=quadrature A, B=quadrature B
#define S_SM_SD 1				// A=step, B=direction
#define S_SM_SASB 2				// A=step dir A, B=step dir B
#define S_SM_SERIAL 3			// A='+' char on serial port, B='-' char on serial port
#define S_SM_LAST 3				// last valid mode in list
#define S_16MHZ BIT2
#define S_BTN_DN BIT3
#define S_BTN_HELD BIT4
#define S_ADC_IN_A BIT5
#define S_STEPPING BIT6


unsigned char state = 0;


#endif /* STATE_H_ */
