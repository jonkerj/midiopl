#include <MIDI.h>
#include <midi_instruments.h>
#include <SPI.h>
#include <OPL2.h>
#include "allocator.h"

MIDI_CREATE_DEFAULT_INSTANCE();
midiopl::VoiceAllocator va(6);
OPL2 opl2;

/*
 OPL2 lib uses octave (0..7) and note (0..11) to address notes
 It seems MIDI C2 (=note 24) corresponds with OPL2 octave 0, note 0
*/
#define GET_OCTAVE(note) (note / 12 - 2)
#define GET_NOTE(note)   (note % 12)

void handleNoteOff(byte inChannel, byte inNote, byte inVelocity) {
	if (24 <= inNote && inNote <= 119) {
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
		opl2.playNote(channel, GET_OCTAVE(inNote), GET_NOTE(inNote));
	}
}

void handleProgramChange(byte inChannel, byte inProgram) {
	// release all voices
	va.releaseAll();
	// note-off these voices
	for(int channel = 0; channel < 6; channel ++) {
		opl2.setKeyOn(channel, false);
	}
	// load program
	if (inProgram < (sizeof(midiInstruments) / sizeof(midiInstruments[0]))) {
		opl2.loadInstrument(midiInstruments[inProgram]);
	}
}

void setup() {
	opl2.init();
	va = midiopl::VoiceAllocator(6);
	MIDI.setHandleNoteOn(handleNoteOn);
	MIDI.setHandleNoteOff(handleNoteOff);
	MIDI.setHandleProgramChange(handleProgramChange);
	MIDI.begin();
}

void loop() {
	MIDI.read();
}
