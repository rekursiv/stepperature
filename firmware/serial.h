

#ifndef SERIAL_H_
#define SERIAL_H_


#define SERIAL_BUF_LEN 1

extern unsigned char serialBuf[];
extern volatile unsigned char serialBufPos;


void serialEnable();
void serialDisable();


void inline serialBegin() {
	if (serialBufPos==255) serialBufPos = 128;
}

void inline serialAddByte(const unsigned char c) {
	if (serialBufPos>=128 && (serialBufPos-128)<SERIAL_BUF_LEN ) {
		serialBuf[serialBufPos-128] = c;
		serialBufPos++;
	}
}

void inline serialEnd() {
	if (serialBufPos>128 && serialBufPos!=255) {
		serialBufPos=0;
		USICTL1 |= USIIFG;
	}
}

void inline serialSendSingleByte(const unsigned char c) {
	if (serialBufPos==255) {
		serialBegin();
		serialAddByte(c);
		serialEnd();
	}
}



#endif /* SERIAL_H_ */
