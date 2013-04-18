#include <msp430.h>
#include "serial.h"


unsigned char serialBuf[SERIAL_BUF_LEN];
volatile unsigned char serialBufPos = SERIAL_BUF_LEN;



// disable GIE before calling
void serialEnable() {
	serialBufPos = SERIAL_BUF_LEN;
	USICTL0 = USISWRST | USIOE | USIMST | USIPE6 | USILSB;
	USICTL1 = USIIE;  //  interrupt enable
	USICKCTL = USISSEL_5 | USIDIV_1;   //  use TimerA, divide by two
	USICTL0 &= ~USISWRST;

}

// disable GIE before calling
void serialDisable() {
	serialBufPos = SERIAL_BUF_LEN;
	USICTL0 = USISWRST;
	USICTL1 = 0;
	USICKCTL = 0;
}


void inline serialLoadRegister(const unsigned char b) {
	USISRL = b << 1;  //  add start bit = 0
	USISRH = (b >> 7) | 0x02;  //  add leftover bit from above and stop bit = 1
	USICNT = 10 | USI16B;  // 1 start bit + 1 byte + 1 stop bit = 10 bits
}


#pragma vector=USI_VECTOR
__interrupt void usi_isr (void) {
	if (serialBufPos<SERIAL_BUF_LEN) {
		serialLoadRegister(serialBuf[serialBufPos]);
		++serialBufPos;
	} else {
		USICTL1 &= ~USIIFG;
		serialBufPos = 255;
//		serialBufReadable = 1;
	}
}

