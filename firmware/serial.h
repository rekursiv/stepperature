

#ifndef SERIAL_H_
#define SERIAL_H_

void serialEnable();
void serialDisable();


// sends a byte out of the serial port immediately if the port is not being used
// if the port is being used, does nothing
void serialPutc(const unsigned char);



#endif /* SERIAL_H_ */
