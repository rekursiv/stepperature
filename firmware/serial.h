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

void inline serialAddInt(const unsigned int data) {
	serialAddByte((unsigned char)(data>>8));
	serialAddByte((unsigned char)data);
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
