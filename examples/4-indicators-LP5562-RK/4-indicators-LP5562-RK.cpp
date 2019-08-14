#include "LP5562-RK.h"

SYSTEM_THREAD(ENABLED);

LP5562 ledDriver;

const unsigned long CHANGE_INTERVAL_MS = 10000;
unsigned long lastChange = 0;
int testNum = 0;

void setup() {
	// LED   Color Name   Actual Color   Current
	// 1     Red          Red            20mA
	// 2     Green        Green          20mA
	// 3     Blue         Yellow         20mA
	// 4     White        Red            10mA
	ledDriver.withLEDCurrent(20.0, 20.0, 20.0, 10.0).begin();

	ledDriver.setIndicatorMode();
}

void loop() {
	if (millis() - lastChange >= CHANGE_INTERVAL_MS) {
		lastChange = millis();

		switch(testNum++) {
		case 0:
			// All blinking
			ledDriver.setLedMappingR(LP5562::REG_LED_MAP_ENGINE_1);
			ledDriver.setLedMappingG(LP5562::REG_LED_MAP_ENGINE_1);
			ledDriver.setLedMappingB(LP5562::REG_LED_MAP_ENGINE_1);
			ledDriver.setLedMappingW(LP5562::REG_LED_MAP_ENGINE_1);
			break;

		case 1:
			// Fast blink red
			ledDriver.setLedMappingR(LP5562::REG_LED_MAP_ENGINE_2);
			break;

		case 2:
			// Breathe all
			ledDriver.setLedMappingR(LP5562::REG_LED_MAP_ENGINE_3);
			ledDriver.setLedMappingG(LP5562::REG_LED_MAP_ENGINE_3);
			ledDriver.setLedMappingB(LP5562::REG_LED_MAP_ENGINE_3);
			ledDriver.setLedMappingW(LP5562::REG_LED_MAP_ENGINE_3);
			break;

		case 3:
			// Off - Blink - Fast Blink - Breathe
			ledDriver.setLedMappingR(LP5562::REG_LED_MAP_DIRECT, 0);
			ledDriver.setLedMappingG(LP5562::REG_LED_MAP_ENGINE_1);
			ledDriver.setLedMappingB(LP5562::REG_LED_MAP_ENGINE_2);
			ledDriver.setLedMappingW(LP5562::REG_LED_MAP_ENGINE_3);
			break;

		case 4:
			// On - Blink - Fast Blink - Breathe
			ledDriver.setLedMappingR(LP5562::REG_LED_MAP_DIRECT, 255);
			ledDriver.setLedMappingG(LP5562::REG_LED_MAP_ENGINE_1);
			ledDriver.setLedMappingB(LP5562::REG_LED_MAP_ENGINE_2);
			ledDriver.setLedMappingW(LP5562::REG_LED_MAP_ENGINE_3);
			break;

		case 5:
			// Dim - Blink - Off - Fast Blink
			ledDriver.setLedMappingR(LP5562::REG_LED_MAP_DIRECT, 64);
			ledDriver.setLedMappingG(LP5562::REG_LED_MAP_ENGINE_1);
			ledDriver.setLedMappingB(LP5562::REG_LED_MAP_DIRECT, 0);
			ledDriver.setLedMappingW(LP5562::REG_LED_MAP_ENGINE_2);
			break;

		case 6:
			// All on full
			ledDriver.setLedMappingR(LP5562::REG_LED_MAP_DIRECT, 255);
			ledDriver.setLedMappingG(LP5562::REG_LED_MAP_DIRECT, 255);
			ledDriver.setLedMappingB(LP5562::REG_LED_MAP_DIRECT, 255);
			ledDriver.setLedMappingW(LP5562::REG_LED_MAP_DIRECT, 255);

			// This is the last test - go back to the first one
			testNum = 0;
			break;

		}

	}
}
