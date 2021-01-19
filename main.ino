#include <MIDI.h>
#include <OPL3Duo.h>
#include <midi_instruments_4op.h>
#include <math.h>

#include "allocator.h"

// MIDI velo is [0..7f] OPLx velo is [3f..0]
const byte velocurve[128] PROGMEM = {
	0x3f, 0x3e, 0x3d, 0x3c, 0x3b, 0x3a, 0x39, 0x38, 0x37, 0x36, 0x35, 0x34, 0x33, 0x32, 0x31, 0x30,
	0x30, 0x2f, 0x2e, 0x2d, 0x2c, 0x2b, 0x2b, 0x2a, 0x29, 0x28, 0x27, 0x27, 0x26, 0x25, 0x24, 0x23,
	0x23, 0x22, 0x21, 0x21, 0x20, 0x1f, 0x1e, 0x1e, 0x1d, 0x1c, 0x1c, 0x1b, 0x1a, 0x1a, 0x19, 0x18,
	0x18, 0x17, 0x17, 0x16, 0x15, 0x15, 0x14, 0x14, 0x13, 0x13, 0x12, 0x12, 0x11, 0x11, 0x10, 0x0f,
	0x0f, 0x0f, 0x0e, 0x0e, 0x0d, 0x0d, 0x0c, 0x0c, 0x0b, 0x0b, 0x0a, 0x0a, 0x0a, 0x09, 0x09, 0x08,
	0x08, 0x08, 0x07, 0x07, 0x07, 0x06, 0x06, 0x06, 0x05, 0x05, 0x05, 0x05, 0x04, 0x04, 0x04, 0x03,
	0x03, 0x03, 0x03, 0x03, 0x02, 0x02, 0x02, 0x02, 0x02, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

#define VOICES 12
MIDI_CREATE_DEFAULT_INSTANCE();
midiopl::VoiceAllocator va(VOICES);
OPL3Duo opl3;
int fnumbers[VOICES];
bool sustain;

/**
 OPL2 lib uses octave (0..7) and note (0..11) to address notes
 It seems MIDI C2 (=note 24) corresponds with OPL2 octave 0, note 0
*/
#define GET_OCTAVE(note) (note / 12 - 2)
#define GET_NOTE(note)   (note % 12)

void handleNoteOff(byte inChannel, byte inNote, byte inVelocity) {
	if (!sustain && (24 <= inNote && inNote <= 119)) {
		byte voice = va.release(inNote);
		byte channel = opl3.get4OPControlChannel(voice);
		opl3.setKeyOn(channel, false);
	}
}

void handleNoteOn(byte inChannel, byte inNote, byte inVelocity) {
	// some midi implementations send notes with velo=0 to signal note-off, so handle this
	if (inVelocity == 0x00) {
		handleNoteOff(inChannel, inNote, inVelocity);
		return;
	}
	if (24 <= inNote && inNote <= 119) {
		byte voice = va.allocate(inNote);
		byte channel = opl3.get4OPControlChannel(voice);
		byte volume = pgm_read_byte_near(velocurve + inVelocity);
		opl3.set4OPChannelVolume(channel, volume);
		opl3.playNote(channel, GET_OCTAVE(inNote), GET_NOTE(inNote));
		fnumbers[voice] = opl3.getFNumber(channel);
	}
}

void allNotesOff() {
	// release all voices
	va.releaseAll();
	// note-off these voices
	for(int voice = 0; voice < VOICES; voice ++) {
		byte channel = opl3.get4OPControlChannel(voice);
		opl3.setKeyOn(channel, false);
	}
}

void handleProgramChange(byte inChannel, byte inProgram) {
	allNotesOff();
	// load program
	if (inProgram < (sizeof(midiInstruments) / sizeof(midiInstruments[0]))) {
		Instrument4OP i = opl3.loadInstrument4OP(midiInstruments[inProgram]);
		for(int voice = 0; voice < VOICES; voice ++) {
			byte channel = opl3.get4OPControlChannel(voice);
			opl3.setInstrument4OP(channel, i);
		}
	}
}

void handleControlChange(byte inChannel, byte inController, byte inValue) {

	// CC to OPL mapping based on my M-Audio Code 61
	switch (inController) {
		case 0x40: // sustain pedal
			if (inValue > 0) {
				sustain = true;
			}
			else {
				sustain = false;
				allNotesOff();
			}
			break;
		case 0x23: // E1
			handleProgramChange(inChannel, inValue);
			break;
	}
}

void handlePitchBend(byte inChannel, int bend) {
	double factor = pow(2.0, (bend / 8192.0) / 6.0);

	for(byte voice = 0; voice < VOICES; voice++) {
		if (va.playing(voice)) {
			int newF = fnumbers[voice] * factor;
			byte channel = opl3.get4OPControlChannel(voice);
			opl3.setFNumber(channel, newF);
		}
	}
}

void blink(int onTime, int offTime, int times) {
	for(int c = 0; c < times; c++) {
		digitalWrite(LED_BUILTIN, HIGH);
		delay(onTime);
		digitalWrite(LED_BUILTIN, LOW);
		delay(offTime);
	}
}

void setup() {
	pinMode(LED_BUILTIN, OUTPUT);
	blink(100, 50, 1);

	opl3.init();
	opl3.setOPL3Enabled(true);
	opl3.setAll4OPChannelsEnabled(true);
	sustain = false;
	blink(200, 150, 3);

	MIDI.begin();

	MIDI.setHandleNoteOn(handleNoteOn);
	MIDI.setHandleNoteOff(handleNoteOff);
	MIDI.setHandleControlChange(handleControlChange);
	MIDI.setHandleProgramChange(handleProgramChange);
	MIDI.setHandlePitchBend(handlePitchBend);
}

void loop() {
	MIDI.read();
}
