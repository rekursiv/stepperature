#include <msp430.h>
#include <string.h>
#include "stdarg.h"
#include "printf.h"


char buf[PUT_BUF_LEN];
volatile unsigned char bufPos = 0;

void initPut() {
	if (bufPos==0) bufPos=255;
}

void sendPut(const unsigned char end) {
	if (bufPos==255&&buf[0]!='\0') {
		if (end!=0) putc(end);
		bufPos=0;
		UC0IFG |= UCA0TXIFG;
	}
}

void initSerial() {
	buf[0]='\0';
	bufPos = 0;

    P1SEL = BIT1 + BIT2; // P1.1=RXD, P1.2=TXD
    P1SEL2 = BIT1 + BIT2; // P1.1=RXD, P1.2=TXD

    UCA0CTL1 |= UCSSEL_2; // use SMCLK for USCI clock
    UCA0BR0 = 130; // 16MHz 9600
    UCA0BR1 = 6; // 16MHz 9600
    UCA0MCTL = UCBRS1 + UCBRS0; // Modulation UCBRSx = 3
    UCA0CTL1 &= ~UCSWRST; // **Initialize USCI state machine**
    UC0IE |= UCA0TXIE;
}


void puts(const char *str) {
	if (bufPos!=255) return;
	if (strlen(str)+strlen(buf)+1 < PUT_BUF_LEN) strcat(buf, str);
}

void putc(const unsigned char c) {
	if (bufPos!=255) return;
	unsigned char len = strlen(buf);
	if (len+1 < PUT_BUF_LEN) {
		buf[len+1]='\0';
		buf[len]=c;
	}
}

void cls(){printf("%c[2J%c[;H", (char)27, (char)27);}


static const unsigned long dv[] = {
//  4294967296      // 32 bit unsigned max
    1000000000,     // +0
     100000000,     // +1
      10000000,     // +2
       1000000,     // +3
        100000,     // +4
//       65535      // 16 bit unsigned max     
         10000,     // +5
          1000,     // +6
           100,     // +7
            10,     // +8
             1,     // +9
};

static void xtoa(unsigned long x, const unsigned long *dp)
{
    char c;
    unsigned long d;
    if(x) {
        while(x < *dp) ++dp;
        do {
            d = *dp++;
            c = '0';
            while(x >= d) ++c, x -= d;
            putc(c);
        } while(!(d & 1));
    } else
        putc('0');
}

static void puth(unsigned n)
{
    static const char hex[16] = { '0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};
    putc(hex[n & 15]);
}

void printf(char *format, ...)
{
	if (bufPos!=255) return;

    char c;
    int i;
    long n;
    
    va_list a;
    va_start(a, format);
    while(c = *format++) {
        if(c == '%') {
            switch(c = *format++) {
                case 's':                       // String
                    puts(va_arg(a, char*));
                    break;
                case 'c':                       // Char
                    putc(va_arg(a, char));
                    break;
                case 'i':                       // 16 bit Integer
                case 'u':                       // 16 bit Unsigned
                    i = va_arg(a, int);
                    if(c == 'i' && i < 0) i = -i, putc('-');
                    xtoa((unsigned)i, dv + 5);
                    break;
                case 'l':                       // 32 bit Long
                case 'n':                       // 32 bit uNsigned loNg
                    n = va_arg(a, long);
                    if(c == 'l' &&  n < 0) n = -n, putc('-');
                    xtoa((unsigned long)n, dv);
                    break;
                case 'x':                       // 16 bit heXadecimal
                    i = va_arg(a, int);
                    puth(i >> 12);
                    puth(i >> 8);
                    puth(i >> 4);
                    puth(i);
                    break;
                case 0: return;
                default: goto bad_fmt;
            }
        } else
bad_fmt:    putc(c);
    }
    va_end(a);
}







#pragma vector=USCIAB0TX_VECTOR
__interrupt void usciab0_isr (void) {

	if (bufPos>PUT_BUF_LEN || buf[bufPos]=='\0') {
		bufPos=0;
		buf[0]='\0';
		UC0IFG &= ~UCA0TXIFG;
	} else {
		UCA0TXBUF=buf[bufPos];
		++bufPos;
	}

}
