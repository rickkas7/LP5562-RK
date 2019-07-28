#include "LP5562-RK.h"

SYSTEM_THREAD(ENABLED);

LP5562 ledDriver;

void setup() {
	ledDriver.withLEDCurrent(5.0).begin();

	// ledDriver.setBlink(255, 255, 255, 500, 500);
	// ledDriver.setBlink2(0xff0000, 500, 0x0000ff, 500);
	ledDriver.setBreathe(false, true, true, 20, 0, 255);
}

void loop() {
}
