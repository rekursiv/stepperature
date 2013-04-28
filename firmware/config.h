
#ifndef CONFIG_H_
#define CONFIG_H_


#define CLOCK_RATE 16000000			// 16 MHz
#define SAMPLE_RATE 19200			// in Hz (double the baud rate of the serial port)
#define BTN_HELD_BLINK_DELAY 2048	// in sample ticks
#define DEADBAND 20					// amount of "hysteresis" used when determining top vs. bottom half of waveform
#define UP_COUNT_MIN 2				// how high to count before triggering a step when waveform is moving quickly
#define UP_COUNT_MAX 20				// how high to count before triggering a step when waveform is moving slowly
#define ADJUST_DELAY 2000			// how far off centerline the averaging sum has to be before adjustment is made (smaller adjusts faster)
#define REF_CENTER 540  			// approx. center point of waveform to use as starting point (will adjust itself)


#define A_OUT BIT6
#define B_OUT BIT7

#define A_IN INCH_2
#define B_IN INCH_5
#define VCC_IN INCH_11



#endif /* CONFIG_H_ */
