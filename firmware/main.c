#include <msp430.h>
#include "serial.h"
#include "state.h"
#include "config.h"
//#include "debug.h"



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


void inline initOuts() {
    P1DIR |= (A_OUT + B_OUT);
    P1OUT &= ~A_OUT;
    P1OUT &= ~B_OUT;
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
	TA0CCTL0 = CCIE | OUTMOD1;					// capture compare interrupt enable, out mode 1
	TA0CTL = TASSEL_2+MC_1; 					// use SMCLK, count UP
}


// sorry, it only works once...
void inline go16xFaster() {
	BCSCTL1 = CALBC1_16MHZ;         // set clock speed
	DCOCTL = CALDCO_16MHZ;
}


void inline adcTriggerSample(unsigned const int chan) {
	ADC10CTL0 &= ~ENC;				// Disable ADC
	ADC10CTL1 &= 0x0fff;  			// clear input channel bits
	ADC10CTL1 |= chan;    			// set input channel bits
	ADC10CTL0 |= ENC + ADC10SC; 	// Enable and start conversion
}


unsigned char inline getStepMode() {
	return (state&S_SM_MASK);
}

void inline setStepMode(const unsigned char mode) {
	state &= ~S_SM_MASK;
	state |= (mode&S_SM_MASK);
	if ((state&S_SM_MASK)==S_SM_SERIAL) {
		_disable_interrupts();
		P1OUT&=~A_OUT;
		serialEnable();
		_enable_interrupts();
	} else {
		_disable_interrupts();
		serialDisable();
		_enable_interrupts();
	}
	//  TODO: store state in flash, load on boot
}

void inline incStepMode() {
	if (getStepMode()==S_SM_LAST) setStepMode(0);
	else setStepMode(getStepMode()+1);
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
	else if ((state&S_SM_MASK)==S_SM_SERIAL) serialPutc('+');
}

void inline stepB_A() {  // step B leading A
	if ((state&S_SM_MASK)==S_SM_QUAD) decQuad();
	else if ((state&S_SM_MASK)==S_SM_SD) P1OUT&=~B_OUT;
	else if ((state&S_SM_MASK)==S_SM_SASB) P1OUT|=B_OUT;
	else if ((state&S_SM_MASK)==S_SM_SERIAL) serialPutc('-');
}

void inline stepReset() {
	if (((state&S_SM_MASK)==S_SM_SD)||((state&S_SM_MASK)==S_SM_SASB)) {
  		P1OUT&=~A_OUT;
  		P1OUT&=~B_OUT;
	}
}

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
	WDTCTL = WDTPW+WDTCNTCL;   // throw the dog a bone

//	setStepMode(S_SM_SERIAL);   //  //  //  TEST

    // set stuff up
    initOuts();    //////
    initAdc();
    initTimer();
    _enable_interrupts();

    while(1) {         //  main loop

    	WDTCTL = WDTPW+WDTCNTCL; // don't forget to pet the dog

    	_bis_SR_register(CPUOFF);  // wait for timer


#ifdef DEBUG
    	debugBegin();
#endif


    	// wait until we have enough voltage to run at 16mhz
    	if (!(state&S_16MHZ)) {
         	state |= S_ADC_IN_A;
        	adcTriggerSample(VCC_IN);  // sample power supply voltage
         	_bis_SR_register(CPUOFF);   // wait for sample
         	if (adcData_A > 615) { // check if we have at least 3v at VCC
         		state |= S_16MHZ;
       	  	    go16xFaster();  // OK let's go fast now
         	}
         	continue;  //  BYPASS the rest of the loop until we're running fast enough
    	}

    	// handle step output for step/dir mode
    	if ((state&S_STEPPING)&&(state&S_SM_MASK)==S_SM_SD&&(P1OUT&A_OUT)==0) P1OUT|=A_OUT;

    	//  acquire data from ADC
     	state |= S_ADC_IN_A;
    	adcTriggerSample(A_IN);
     	_bis_SR_register(CPUOFF);   // wait for sample A
     	state &= ~S_ADC_IN_A;
   		adcTriggerSample(B_IN);
   	  	_bis_SR_register(CPUOFF);   // wait for sample B


   	  	// handle button press
   	  	if (adcData_A<100) {
   	  		if (!(state&S_BTN_DN)) {
   	  			state |= S_BTN_DN;
   	  		}
   	  		continue;
   	  	} else {
   	  		if ((state&S_BTN_DN)) {
  	  			state &= ~S_BTN_DN;
  	  			incStepMode();
   	  		}
   	  	}

   	  	calcRunningAverage();

//   	  	printf("%u:%u", adcRef_A, adcRef_B);


//   	  	if (state&S_TEST) printf(" %u:%u ", adcData_A, adcData_B);
   	  		//printf("  %u", loopCount);
//   	  	if (loopCount>10000) state&=~S_TEST;

//

/*
  		if (loopCount%0x2000==0) {
//  			printf("D:%u:%u ", adcDeadband_A, adcDeadband_B);
  			if (adcDeadband_A>DEADBAND_MIN) --adcDeadband_A;
  			if (adcDeadband_B>DEADBAND_MIN) --adcDeadband_B;
   	  	}
*/



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


#ifdef DEBUG
    	debugEnd();
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
