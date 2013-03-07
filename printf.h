



#define PUT_BUF_LEN 64


void initSerial();
void initPut();
void sendPut(const unsigned char);

void putc(const unsigned char);
void puts(const char *);
void printf(char *, ...);

// clear screen in ANSI terminal
void cls();

