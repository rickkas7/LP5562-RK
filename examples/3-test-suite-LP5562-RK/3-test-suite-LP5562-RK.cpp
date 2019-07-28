#include "LP5562-RK.h"

SYSTEM_THREAD(ENABLED);

SerialLogHandler logHandler;

//                         red       green     blue      yellow    cyan      magenta   white
uint32_t testColors[7] = { 0xFF0000, 0x00FF00, 0x0000FF, 0xFFFF00, 0x00FFFF, 0xFF00FF, 0xFFFFFF };

LP5562 ledDriver;
const unsigned long TEST_PERIOD_MS = 10000;
unsigned long testStartMs = 1000 - TEST_PERIOD_MS;
int testNum = 0;

void setup() {
	ledDriver.withLEDCurrent(5.0).begin();
}

void loop() {
	if (millis() - testStartMs >= TEST_PERIOD_MS) {
		testStartMs = millis();
		switch(testNum) {
		case 0: // red
		case 1: // green
		case 2: // blue
		case 3: // yellow
		case 4: // cyan
		case 5: // magenta
		case 6: // white
			// Main solid colors
			ledDriver.useDirectRGB();
			ledDriver.setRGB(testColors[testNum]);
			break;

		case 7:
			// Blink fast red
			ledDriver.setBlink(255, 0, 0, 100, 100);
			break;

		case 8:
			// Blink green
			ledDriver.setBlink(0, 255, 0, 500, 500);
			break;

		case 9:
			// Blink slow blue
			ledDriver.setBlink(0, 0, 255, 1000, 1000);
			break;

		case 10:
			// Alternate red/blue
			ledDriver.setBlink2(0xff0000, 500, 0x0000ff, 500);
			break;

		case 11:
			// Alternate cyan yellow slow
			ledDriver.setBlink2(0x00FFFF, 2000, 0xFFFF00, 2000);
			break;

		case 12:
			// Breathe cyan
			ledDriver.setBreathe(false, true, true, 20, 0, 255);
			break;

		case 13:
			// Breathe magenta
			ledDriver.setBreathe(true, false, true, 20, 0, 255);
			break;

		case 14:
			// Breathe red fast, partial ramp half to full brightness (never off)
			ledDriver.setBreathe(true, false, false, 10, 128, 255);
			break;

		case 15:
			// Quick blink white with 4 seconds between blinks
			ledDriver.setBlink(255, 255, 255, 100, 4000);
			break;

		default:
			// Start tests over from beginning. -1 because of the increment below.
			testNum = -1;
			break;
		}

		// Go to next test
		Log.info("running test %d", testNum);
		testNum++;
	}
}
