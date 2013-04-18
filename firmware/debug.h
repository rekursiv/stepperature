
#ifndef DEBUG_H_
#define DEBUG_H_

#define DEBUG

#undef SERIAL_BUF_LEN
#define SERIAL_BUF_LEN 2

//#define PIN_DEBUG_OUT BIT7   // output B

//#define USE_SERIAL
//#define SER_STEP_OUT
//#define SER_DEBUG_OUT
/////



void debugBegin();
void debugEnd();
void debugChar(const unsigned char);
//void debugByte(const char);

#endif /* DEBUG_H_ */
