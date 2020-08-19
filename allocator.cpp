#include <limits.h>
#include "allocator.h"

namespace midiopl {

VoiceAllocator::VoiceAllocator(int n) {
	n_voices = n;
	voices = (va_entry *) malloc( n_voices * sizeof(va_entry) );
	for(int i = 0; i < n_voices; i++) {
		voices[i].note=-1;
		voices[i].lru = 0;
	}
}

VoiceAllocator::~VoiceAllocator() {
	free(voices);
}

int VoiceAllocator::find_note(int note) {
	for(int i = 0; i < n_voices; i ++) {
		if (voices[i].note == note) {
			return i;
		}
	}
	return -1;
}

int VoiceAllocator::find_lru(bool playing) {
	int found_i = -1;
	unsigned int found_u = UINT_MAX;

	for(int i = 0; i < n_voices; i++) {
		/* if...
		       not looking for playing AND note is idle
		     OR
		       looking for playing
		   AND
		     less recently used that "best" one
		*/
		if (((!playing && voices[i].note == -1) || playing) && (voices[i].lru < found_u)) {
			found_u = voices[i].lru;
			found_i = i;
		}
	}
	return found_i;
}

int VoiceAllocator::allocate(int note) {
	int v;

	/* First, try to find a voice already playing this note */
	v = find_note(note);
	if (v >= 0) {
		touch(v, note);
		return v;
	}

	/* Still here? Find least recently used non-playing voice */
	v = find_lru(false);
	if (v >= 0) {
		touch(v, note);
		return v;
	}

	/* Still here? Find least recently used playing voice */
	v = find_lru(true);
	if (v >= 0) {
		touch(v, note);
		return v;
	}

	/* You should be unable to reach this point. Yet, we select voice 0 */
	touch(0, note);
	return 0;
}

int VoiceAllocator::release(int note) {
	int v;

	v = find_note(note);
	if (v >= 0) {
		touch(v, -1);
	}
	return v;
}

void VoiceAllocator::releaseAll() {
	for (int v = 0; v < n_voices; v++) {
		touch(v, -1);
	}
}

void VoiceAllocator::touch(int voice, int note) {
	voices[voice].lru = t++;
	voices[voice].note = note;
}

} // namespace
