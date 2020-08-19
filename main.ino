#include <MIDIUSB.h>
#include <SPI.h>
#include <OPL2.h>
#include <midi_instruments.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "allocator.h"

#define CHANNELS 9

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET     -1

midiopl::VoiceAllocator va(CHANNELS);
OPL2 opl2;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

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
	for(int channel = 0; channel < CHANNELS; channel ++) {
		opl2.setKeyOn(channel, false);
	}
	// load program
	if (inProgram < (sizeof(midiInstruments) / sizeof(midiInstruments[0]))) {
		Instrument i = opl2.loadInstrument(midiInstruments[inProgram]);
		for(int channel = 0; channel < CHANNELS; channel ++) {
			opl2.setInstrument(channel, i);
		}
	}
}

void handleControlChange(byte inChannel, byte inController, byte inValue) {
	display.clearDisplay();
	display.setCursor(0,0);
	display.print("CC ");
	display.print(inController, HEX);
	display.print("=");
	display.print(inValue, HEX);
	display.display();
}

void handlePitchBend(byte inChannel, int bend) {
	display.clearDisplay();
	display.setCursor(0,0);
	display.print("PB ");
	display.print(bend);
	display.display();
}

void setup() {
	opl2.init();
	display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
	display.clearDisplay();
	display.display();
	display.setTextColor(SSD1306_WHITE);        // Draw white text
	display.setTextSize(2);
}

void loop() {
	midiEventPacket_t rx = MidiUSB.read();
	switch (rx.header) {
		case 0x9: // 0x90 = NoteOn
			handleNoteOn(rx.byte1, rx.byte2, rx.byte3);
			break;
		case 0x8: // 0x80 = NoteOff
			handleNoteOff(rx.byte1, rx.byte2, rx.byte3);
			break;
		case 0xb: // 0xb0 = Control Change
			handleControlChange(rx.byte1, rx.byte2, rx.byte3);
			break;
		case 0xc: // 0xc0 = Program Change
			handleProgramChange(rx.byte1, rx.byte2);
			break;
		case 0xe: // 0xe0 = Pitch bend
			int bend = (rx.byte3 << 7) + rx.byte2 - 0x2000;
			handlePitchBend(rx.byte1, bend);
			break;
	}
}
