// Repository: https://github.com/rickkas7/LP5562-RK
// License: MIT

#include "LP5562-RK.h"

LP5562::LP5562(uint8_t addr, TwoWire &wire) : addr(addr), wire(wire) {
	if (addr < 0x4) {
		// Just passed in 0 - 3, add in the 0x30 automatically to make addresses 0x30 - 0x33
		addr |= 0x30;
	}
}

LP5562::~LP5562() {

}


LP5562 &LP5562::withLEDCurrent(float red, float green, float blue, float white) {
	redCurrent = floatToCurrent(red);
	greenCurrent = floatToCurrent(green);
	blueCurrent = floatToCurrent(blue);
	whiteCurrent = floatToCurrent(white);

	return *this;
}

uint8_t LP5562::floatToCurrent(float value) const {
	if (value < 0) {
		value = 0;
	}
	if (value > 25.5) {
		value = 25.5;
	}
	return (uint8_t) (value * 10);
}

bool LP5562::begin() {
	// Initialize the I2C bus in standard master mode.
	wire.begin();

	// Reset chip - reset all registers to default values. Note that resetting the MCU won't reset
	// the values in the chip, so it's a good idea to do this in begin().
	bool bResult = writeRegister(REG_RESET, 0xff);
	if (!bResult) {
		return false;
	}

	// Set current level. The hardware default is 17.8 mA, but we default to 5 mA in software. You can
	// override this with the withLEDCurrent methods. Be sure to do this before enabling the chip!
	bResult = writeRegister(REG_R_CURRENT, redCurrent);
	if (!bResult) {
		return false;
	}

	bResult = writeRegister(REG_G_CURRENT, greenCurrent);
	if (!bResult) {
		return false;
	}

	bResult = writeRegister(REG_B_CURRENT, blueCurrent);
	if (!bResult) {
		return false;
	}

	bResult = writeRegister(REG_W_CURRENT, whiteCurrent);
	if (!bResult) {
		return false;
	}

	// Set the default PWM levels to 0 initially
	(void) writeRegister(REG_R_PWM, 0x00);
	(void) writeRegister(REG_G_PWM, 0x00);
	(void) writeRegister(REG_B_PWM, 0x00);
	(void) writeRegister(REG_W_PWM, 0x00);

	// Enable the chip
	uint8_t value = REG_ENABLE_CHIP_EN;
	if (useLogarithmicMode) {
		value |= REG_ENABLE_LOG_EN;
	}

	bResult = writeRegister(REG_ENABLE, value);
	if (!bResult) {
		return false;
	}

	// Hardware start-up delay
	delayMicroseconds(500);

	// Enable clock
	value = 0x00;
	if (!useExternalOscillator) {
		value |= REG_CONFIG_INT_CLK_EN;
	}
	if (highFrequencyMode) {
		value |= REG_CONFIG_HF;
	}
	bResult = writeRegister(REG_CONFIG, value);
	if (!bResult) {
		return false;
	}

	// Enable direct PWM control for all LEDs by default
	bResult = writeRegister(REG_LED_MAP, 0x00);
	if (!bResult) {
		return false;
	}

	return true;
}

#ifdef ENABLE_TESTPGM

void LP5562::testPgm1() {

	// This is the test program from the datasheet

	// Write to address 01h 0001 0000b (configure engine 1 into 'Load program to SRAM' mode)
	writeRegister(REG_OP_MODE, 0b00010000); // 0x10

	// Ramp prescale=0 stepTime=3 sign=0 increment=7f
	writeRegister(0x10, 0b00000011); // 1st ramp command 8 MSB 0x037f
	writeRegister(0x11, 0b01111111); // 1st ramp command 8 LSB

	// Wait prescale=1 stepTime=13 sign=0 increment=0 (wait)
	writeRegister(0x12, 0b01001101); // 1st wait command 8 MSB 0x4d00
	writeRegister(0x13, 0b00000000); // 1st wait command 8 LSB

	// Ramp prescale=0 stepTime=3 sign=1 increment=7f
	writeRegister(0x14, 0b00000011); // 2nd ramp command 8 MSB 0x03ff
	writeRegister(0x15, 0b11111111); // 2nd ramp command 8 LSB

	// Wait prescale=1 stepTime=32 sign=0 increment=0 (wait)
	writeRegister(0x16, 0b01100000); // 2nd wait command 8 MSB 0x6000
	writeRegister(0x17, 0b00000000); // 2nd wait command 8 LSB

	// Configure LED controller operation mode to "Run program" in engine 1
	writeRegister(REG_OP_MODE, 0b00100000);

	// Configure program execution mode from "Hold" to "Run" in engine 1
	writeRegister(REG_ENABLE, 0b01100000);

	// Set red, green, and blue LEDs to program 1
	writeRegister(REG_LED_MAP, 0b00010101);
}

