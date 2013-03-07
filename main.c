#include <msp430.h>
#include "printf.h"


//#define REF_OFFSET 0    ////////////

#define SAMPLE_RATE 20000	// in Hz
#define DEADBAND_DEFAULT 20	// amount of "hysteresis" used when determining top vs. bottom half of waveform
#define DEADBAND_MIN 15
#define DEADBAND_MAX 45
#define UP_COUNT_MIN 2		// how high to count before triggering a step when waveform is moving quickly
#define UP_COUNT_MAX 50		// how high to count before triggering a step when waveform is moving slowly
#define ADJUST_DELAY 2000	// how far off centerline the averaging sum has to be before adjustment is made (smaller adjusts faster)
#define REF_CENTER 590  	// approx. center point of waveform to use as starting point (will adjust itself)

#define CLOCK_RATE 16000000	// 16 MHz

// debugging
#define PIN_DEBUG_OUT
#define USE_SERIAL
#define SER_STEP_OUT
//#define SER_DEBUG_OUT
/////

#define TEST1_OUT BIT7
#define A_OUT BIT0		//  red LED
#define B_OUT BIT6		//  green LED
#define BTN_IN BIT3

#define A_IN INCH_4
#define B_IN INCH_5

// state flags
#define S_SM_MASK BIT0+BIT1     // mask for step mode state
#define S_SM_QUAD 0				// A=quadrature A, B=quadrature B
#define S_SM_SD 1				// A=step, B=direction
#define S_SM_SASB 2				// A=step dir A, B=step dir B
#define S_SM_LAST 2				// last valid mode in list
//#define S_SERIAL_STEP_OUT BIT2  // enable output of steps over serial port
#define S_BTN_DN BIT3
#define S_ADC_IN_A BIT4
#define S_STEPPING BIT5

unsigned char state = S_SM_QUAD;


unsigned int loopCount = 0;
volatile unsigned int adcData_A = 0;
volatile unsigned int adcData_B = 0;
unsigned int adcRef_A = REF_CENTER;
unsigned int adcRef_B = REF_CENTER;
unsigned int adcUpCount_A = 0;
unsigned int adcUpCount_B = 0;
int adcOffsetAdj_A = 0;
int adcOffsetAdj_B = 0;
unsigned char upCountRef = UP_COUNT_MAX;
unsigned char adcDeadband_A = DEADBAND_DEFAULT;
unsigned char adcDeadband_B = DEADBAND_DEFAULT;


void inline initDebug() {
    initPut();
    cls();
    puts("Ready.");
    sendPut(0);
}

void inline initClk() {
	BCSCTL1 = CALBC1_16MHZ;         // set clock speed
	DCOCTL = CALDCO_16MHZ;
}

void inline initBtn() {
    P1REN |= BTN_IN; // enable internal pullup
    P1OUT |= BTN_IN; // set pullup
}

void inline initOuts() {
    P1DIR |= (A_OUT + B_OUT); // set leds as outputs
    P1OUT &= ~A_OUT; // turn off red
    P1OUT &= ~B_OUT;  // turn off green
#ifdef PIN_DEBUG_OUT
    // init output used for testing
	P1DIR |= TEST1_OUT;
	P1OUT &= ~TEST1_OUT;
#endif
}

void inline initAdc() {
	ADC10AE0 &= A_IN+B_IN;      // analog enable for input pins
	ADC10CTL0 &= ~ENC;					// Disable ADC
	ADC10CTL0 = ADC10SHT_0 + ADC10ON + ADC10IE  // 4 clock ticks, ADC On, enable ADC interrupt
					+ REFON + SREF_1 + REF2_5V; //  enable 2.5V ref
	ADC10CTL0 |= ENC;  	// Enable ADC
}

void inline initTimer() {
	TA0CCR0 = (CLOCK_RATE/SAMPLE_RATE)-1;		// Count limit (16 bit)
	TA0CCTL0 = CCIE;							// capture compare interrupt enable
	TA0CTL = TASSEL_2+MC_1; 					// use SMCLK, count UP
}

