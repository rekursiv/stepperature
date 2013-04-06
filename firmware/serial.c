#include <msp430.h>
#include "serial.h"



void serialSendByte(const unsigned char b) {
	USISRL = b << 1;  //  add start bit = 0
	USISRH = (b >> 7) | 0x02;  //  add leftover bit from above and stop bit = 1
	USICNT = 10 | USI16B;  // 1 start bit + 1 byte + 1 stop bit = 10 bits
}

// disable GIE before calling
void serialEnable() {
	USICTL0 = USISWRST | USIOE | USIMST | USIPE6 | USILSB;
	USICTL1 = USIIE;  //  interrupt enable
	USICKCTL = USISSEL_5 | USIDIV_1;   //  use TimerA, divide by two
	USICTL0 &= ~USISWRST;
}

// disable GIE before calling
void serialDisable() {
	USICTL0 = USISWRST;
	USICTL1 = 0;
	USICKCTL = 0;
}


// sends a byte out of the serial port immediately if the port is not being used
// if the port is being used, does nothing
void serialPutc(const unsigned char c) {
	if (!(USICTL1&USIIFG)) {  // port is available if interrupt flag is NOT set
		serialSendByte(c);
	}
}


#pragma vector=USI_VECTOR
__interrupt void usi_isr (void) {
	// TODO:  if using buffer, call buffer callback
	USICTL1 &= ~USIIFG;
}
