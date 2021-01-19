#include <MIDI.h>
#include <OPL3Duo.h>
#include <midi_instruments_4op.h>
#include <math.h>

#include "allocator.h"

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
		byte volume = 0x3f - (inVelocity >> 1);
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