void inline adcTriggerSample(unsigned const int chan) {
	ADC10CTL0 &= ~ENC;				// Disable ADC
	ADC10CTL1 &= 0x0fff;  			// clear input channel bits
	ADC10CTL1 |= chan;    			// set input channel bits
	ADC10CTL0 |= ENC + ADC10SC; 	// Enable and start conversion
}

void inline incMode() {
	if ((state&S_SM_MASK)==S_SM_LAST) state&=~S_SM_MASK;
	else ++state;
	//  TODO: store state in flash, load on boot
}

void inline incQuad() {
	if ((P1OUT&A_OUT)==0&&(P1OUT&B_OUT)==0) P1OUT|=A_OUT;
	else if ((P1OUT&A_OUT)!=0&&(P1OUT&B_OUT)==0) P1OUT|=B_OUT;
	else if ((P1OUT&A_OUT)!=0&&(P1OUT&B_OUT)!=0) P1OUT&=~A_OUT;
	else if ((P1OUT&A_OUT)==0&&(P1OUT&B_OUT)!=0) P1OUT&=~B_OUT;
}

void inline decQuad() {
	if ((P1OUT&A_OUT)==0&&(P1OUT&B_OUT)==0) P1OUT|=B_OUT;
	else if ((P1OUT&A_OUT)==0&&(P1OUT&B_OUT)!=0) P1OUT|=A_OUT;
	else if ((P1OUT&A_OUT)!=0&&(P1OUT&B_OUT)!=0) P1OUT&=~B_OUT;
	else if ((P1OUT&A_OUT)!=0&&(P1OUT&B_OUT)==0) P1OUT&=~A_OUT;
}

void inline stepA_B() {  // step A leading B
	if ((state&S_SM_MASK)==S_SM_QUAD) incQuad();
	else if ((state&S_SM_MASK)==S_SM_SD) P1OUT|=B_OUT;
	else if ((state&S_SM_MASK)==S_SM_SASB) P1OUT|=A_OUT;
#ifdef SER_STEP_OUT
	initPut();
	putc('+');
	sendPut(0);
#endif
}

void inline stepB_A() {  // step B leading A
	if ((state&S_SM_MASK)==S_SM_QUAD) decQuad();
	else if ((state&S_SM_MASK)==S_SM_SD) P1OUT&=~B_OUT;
	else if ((state&S_SM_MASK)==S_SM_SASB) P1OUT|=B_OUT;
#ifdef SER_STEP_OUT
	initPut();
	putc('-');
	sendPut(0);
#endif
}

void inline stepReset() {
	if ((state&S_SM_MASK)!=S_SM_QUAD) {
  		P1OUT&=~A_OUT;
  		P1OUT&=~B_OUT;
	}
}

//int min = 0;
//int max = 0;
//nt test = 0;

void inline calcRunningAverage() {
	adcOffsetAdj_A+=adcRef_A-adcData_A;
	if (adcOffsetAdj_A<-ADJUST_DELAY) {
		++adcRef_A;
		adcOffsetAdj_A=0;
	} else if (adcOffsetAdj_A>ADJUST_DELAY) {
		--adcRef_A;
		adcOffsetAdj_A=0;
	}

	adcOffsetAdj_B+=adcRef_B-adcData_B;
	if (adcOffsetAdj_B<-ADJUST_DELAY) {
		++adcRef_B;
		adcOffsetAdj_B=0;
	} else if (adcOffsetAdj_B>ADJUST_DELAY) {
		--adcRef_B;
		adcOffsetAdj_B=0;
	}
}


void inline calcUpCountRef(unsigned int upCount) {
	upCountRef=upCount>>3;  //  /8
	if (upCountRef<UP_COUNT_MIN) upCountRef=UP_COUNT_MIN;
	else if (upCountRef>UP_COUNT_MAX) upCountRef=UP_COUNT_MAX;
//	printf("u:%u  ", upCountRef);
}