void LP5562::testPgm2() {
	// This is the same as testPgm1, but implemented using the LP5562Program class.

	// Clear any existing programs and put the engines into hold mode.
	clearAllPrograms();

	LP5562Program program;

	program.addCommandRamp(false, 3, false, 0x7f);

	program.addCommandWait(true, 13);

	program.addCommandRamp(false, 3, true, 0x7f);

	program.addCommandWait(true, 32);

	setProgram(1, program, true);

	// Set red, green, and blue LEDs to program 1
	setLedMapping(REG_LED_MAP_ENGINE_1, REG_LED_MAP_ENGINE_1, REG_LED_MAP_ENGINE_1, REG_LED_MAP_DIRECT);
}
#endif /* ENABLE_TESTPGM */


bool LP5562::clearAllPrograms() {
	for(size_t engine = 1; engine <= 3; engine++) {
		bool bResult = clearProgram(engine);
		if (!bResult) {
			return false;
		}
	}
	return true;
}

bool LP5562::setProgram(size_t engine, const uint16_t *instructions, size_t numInstructions, bool startRunning) {

	bool  bResult;

	// Set the engine to hold. This might not be necessary, I think it might happen automatically.
	setEnable(engineNumToMask(engine), REG_ENABLE_HOLD);

	// Enter code loading mode.
	bResult = setOpMode(engine, REG_ENGINE_LOAD);
	if (!bResult) {
		return false;
	}

	// Make a copy of the instruction list with 0x0000 (go to start) instructions to the end of the buffer, like
	// is the case on boot. Also, so you don't need to manually add one to have the code loop.
	uint16_t instructionsPadded[16];
	for(size_t ii = 0; ii < 16; ii++) {
		if (ii < numInstructions) {
			instructionsPadded[ii] = instructions[ii];
		}
		else {
			instructionsPadded[ii] = 0;
		}
	}

	uint8_t startAddr = (uint8_t)(REG_PROGRAM_1 + (engine - 1) * 0x20);

	// We can only write 15 instructions at a time because the I2C writes are limited to 32 bytes
	// and the register address takes 1, leaving 31 byte or 15 instructions (each instruction word is 2 bytes).
	wire.beginTransmission(addr);
	wire.write(startAddr);

	for(size_t ii = 0; ii < 15; ii++) {
		wire.write((uint8_t) (instructionsPadded[ii] >> 8)); // MSB first
		wire.write((uint8_t) instructionsPadded[ii]); // LSB second

		// Log.info("startAddr=%02x ii=%u program=%04x", startAddr, ii, instructionsPadded[ii]);

		startAddr += 2;
	}

	int stat = wire.endTransmission(true);
	if (stat != 0) {

		return false;
	}

	// Write the last instruction word that did not fit
	wire.beginTransmission(addr);
	wire.write(startAddr);

	wire.write((uint8_t) (instructionsPadded[15] >> 8)); // MSB first
	wire.write((uint8_t) instructionsPadded[15]); // LSB second
	// Log.info("startAddr=%02x ii=%u program=%04x", startAddr, 15, instructionsPadded[15]);

	stat = wire.endTransmission(true);
	if (stat != 0) {
		return false;
	}

	// Get out of programming mode
	bResult = setOpMode(engine, (numInstructions > 0) ? REG_ENGINE_RUN : REG_ENGINE_DISABLED);
	if (!bResult) {
		return false;
	}

	// If startRunning is true, actually start running
	if (startRunning && numInstructions > 0) {
		setEnable(engineNumToMask(engine), REG_ENABLE_RUN);
	}

	return true;
}

