#include "LP5562-RK.h"

SYSTEM_THREAD(ENABLED);

SerialLogHandler logHandler;

//                         red       green     blue      yellow    cyan      magenta   white
uint32_t testColors[7] = { 0xFF0000, 0x00FF00, 0x0000FF, 0xFFFF00, 0x00FFFF, 0xFF00FF, 0xFFFFFF };

LP5562 ledDriver;

const unsigned long UPDATE_PERIOD_MS = 1000;
unsigned long lastUpdate = 0;
size_t curColor = 0;
bool whiteState = false;

void setup() {
	// Wait for a USB serial connection for up to 10  seconds
	waitFor(Serial.isConnected, 10000);

	ledDriver.withLEDCurrent(5.0).begin();
}

void loop() {
	if (millis() - lastUpdate >= UPDATE_PERIOD_MS) {
		lastUpdate = millis();

		ledDriver.setRGB(testColors[curColor]);
		if (++curColor >= (sizeof(testColors) / sizeof(testColors[0]))) {
			curColor = 0;

			ledDriver.setW(whiteState ? 255 : 0);
			whiteState = !whiteState;
		}
	}
}