int main(void) {
    WDTCTL = WDTPW + WDTHOLD;  // throw the watchdog a bone

    // set stuff up
    initClk();
    initBtn();
    initOuts();
#ifdef USE_SERIAL
    initSerial();
#endif
    initAdc();
    initTimer();
    _enable_interrupts();
#ifdef SER_DEBUG_OUT
    initDebug();
#endif

    while(1) {         //  main loop

    	_bis_SR_register(CPUOFF);  // wait for timer
#ifdef PIN_DEBUG_OUT
    	P1OUT |= TEST1_OUT;
#endif
#ifdef SER_DEBUG_OUT
    	initPut();
#endif

    	// handle step output for step/dir mode
    	if ((state&S_STEPPING)&&(state&S_SM_MASK)==S_SM_SD&&(P1OUT&A_OUT)==0) P1OUT|=A_OUT;

/*
   	  	// handle button press
   	  	if ((P1IN&BTN_IN)==0&&(state&S_BTN_DN)==0) {
   	  		calibrate();
//  	  		printf(" %u:%u ", adcData_A, adcData_B);
//   	  		incMode();
   	  		state |= S_BTN_DN;
//   	  		continue;
   	  	} else if ((P1IN&BTN_IN)&&(state&S_BTN_DN)) {
   	  		state &= ~S_BTN_DN;
//   	  		continue;
   	  	}
*/


    	//  acquire data from ADC
     	state |= S_ADC_IN_A;
    	adcTriggerSample(A_IN);
     	_bis_SR_register(CPUOFF);   // wait for sample A
     	state &= ~S_ADC_IN_A;
   		adcTriggerSample(B_IN);
   	  	_bis_SR_register(CPUOFF);   // wait for sample B

   	  	calcRunningAverage();
/*
  		if (loopCount%0x2000==0) {
//  			printf("D:%u:%u ", adcDeadband_A, adcDeadband_B);
  			if (adcDeadband_A>DEADBAND_MIN) --adcDeadband_A;
  			if (adcDeadband_B>DEADBAND_MIN) --adcDeadband_B;
   	  	}
*/
#ifdef SER_DEBUG_OUT
   	  	//  TEST  // // //
   	  	if ((P1IN&BTN_IN)==0) {
   	  		printf("u:%u  ", upCountRef);
   	  		printf("A:%u:%u|%i ", adcData_A, adcRef_A, adcOffsetAdj_A);
   	  		printf("B:%u:%u|%i ", adcData_B, adcRef_B, adcOffsetAdj_B);
   	  	}  // // // // // //
#endif


   	  	// process sample data

   	  	if (adcData_A>adcRef_A+adcDeadband_A) {
  			++adcUpCount_A;
   	  	} else if (adcData_A<adcRef_A-adcDeadband_A) {
   	  		if (adcUpCount_A==1) {
//   	  			printf("a%u ", adcDeadband_A);
   	  			if (adcDeadband_A<DEADBAND_MAX) ++adcDeadband_A;
   	  		} else if (adcUpCount_A>100&&adcUpCount_B>100&&(int)(adcData_B-(adcRef_B+adcDeadband_B))>20) {
//  	  			printf("Ab:%i:%u ", adcData_B-(adcRef_B+adcDeadband_B), adcDeadband_A);
  	  			if ((int)(adcData_B-(adcRef_B+adcDeadband_B))>80) {
  	  				if (adcDeadband_A<DEADBAND_MAX) ++adcDeadband_A;
  	  			} else {
  	  				if (adcDeadband_A>DEADBAND_MIN) --adcDeadband_A;
  	  			}
  	  		}
   	  		adcUpCount_A=0;
   	  	}

   	  	if (adcData_B>adcRef_B+adcDeadband_B) {
   	  		++adcUpCount_B;
   	  	} else if (adcData_B<adcRef_B-adcDeadband_B) {
   	  		if (adcUpCount_B==1) {
 //  	  			printf("b%u ", adcDeadband_B);
   	  			if (adcDeadband_B<DEADBAND_MAX) ++adcDeadband_B;
   	  		} else  if (adcUpCount_A>100&&adcUpCount_B>100&&(int)(adcData_A-(adcRef_A+adcDeadband_A))>20) {
//  	  			printf("Ba:%i:%u ", adcData_A-(adcRef_A+adcDeadband_A), adcDeadband_B);
  	  			if ((int)(adcData_A-(adcRef_A+adcDeadband_A))>80) {
  	  				if (adcDeadband_B<DEADBAND_MAX) ++adcDeadband_B;
  	  			} else {
  	  				if (adcDeadband_B>DEADBAND_MIN) --adcDeadband_B;
  	  			}
  	  		}
   	  		adcUpCount_B=0;
   	  	}

   	 	if ((state&S_STEPPING) && adcUpCount_A==0 && adcUpCount_B==0) {
  	  		stepReset();
   	  		state &= ~S_STEPPING;
   		}

   	 	if ((state&S_STEPPING)==0 &&
   	 			adcData_A>adcRef_A+adcDeadband_A && adcData_B>adcRef_B+adcDeadband_B &&
   	 			adcUpCount_A>=upCountRef && adcUpCount_B>=upCountRef) {

			if (adcUpCount_A>adcUpCount_B) {
				stepA_B();
			} else {
				stepB_A();
			}

			calcUpCountRef(adcUpCount_A+adcUpCount_B);
			state |= S_STEPPING;
   	  	}


    	++loopCount;

#ifdef SER_DEBUG_OUT
  	  	sendPut('\n');
#endif
#ifdef PIN_DEBUG_OUT
   		P1OUT &= ~TEST1_OUT;
#endif

    }  // while
}



