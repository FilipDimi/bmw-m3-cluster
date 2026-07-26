#ifndef __SHCUSTOMPROTOCOL_H__
#define __SHCUSTOMPROTOCOL_H__

#include <Arduino.h>

/*
	BMW E36 cluster driver, ported from the AMSTUDIO / MorGuux "E36-Cluster-SIMHUB" H-revision sketch.

	SimHub custom protocol message (set in Arduino settings -> Custom protocol):
	if([DataCorePlugin.GameRunning] = 1, '1', '0') + ';' +
	format([DataCorePlugin.GameData.NewData.SpeedKmh],'0') + ';' +
	format([DataCorePlugin.Computed.Fuel_Percent], '0') + ';' +
	format([DataCorePlugin.GameData.NewData.WaterTemperature], '0') + ';' +
	format([DataCorePlugin.GameData.NewData.Throttle], '0')

	Outputs (same wiring as the original sketch):
	D3 = speedometer square wave, D4 = MPG gauge PWM, D5 = fuel gauge PWM, D6 = temp gauge PWM,
	D2 = tachometer square wave (native SimHub tachometer feature via SHTonePin, timer5).

	The speedometer signal is generated on hardware timer1 (CTC mode) instead of the Tone/NewTone
	libraries: those two can't be linked together, and a second Tone instance would claim timer3,
	killing analogWrite on pin 5 (fuel gauge). Timer1 PWM pins (11/12) are unused by the cluster,
	and timers 0/3/4 keep PWM alive on pins 4/5/6.
*/

#if !defined(__AVR_ATmega1280__) && !defined(__AVR_ATmega2560__)
#error "This SHCustomProtocol implementation requires an Arduino Mega (timer1/timer5)"
#endif

static volatile uint8_t *e36SpeedPort = 0;
static uint8_t e36SpeedMask = 0;

ISR(TIMER1_COMPA_vect) {
	*e36SpeedPort ^= e36SpeedMask;
}

class SHCustomProtocol {
private:
	static const int speedPin = 3;
	static const int mpgPin = 4;
	static const int fuelPin = 5;
	static const int tempPin = 6;

	void speedTonePlay(unsigned long freq) {
		uint32_t top = (F_CPU / 128UL) / freq;
		uint8_t cs = _BV(CS11) | _BV(CS10);
		if (top > 65536UL) {
			top = (F_CPU / 512UL) / freq;
			cs = _BV(CS12);
		}
		if (top > 65536UL) top = 65536UL;
		if (top < 2UL) top = 2UL;
		noInterrupts();
		TCCR1A = 0;
		TCCR1B = _BV(WGM12) | cs;
		OCR1A = (uint16_t)(top - 1);
		if (TCNT1 >= OCR1A) TCNT1 = 0;
		TIMSK1 = _BV(OCIE1A);
		interrupts();
	}

	void speedToneStop() {
		noInterrupts();
		TIMSK1 = 0;
		TCCR1B = 0;
		interrupts();
		digitalWrite(speedPin, LOW);
	}

public:
	void setup() {
		pinMode(speedPin, OUTPUT);
		digitalWrite(speedPin, LOW);
		e36SpeedPort = portOutputRegister(digitalPinToPort(speedPin));
		e36SpeedMask = digitalPinToBitMask(speedPin);
	}

	void read() {
		bool gamerunning = FlowSerialReadStringUntil(';').toInt();
		int speed = FlowSerialReadStringUntil(';').toInt();
		int fuel = FlowSerialReadStringUntil(';').toInt();
		int temp = FlowSerialReadStringUntil(';').toInt();
		int mpg = FlowSerialReadStringUntil('\n').toInt();

		int speedoutput = map(speed, 0, 286, 0, 367);
		int mpgoutput = map(mpg, 0, 100, 255, 0);
		int tempoutput = map(temp, 40, 120, 100, 0);
		int fueloutput = map(fuel, 0, 100, 0, 162);

		if (gamerunning) {
			if (speedoutput < 1) {
				speedToneStop();
			}
			else {
				speedTonePlay(speedoutput);
			}
			analogWrite(mpgPin, mpgoutput);
			analogWrite(fuelPin, fueloutput);
			analogWrite(tempPin, tempoutput);
		}
	}

	void loop() {
	}

	void idle() {
	}
};

#endif
