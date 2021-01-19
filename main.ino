#include <MIDI.h>
#include <OPL2.h>
#include <platform_arduino.h> // OPL2 custom platforms
#include <midi_instruments.h>
#include <math.h>
#include "allocator.h"

#define CHANNELS 6

MIDI_CREATE_DEFAULT_INSTANCE();
midiopl::VoiceAllocator va(CHANNELS);
WiringShiftOut wso(4, 5, 3, 6, 7);
OPL2 opl2(&wso);
int fnumbers[CHANNELS];
bool sustain;

/*
 OPL2 lib uses octave (0..7) and note (0..11) to address notes
 It seems MIDI C2 (=note 24) corresponds with OPL2 octave 0, note 0
*/
#define GET_OCTAVE(note) (note / 12 - 2)
#define GET_NOTE(note)   (note % 12)

void handleNoteOff(byte inChannel, byte inNote, byte inVelocity) {
	if (!sustain && (24 <= inNote && inNote <= 119)) {
		byte channel = va.release(inNote);
		opl2.setKeyOn(channel, false);
	}
}

void handleNoteOn(byte inChannel, byte inNote, byte inVelocity) {
	// some midi implementations send notes with velo=0 to signal note-off, so handle this
	if (inVelocity == 0x00) {
		handleNoteOff(inChannel, inNote, inVelocity);
		return;
	}
	if (24 <= inNote && inNote <= 119) {
		byte channel = va.allocate(inNote);
		opl2.setVolume(channel, 1, 0x3f - (inVelocity >> 1));
		opl2.playNote(channel, GET_OCTAVE(inNote), GET_NOTE(inNote));
		fnumbers[channel] = opl2.getFNumber(channel);
	}
}

void allNotesOff() {
	// release all voices
	va.releaseAll();
	// note-off these voices
	for(int channel = 0; channel < CHANNELS; channel ++) {
		opl2.setKeyOn(channel, false);
	}
}

void handleProgramChange(byte inChannel, byte inProgram) {
	allNotesOff();
	// load program
	if (inProgram < (sizeof(midiInstruments) / sizeof(midiInstruments[0]))) {
		Instrument i = opl2.loadInstrument(midiInstruments[inProgram]);
		for(int channel = 0; channel < CHANNELS; channel ++) {
			opl2.setInstrument(channel, i);
		}
	}
}

void handleControlChange(byte inChannel, byte inController, byte inValue) {

	// CC to OPL2 mapping based on my M-Audio Code 61
	switch (inController) {
		/*
		case 0x01: // mod wheel
		*/
		case 0x40: // sustain pedal
			if (inValue > 0) {
				sustain = true;
			}
			else {
				sustain = false;
				allNotesOff();
			}
			break;
		case 0x76: // F1
			for(byte channel = 0; channel < CHANNELS; channel ++) 
				opl2.setVolume(channel, 0, 0x3f - (inValue >> 1));
			break;
		case 0x77: // F2
			for(byte channel = 0; channel < CHANNELS; channel ++) 
				opl2.setAttack(channel, 0, 0xf - (inValue >> 3));
			break;
		case 0x78: // F3
			for(byte channel = 0; channel < CHANNELS; channel ++) 
				opl2.setDecay(channel, 0, 0xf - (inValue >> 3));
			break;
		case 0x79: // F4
			for(byte channel = 0; channel < CHANNELS; channel ++) 
				opl2.setSustain(channel, 0, 0xf - (inValue >> 3));
			break;
		case 0x7a: // F5
			for(byte channel = 0; channel < CHANNELS; channel ++) 
				opl2.setRelease(channel, 0, 0xf - (inValue >> 3));
			break;
		case 0x7b: // F6
			for(byte channel = 0; channel < CHANNELS; channel ++) 
				opl2.setAttack(channel, 1, 0xf - (inValue >> 3));
			break;
		case 0x7c: // F7
			for(byte channel = 0; channel < CHANNELS; channel ++) 
				opl2.setDecay(channel, 1, 0xf - (inValue >> 3));
			break;
		case 0x7d: // F8
			for(byte channel = 0; channel < CHANNELS; channel ++) 
				opl2.setSustain(channel, 0, 0xf - (inValue >> 3));
			break;
		case 0x7e: // F9
			for(byte channel = 0; channel < CHANNELS; channel ++) 
				opl2.setRelease(channel, 0, 0xf - (inValue >> 5));
			break;
		case 0x23: // E1
			for(byte channel = 0; channel < CHANNELS; channel ++) 
				opl2.setWaveForm(channel, 0, inValue);
			break;
		case 0x29: // E2
			for(byte channel = 0; channel < CHANNELS; channel ++) 
				opl2.setWaveForm(channel, 0, inValue >> 5);
			break;
	}
}

void handlePitchBend(byte inChannel, int bend) {
	double factor = pow(2.0, (bend / 8192.0) / 6.0);

	for(byte ch = 0; ch < CHANNELS; ch++) {
		int newF = fnumbers[ch] * factor;
		opl2.setFNumber(ch, newF);
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

	opl2.init();
	blink(200, 50, 2);
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