#pragma vector=TIMER0_A0_VECTOR
__interrupt void timer0_A0_isr (void) {
//	P1OUT ^= TEST1_OUT;
	_bic_SR_register_on_exit(CPUOFF);   // trigger code execution in main loop
}

#pragma vector=ADC10_VECTOR
__interrupt void adc10_isr (void)
{
	if (state&S_ADC_IN_A) {
		adcData_A = ADC10MEM;
	} else {
		adcData_B = ADC10MEM;
	}
  	_bic_SR_register_on_exit(CPUOFF);	// trigger code execution in main loop
}



/*
 *
// 	  		if (adcUpCount_A>0) printf("Ab:%i:%u:%u ", adcData_B-adcRef_B, adcUpCount_A, adcUpCount_B);  /////////////////

   	  	if (adcData_A>adcRef_A+DEADBAND) {
   	  		if (adcUpCount_A>COUNT_MAX) {
//   	  			printf("AU:%u:%u ", adcData_A, adcRef_A);
   	  			++adcRef_A;
   	  			adcUpCount_A=0;
   	  		} else {
   	  			++adcUpCount_A;
   	  		}
   	  		adcDownCount_A=0;
   	  	} else if (adcData_A<adcRef_A-DEADBAND*2) {
   	  		if (adcDownCount_A>COUNT_MAX) {
 //  	  			printf("AD:%u:%u ", adcData_A, adcRef_A);
   	  			--adcRef_A;
   	  			adcDownCount_A=0;
   	  		} else {
   	  			++adcDownCount_A;
   	  		}
  	  		adcUpCount_A=0;
   	  	} else {
   	  		adcUpCount_A=0;
   	  		adcDownCount_A=0;
   	  	}

   	  	if (adcData_B>adcRef_B+DEADBAND) {
   	  		if (adcUpCount_B>COUNT_MAX) {
//   	  			printf("BU:%u ", adcUpCount_B);
   	  			++adcRef_B;
   	  			adcUpCount_B=0;
   	  		} else {
   	  			++adcUpCount_B;
   	  		}
   	  		adcDownCount_B=0;
   	  	} else if (adcData_B<adcRef_B-DEADBAND*2) {
   	  		if (adcDownCount_B>COUNT_MAX) {
//   	  			printf("BD:%u ", adcDownCount_B);
   	  			--adcRef_B;
   	  			adcDownCount_B=0;
   	  		} else {
   	  			++adcDownCount_B;
   	  		}
  	  		adcUpCount_B=0;
   	  	} else {
   	  		adcUpCount_B=0;
   	  		adcDownCount_B=0;
   	  	}


 *
*/
