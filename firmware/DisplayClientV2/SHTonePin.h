#ifndef __SHTONEPIN_H__
#define __SHTONEPIN_H__
#include <Arduino.h>

/*
	Square-wave output on hardware timer5 (CTC mode), replacing the Tone library.
	The Tone library cannot be linked alongside NewTone (duplicate timer ISRs), and a
	second Tone instance on the Mega would claim timer3, killing analogWrite on pin 5
	(the E36 fuel gauge). Timer5 PWM pins (44/45/46) are unused by the cluster wiring.
*/

#if !defined(__AVR_ATmega1280__) && !defined(__AVR_ATmega2560__)
#error "This SHTonePin implementation requires an Arduino Mega (timer5)"
#endif

static volatile uint8_t *shTone5Port = 0;
static uint8_t shTone5Mask = 0;

ISR(TIMER5_COMPA_vect) {
	*shTone5Port ^= shTone5Mask;
}

class SHTonePin {
private:
	uint8_t pin = 0;

	void setFrequency(unsigned long freq) {
		uint32_t top = (F_CPU / 128UL) / freq;
		uint8_t cs = _BV(CS51) | _BV(CS50);
		if (top > 65536UL) {
			top = (F_CPU / 512UL) / freq;
			cs = _BV(CS52);
		}
		if (top > 65536UL) top = 65536UL;
		if (top < 2UL) top = 2UL;
		noInterrupts();
		TCCR5A = 0;
		TCCR5B = _BV(WGM52) | cs;
		OCR5A = (uint16_t)(top - 1);
		if (TCNT5 >= OCR5A) TCNT5 = 0;
		TIMSK5 = _BV(OCIE5A);
		interrupts();
	}

public:
	void begin(int tonePin) {
		pin = tonePin;
		pinMode(pin, OUTPUT);
		digitalWrite(pin, LOW);
		shTone5Port = portOutputRegister(digitalPinToPort(pin));
		shTone5Mask = digitalPinToBitMask(pin);
	}

	void stop() {
		noInterrupts();
		TIMSK5 = 0;
		TCCR5B = 0;
		interrupts();
		digitalWrite(pin, LOW);
	}

	void play(unsigned long freq) {
		if (freq < 1) {
			stop();
		}
		else {
			setFrequency(freq);
		}
	}

	void readFromString()
	{
		int level = FlowSerialReadStringUntil('\n').toInt();
		if (level < 1)
			stop();
		else
			play(level);
	}
};

#endif
