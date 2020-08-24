#include <MIDIUSB.h>
#include <SPI.h>
#include <OPL2.h>
#include <midi_instruments.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <math.h>
#include "allocator.h"

#define CHANNELS 9

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET     -1

midiopl::VoiceAllocator va(CHANNELS);
OPL2 opl2;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
int fnumbers[CHANNELS];
bool sustain;

/*
 OPL2 lib uses octave (0..7) and note (0..11) to address notes
 It seems MIDI C2 (=note 24) corresponds with OPL2 octave 0, note 0
*/
#define GET_OCTAVE(note) (note / 12 - 2)
#define GET_NOTE(note)   (note % 12)

#define PS_SIZE 8
#define PS_OFF_X 0
#define PS_OFF_Y 16

void drawPolyOutline() {
	for(byte channel = 0; channel < CHANNELS; channel++) {
		display.drawRect( \
			PS_OFF_X + channel * PS_SIZE, \
			PS_OFF_Y, \
			PS_SIZE, \
			PS_SIZE, \
			SSD1306_WHITE \
		);
	}
	display.display();
}

void drawPolyStatus() {
	for(byte channel = 0; channel < CHANNELS; channel++) {
		display.fillRect( \
			PS_OFF_X + channel * PS_SIZE + 1, \
			PS_OFF_Y + 1, \
			PS_SIZE - 2, \
			PS_SIZE - 2, \
			va.playing(channel) ? SSD1306_WHITE : SSD1306_BLACK \
		);
	}
	display.display();
}

void handleNoteOff(byte inChannel, byte inNote, byte inVelocity) {
	if (! sustain) {
		if (24 <= inNote && inNote <= 119) {
			byte channel = va.release(inNote);
			opl2.setKeyOn(channel, false);
			drawPolyStatus();
		}
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
		drawPolyStatus();
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

	// CC to OPL2 mapping based on my MK449c
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
		case 0x49: // F1
			for(byte channel = 0; channel < CHANNELS; channel ++) 
				opl2.setAttack(channel, 0, 0xf - (inValue >> 3));
			break;
		case 0x48: // F2
			for(byte channel = 0; channel < CHANNELS; channel ++) 
				opl2.setDecay(channel, 0, 0xf - (inValue >> 3));
			break;
		case 0x5b: // F3
			for(byte channel = 0; channel < CHANNELS; channel ++) 
				opl2.setSustain(channel, 0, 0xf - (inValue >> 3));
			break;
		case 0x5d: // F4
			for(byte channel = 0; channel < CHANNELS; channel ++) 
				opl2.setRelease(channel, 0, 0xf - (inValue >> 3));
			break;
		case 0x4a: // F5
			for(byte channel = 0; channel < CHANNELS; channel ++) 
				opl2.setAttack(channel, 1, 0xf - (inValue >> 3));
			break;
		case 0x47: // F6
			for(byte channel = 0; channel < CHANNELS; channel ++) 
				opl2.setDecay(channel, 1, 0xf - (inValue >> 3));
			break;
		case 0x05: // F7
			for(byte channel = 0; channel < CHANNELS; channel ++) 
				opl2.setSustain(channel, 0, 0xf - (inValue >> 3));
			break;
		case 0x54: // F8
			for(byte channel = 0; channel < CHANNELS; channel ++) 
				opl2.setRelease(channel, 0, 0xf - (inValue >> 5));
			break;
		case 0x07: // F9
			for(byte channel = 0; channel < CHANNELS; channel ++) 
				opl2.setVolume(channel, 0, 0x3f - (inValue >> 1));
			break;
		case 0x0a: // C10
			for(byte channel = 0; channel < CHANNELS; channel ++) 
				opl2.setWaveForm(channel, 0, inValue);
			break;
		case 0x02: // C11
			for(byte channel = 0; channel < CHANNELS; channel ++) 
				opl2.setWaveForm(channel, 0, inValue >> 5);
			break;
		/*
		case 0x0c: // C12
		case 0x0d: // C13
		case 0x4b: // C14
		case 0x4c: // C15
		case 0x5c: // C16
		case 0x5f: // C17
		*/
		default:
			display.clearDisplay();
			display.setCursor(0,0);
			display.print("CC ");
			display.print(inController, HEX);
			display.print("=");
			display.print(inValue, HEX);
			display.display();
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

void setup() {
	opl2.init();
	display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
	display.clearDisplay();
	display.setTextColor(SSD1306_WHITE);        // Draw white text
	display.setTextSize(1);
	display.display();
	sustain = false;
	drawPolyOutline();
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