uint8_t LP5562::engineNumToMask(size_t engine) const {
	switch(engine) {
	case 1:
		return MASK_ENGINE_1;
	case 2:
		return MASK_ENGINE_2;
	case 3:
		return MASK_ENGINE_3;
	default:
		return 0;
	}
}

bool LP5562::setLedMapping(uint8_t red, uint8_t green, uint8_t blue, uint8_t white) {
	uint8_t value = 0;

	value |= (white & 0b11) << 6;
	value |= (red & 0b11) << 4;
	value |= (green & 0b11) << 2;
	value |= (blue & 0b11);

	return writeRegister(REG_LED_MAP, value);
}

bool LP5562::setLedMappingR(uint8_t mode, uint8_t value) {
	uint8_t regValue = getLedMapping();

	regValue &= 0b11001111;
	regValue |= (mode & 0b11) << 4;

	if (mode == 0) {
		(void) writeRegister(REG_R_PWM, value);
	}

	return writeRegister(REG_LED_MAP, regValue);
}

bool LP5562::setLedMappingG(uint8_t mode, uint8_t value) {
	uint8_t regValue = getLedMapping();

	regValue &= 0b11110011;
	regValue |= (mode & 0b11) << 2;

	if (mode == 0) {
		(void) writeRegister(REG_G_PWM, value);
	}

	return writeRegister(REG_LED_MAP, regValue);
}

bool LP5562::setLedMappingB(uint8_t mode, uint8_t value) {
	uint8_t regValue = getLedMapping();

	regValue &= 0b11111100;
	regValue |= (mode & 0b11);

	if (mode == 0) {
		(void) writeRegister(REG_B_PWM, value);
	}

	return writeRegister(REG_LED_MAP, regValue);
}

bool LP5562::setLedMappingW(uint8_t mode, uint8_t value) {
	uint8_t regValue = getLedMapping();

	regValue &= 0b00111111;
	regValue |= (mode & 0b11) << 6;

	if (mode == 0) {
		(void) writeRegister(REG_W_PWM, value);
	}

	return writeRegister(REG_LED_MAP, regValue);
}


bool LP5562::setEnable(uint8_t engineMask, uint8_t engineMode) {

	uint8_t value = readRegister(REG_ENABLE);

	if ((engineMask & MASK_ENGINE_1) != 0) {
		value &= 0b11001111;
		value |= (engineMode & 0b11) << 4;
	}

	if ((engineMask & MASK_ENGINE_2) != 0) {
		value &= 0b11110011;
		value |= (engineMode & 0b11) << 2;
	}

	if ((engineMask & MASK_ENGINE_3) != 0) {
		value &= 0b11111100;
		value |= (engineMode & 0b11);
	}

	// Log.info("setEnable mask 0x04x engineMode=%u value=%04x", engineMask, engineMode, value);

	return writeRegister(REG_ENABLE, value);
}


bool LP5562::setOpMode(size_t engine, uint8_t engineMode) {

	uint8_t value = readRegister(REG_OP_MODE);

	switch(engine) {
	case 1:
		value &= 0b11001111;
		value |= (engineMode & 0b11) << 4;
		break;

	case 2:
		value &= 0b11110011;
		value |= (engineMode & 0b11) << 2;
		break;

	case 3:
		value &= 0b11111100;
		value |= (engineMode & 0b11);
		break;
	}
	// Log.info("setOpMode engine=%u engineMode=%u value=%04x", engine, engineMode, value);

	return writeRegister(REG_OP_MODE, value);
}


void LP5562::setR(uint8_t red) {
	(void) writeRegister(REG_R_PWM, red);
}

