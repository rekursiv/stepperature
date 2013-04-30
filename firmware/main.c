/*
 * Copyright 2013 ProtoPlant INC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include <msp430.h>
#include "serial.h"
#include "state.h"
#include "config.h"
//#include "debug.h"


volatile unsigned int adcData_A = 0;
volatile unsigned int adcData_B = 0;
unsigned int adcRef_A = REF_CENTER;
unsigned int adcRef_B = REF_CENTER;

unsigned int adcUpCount_A = 0;
unsigned int adcUpCount_B = 0;
unsigned char upCountRef = UP_COUNT_MAX;

int adcOffsetAdj_A = 0;
int adcOffsetAdj_B = 0;

unsigned char ticksSinceLastStep = 0;
unsigned int ticksBtnDown = 0;

#ifdef DEBUG
unsigned char stepDebug = 0;
#endif

// flash segment D begins at 0x1000
#define FLASH_STATE		*(char*)	0x1000
#define FLASH_ADCREF_A	*(int*)		0x1002
#define FLASH_ADCREF_B	*(int*)		0x1004
#define FLASH_PW		*(int*)		0x1006



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

void inline go16xFaster() {
	BCSCTL1 = CALBC1_16MHZ;         // set clock speed
	DCOCTL = CALDCO_16MHZ;
}

void inline adcTriggerSample(unsigned const int chan) {
	ADC10CTL0 &= ~ENC;				// Disable ADC
	ADC10CTL1 &= 0x0FFF;  			// clear input channel bits
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
		serialEnable();
		_enable_interrupts();
	} else {
		_disable_interrupts();
		serialDisable();
		_enable_interrupts();
	}
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
	else if ((state&S_SM_MASK)==S_SM_SERIAL) {
#ifdef DEBUG
		++stepDebug;
#else
		serialSendSingleByte('+');
#endif
	}
}

void inline stepB_A() {  // step B leading A
	if ((state&S_SM_MASK)==S_SM_QUAD) decQuad();
	else if ((state&S_SM_MASK)==S_SM_SD) P1OUT&=~B_OUT;
	else if ((state&S_SM_MASK)==S_SM_SASB) P1OUT|=B_OUT;
	else if ((state&S_SM_MASK)==S_SM_SERIAL) {
#ifdef DEBUG
		--stepDebug;
#else
		serialSendSingleByte('-');
#endif
	}
}

void inline stepReset() {
	if (((state&S_SM_MASK)==S_SM_SD)||((state&S_SM_MASK)==S_SM_SASB)) {
  		P1OUT&=~A_OUT;
  		P1OUT&=~B_OUT;
	}
}

void inline calcAdcRef_A() {
	adcOffsetAdj_A+=adcRef_A-adcData_A;
	if (adcOffsetAdj_A<-ADJUST_DELAY) {
		++adcRef_A;
		adcOffsetAdj_A=0;
	} else if (adcOffsetAdj_A>ADJUST_DELAY) {
		--adcRef_A;
		adcOffsetAdj_A=0;
	}
}

void inline calcAdcRef_B() {
	adcOffsetAdj_B+=adcRef_B-adcData_B;
	if (adcOffsetAdj_B<-ADJUST_DELAY) {
		++adcRef_B;
		adcOffsetAdj_B=0;
	} else if (adcOffsetAdj_B>ADJUST_DELAY) {
		--adcRef_B;
		adcOffsetAdj_B=0;
	}
}

void inline calcUpCountRef() {
	upCountRef=ticksSinceLastStep>>3;  //  /8
	if (upCountRef<UP_COUNT_MIN) upCountRef=UP_COUNT_MIN;
	else if (upCountRef>UP_COUNT_MAX) upCountRef=UP_COUNT_MAX;
}

void inline readFlash() {
	// MPU ships with random data in flash
	// so we check for password to prevent
	// reading corrupt values
	if (FLASH_PW==0xBEEF) {
		setStepMode(FLASH_STATE);
		adcRef_A = FLASH_ADCREF_A;
		adcRef_B = FLASH_ADCREF_B;
	}
}

void inline writeFlash() {
	 WDTCTL = WDTPW|WDTHOLD;	// Stop watchdog
	_disable_interrupts();

	FCTL2 = FWKEY|FSSEL1|FN4|FN5;	// MCLK/48 @ 16mhz = 333kHz
	FCTL3 = FWKEY;					// Clear LOCK
	FCTL1 = FWKEY|ERASE;
	FLASH_STATE = 0;				// Initiate erase
	while(FCTL3&BUSY);

	FCTL3 = FWKEY;					// Clear LOCK
	FCTL1= FWKEY|WRT;               // Write enable

	// write data
	FLASH_STATE = state&S_SM_MASK;	// only save step mode
	while(FCTL3&BUSY);
	FLASH_ADCREF_A = adcRef_A;
	while(FCTL3&BUSY);
	FLASH_ADCREF_B = adcRef_B;
	while(FCTL3&BUSY);
	FLASH_PW = 0xBEEF;
	while(FCTL3&BUSY);

    FCTL1= FWKEY;					// Disable write
    FCTL3= FWKEY|LOCK;              // Set LOCK

	_enable_interrupts();
	WDTCTL = WDTPW|WDTCNTCL;	// Restart watchdog
}




int main(void) {
	WDTCTL = WDTPW|WDTCNTCL;   // throw the dog a bone

    // set stuff up
    readFlash();
    initOuts();
    initAdc();
    initTimer();
    _enable_interrupts();

#ifdef DEBUG
	setStepMode(S_SM_SERIAL);
#endif

    while(1) {         //  main loop

    	WDTCTL = WDTPW|WDTCNTCL;	// dog needs attention on every tick, else RESET
    	_bis_SR_register(CPUOFF);  	// wait for timer

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
   	  			state|=S_BTN_DN;
   	  			P1OUT&=~A_OUT;
   	  			P1OUT&=~B_OUT;
   	  		}

			// button is considered "held" after 1 second
			if ((state&S_BTN_HELD)) {
				if (ticksBtnDown%BTN_HELD_BLINK_DELAY==0) {
	   	  			P1OUT^=A_OUT;
	   	  			P1OUT^=B_OUT;
				}
			} else {
				if (ticksBtnDown==SAMPLE_RATE) {
					writeFlash();
					state|=S_BTN_HELD;
					P1OUT|=A_OUT;
				}
			}
			++ticksBtnDown;

   	  		continue;		//  BYPASS the rest of the loop while button is down
   	  	} else {
   	  		if ((state&S_BTN_DN)) {
  	  			state&=~S_BTN_DN;
  	  			P1OUT&=~A_OUT;
  	  			P1OUT&=~B_OUT;
  	  			ticksBtnDown = 0;
  	  			if ((state&S_BTN_HELD)) state&=~S_BTN_HELD;
  	  			else incStepMode();
   	  		}
   	  	}

   	  	// maintain running average of sample data
   	 	calcAdcRef_A();
   	 	calcAdcRef_B();

   	  	// count "high" samples
   	  	if (adcData_A>adcRef_A+DEADBAND) ++adcUpCount_A;
   	  	else if (adcData_A<adcRef_A) adcUpCount_A=0;
   	  	if (adcData_B>adcRef_B+DEADBAND) ++adcUpCount_B;
   	  	else if (adcData_B<adcRef_B) adcUpCount_B=0;

   	   	// handle step output for step/dir mode
 	    if ((state&S_STEPPING)&&(state&S_SM_MASK)==S_SM_SD&&(P1OUT&A_OUT)==0) P1OUT|=A_OUT;

   	  	// reset prev step
   	 	if ((state&S_STEPPING) && adcUpCount_A==0 && adcUpCount_B==0) {
  	  		stepReset();
   	  		state &= ~S_STEPPING;
   	  		ticksSinceLastStep = 0;
   		}

     	// detect step
   	 	if ((state&S_STEPPING)==0 &&
   	 			adcData_A>adcRef_A+DEADBAND && adcData_B>adcRef_B+DEADBAND &&
   	 			adcUpCount_A>=upCountRef && adcUpCount_B>=upCountRef) {

			if (adcUpCount_A>adcUpCount_B) stepA_B();
			else stepB_A();

			calcUpCountRef();
			state |= S_STEPPING;
   	  	}

   	 	if (ticksSinceLastStep<255) ++ticksSinceLastStep;


#ifdef DEBUG
    	serialBegin();
   	 	serialAddByte(stepDebug);
//   	 	serialAddInt(adcData_A);
//   	 	serialAddInt(adcData_B);
//   	 	serialAddInt(adcUpCount_A);
//    	serialAddByte(upCountRef);
    	serialEnd();
#endif


    }  // while
}



#pragma vector=TIMER0_A0_VECTOR
__interrupt void timer0_A0_isr (void) {
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
