#include <stdlib.h>

namespace midiopl {

struct va_entry {
	int note; // -1 denotes idle voice
	unsigned int lru;
};

class VoiceAllocator {
	public:
		VoiceAllocator(int n);
		~VoiceAllocator();
	public:
		int allocate(int note);
		int release(int note);
		void releaseAll();
		bool playing(int voice);
	private:
		va_entry * voices;
		int n_voices;
		unsigned int t;
		void touch(int voice, int note);
		int find_note(int note);
		int find_lru(bool playing);
};
} // namespace