void LP5562::setG(uint8_t green) {
	(void) writeRegister(REG_G_PWM, green);
}

void LP5562::setB(uint8_t blue) {
	(void) writeRegister(REG_B_PWM, blue);
}

void LP5562::setRGB(uint8_t red, uint8_t green, uint8_t blue) {
	setR(red);
	setG(green);
	setB(blue);
}

void LP5562::setRGB(uint32_t rgb) {
	setR(rgb >> 16);
	setG(rgb >> 8);
	setB(rgb);
}


void LP5562::useDirectRGB() {
	uint8_t ledMap = getLedMapping();

	uint8_t engineMask = 0;

	engineMask |= engineNumToMask(ledMap & 0b11);
	engineMask |= engineNumToMask((ledMap >> 2) & 0b11);
	engineMask |= engineNumToMask((ledMap >> 4) & 0b11);

	if (engineMask != 0) {
		setEnable(engineMask, REG_ENABLE_HOLD);
		(void) writeRegister(REG_LED_MAP, ledMap & 0b11000000);
	}
}

void LP5562::setW(uint8_t white) {
	(void) writeRegister(REG_W_PWM, white);
}

void LP5562::useDirectW() {
	uint8_t ledMap = getLedMapping();

	uint8_t engineMask = 0;
	engineMask |= engineNumToMask((ledMap >> 6) & 0b11);

	if (engineMask != 0) {
		setEnable(engineMask, REG_ENABLE_HOLD);
		(void) writeRegister(REG_LED_MAP, ledMap & 0b11000000);
	}
}

void LP5562::setIndicatorMode(unsigned long on1ms, unsigned long off1ms, unsigned long on2ms, unsigned long off2ms, uint8_t breatheTime) {

	// Engine 1 = Blink
	// Engine 2 = Fast Blink
	// Engine 3 = Breathe

	clearAllPrograms();


	LP5562Program program;

	// The main program is either 6 or 8 instructions. When msOn or msOff is > 1000 ms, then the delay requires 2 instructions.

	// Normally blink
	program.addCommandSetPWM(255); // full brightness
	program.addDelay(on1ms);
	program.addCommandSetPWM(0); // off
	program.addDelay(off1ms);
	program.addCommandGoToStart();
	setProgram(1, program, false);

	// Normally fast blink
	program.clear();
	program.addCommandSetPWM(255); // full brightness
	program.addDelay(on2ms);
	program.addCommandSetPWM(0); // off
	program.addDelay(off2ms);
	program.addCommandGoToStart();
	setProgram(2, program, false);

	// Breathe
	program.clear();
	program.addCommandSetPWM(0); // Start at lowLevel
	program.addCommandRamp(false, breatheTime, false, 255); // Ramp up
	program.addCommandRamp(false, breatheTime, true, 255); // Ramp down
	setProgram(3, program, false);


	// Default to LEDs off
	setLedMapping(REG_LED_MAP_DIRECT, REG_LED_MAP_DIRECT, REG_LED_MAP_DIRECT, REG_LED_MAP_DIRECT);
	setRGB(0, 0, 0);
	setW(0);

	setEnable(MASK_ENGINE_ALL, REG_ENABLE_RUN);


}

void LP5562::setBlink(uint8_t red, uint8_t green, uint8_t blue, unsigned long msOn, unsigned long msOff) {
	LP5562Program program;

	clearAllPrograms();

	// The main program is either 6 or 8 instructions. When msOn or msOff is > 1000 ms, then the delay requires 2 instructions.
	program.addCommandSetPWM(red);
	program.addDelay(msOn);
	program.addCommandSetPWM(0);
	program.addDelay(msOff);
	uint8_t triggerStep = program.getStepNum();
	program.addCommandTriggerSend(MASK_ENGINE_2 | MASK_ENGINE_3);
	program.addCommandGoToStart();
	setProgram(1, program, false);

	program.addCommandSetPWM(green, 0);
	program.addCommandTriggerWait(MASK_ENGINE_1, triggerStep);
	setProgram(2, program, false);

	program.addCommandSetPWM(blue, 0);
	setProgram(3, program, false);

	setLedMapping(REG_LED_MAP_ENGINE_1, REG_LED_MAP_ENGINE_2, REG_LED_MAP_ENGINE_3, REG_LED_MAP_DIRECT);

	setEnable(MASK_ENGINE_ALL, REG_ENABLE_RUN);
}

void LP5562::setBlink2(uint32_t rgb1, unsigned long ms1, uint32_t rgb2, unsigned long ms2) {
	setBlink2((uint8_t)(rgb1 >> 16), (uint8_t)(rgb1 >> 8), (uint8_t)rgb1, ms1,
			(uint8_t)(rgb2 >> 16), (uint8_t)(rgb2 >> 8), (uint8_t)rgb2, ms2);
}



void LP5562::setBlink2(uint8_t red1, uint8_t green1, uint8_t blue1, unsigned long ms1, uint8_t red2, uint8_t green2, uint8_t blue2, unsigned long ms2) {
	LP5562Program program;

	clearAllPrograms();

	// The main program is either 6 or 8 instructions. When ms1 or ms2 is > 1000 ms, then the delay requires 2 instructions.
	program.addCommandSetPWM(red1);
	program.addDelay(ms1);
	uint8_t colorStep = program.getStepNum();
	program.addCommandSetPWM(red2);
	program.addDelay(ms2);
	uint8_t triggerStep = program.getStepNum();
	program.addCommandTriggerSend(MASK_ENGINE_2 | MASK_ENGINE_3);
	program.addCommandGoToStart();
	setProgram(1, program, false);

	program.addCommandSetPWM(green1, 0);
	program.addCommandSetPWM(green2, colorStep);
	program.addCommandTriggerWait(MASK_ENGINE_1, triggerStep);
	setProgram(2, program, false);

	program.addCommandSetPWM(blue1, 0);
	program.addCommandSetPWM(blue2, colorStep);
	setProgram(3, program, false);

	setLedMapping(REG_LED_MAP_ENGINE_1, REG_LED_MAP_ENGINE_2, REG_LED_MAP_ENGINE_3, REG_LED_MAP_DIRECT);

	setEnable(MASK_ENGINE_ALL, REG_ENABLE_RUN);
}

void LP5562::setBreathe(bool red, bool green, bool blue, uint8_t stepTimeHalfMs, uint8_t lowLevel, uint8_t highLevel) {
	LP5562Program program;

	clearAllPrograms();

	// Clear all LEDs because if they're not turned on, then we want them to be off.
	setRGB(0, 0, 0);

	// Program is 3 instructions

	// Start at lowLevel
	program.addCommandSetPWM(lowLevel);

	// Ramp up
	program.addCommandRamp(false, stepTimeHalfMs, false, highLevel - lowLevel);

	// Ramp down
	program.addCommandRamp(false, stepTimeHalfMs, true, highLevel - lowLevel);

	setProgram(1, program, true);

	setLedMapping(red ? REG_LED_MAP_ENGINE_1 : REG_LED_MAP_DIRECT,
			green ? REG_LED_MAP_ENGINE_1 : REG_LED_MAP_DIRECT,
			blue ? REG_LED_MAP_ENGINE_1 : REG_LED_MAP_DIRECT,
			REG_LED_MAP_DIRECT);

	setEnable(REG_LED_MAP_ENGINE_1, REG_ENABLE_RUN);
}

uint8_t LP5562::readRegister(uint8_t reg) {
	wire.beginTransmission(addr);
	wire.write(reg);
	wire.endTransmission(false);

	wire.requestFrom(addr, (uint8_t) 1, (uint8_t) true);
	uint8_t value = (uint8_t) wire.read();

	// Log.trace("readRegister reg=%d value=%d", reg, value);

	return value;
}

bool LP5562::writeRegister(uint8_t reg, uint8_t value) {
	wire.beginTransmission(addr);
	wire.write(reg);
	wire.write(value);

	int stat = wire.endTransmission(true);

	// Log.trace("writeRegister reg=%d value=%d stat=%d read=%d", reg, value, stat, readRegister(reg));

	return (stat == 0);
}



LP5562Program::LP5562Program() {

}

LP5562Program::~LP5562Program() {

}

bool LP5562Program::addCommandRamp(bool prescale, uint8_t stepTime, bool decrease, uint8_t numSteps, int atInst) {
	uint16_t command = 0;

	if (stepTime > 0x3f) {
		stepTime = 0x3f;
	}

	if (prescale) {
		command |= 0b0100000000000000;
	}
	command |= (uint16_t)stepTime << 8;

	if (decrease) {
		command |= 0b0000000010000000;
	}
	command |= (numSteps & 0x7f);

	return addCommand(command, atInst);
}

bool LP5562Program::addCommandSetPWM(uint8_t level, int atInst) {
	uint16_t command = 0b0100000000000000 | level;

	return addCommand(command, atInst);
}

bool LP5562Program::addCommandGoToStart(int atInst) {
	uint16_t command = 0b0000000000000000;

	return addCommand(command, atInst);
}

bool LP5562Program::addCommandBranch(uint8_t loopCount, uint8_t stepNum, int atInst) {
	if (loopCount > 0x3f) {
		loopCount = 0x3f;
	}
	if (stepNum > 0xf) {
		// Invalid step number
		return false;
	}

	uint16_t command = 0b1010000000000000 | (((uint16_t)loopCount) << 7) | stepNum;

	return addCommand(command, atInst);
}

bool LP5562Program::addCommandEnd(bool generateInterrupt, bool setPWMto0, int atInst) {
	uint16_t command = 0b110000000000;

	if (generateInterrupt) {
		command |= 0b0001000000000000;
	}
	if (setPWMto0) {
		command |= 0b0000100000000000;
	}

	return addCommand(command, atInst);
}

bool LP5562Program::addCommandTriggerSend(uint8_t engineMask, int atInst) {
	uint16_t command = 0b1110000000000000;

	command |= (uint16_t)engineMask << 7;

	return addCommand(command, atInst);
}

bool LP5562Program::addCommandTriggerWait(uint8_t engineMask, int atInst) {
	uint16_t command = 0b1110000000000000;

	command |= engineMask << 1;

	return addCommand(command, atInst);
}


bool LP5562Program::addCommand(uint16_t cmd, int atInst) {
	if (atInst >= 0 && (size_t)atInst < MAX_INSTRUCTIONS) {
		if ((size_t)atInst >= nextInst) {
			nextInst = (size_t)atInst + 1;
		}
		instructions[atInst] = cmd;
	}
	else {
		if (nextInst >= MAX_INSTRUCTIONS) {
			return false;
		}
		instructions[nextInst++] = cmd;
	}
	return true;
}


bool LP5562Program::addDelay(unsigned long milliseconds) {
	if (milliseconds < 32) {
		// No prescale, no branch
		// 0.49 milliseconds per cycle
		uint8_t steps = (uint8_t) (milliseconds * 2);

		return addCommandWait(false, steps);
	}
	else
	if (milliseconds <= 1000) {
		// Prescale, no branch
		// 15.6 milliseconds per cycle
		uint8_t steps = (uint8_t) (milliseconds / 16);

		return addCommandWait(true, steps);
	}
	else
	if (milliseconds <= 63000) {
		// Prescale and branch

		// Each delay is 1 second (63 steps), up to 63 loops
		uint8_t loopCount = (uint8_t) (milliseconds / 1000);

		uint8_t stepNum = getStepNum();

		bool bResult = addCommandWait(true, 63);
		if (!bResult) {
			return false;
		}

		return addCommandBranch(loopCount, stepNum);
	}
	else {
		// Too long
		return false;
	}
}

void LP5562Program::clear() {
	for(uint8_t ii = 0; ii < MAX_INSTRUCTIONS; ii++) {
		instructions[ii] = 0;
	}
	nextInst = 0;
}


