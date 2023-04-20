#include "util/rand.h" 

static u32 state;

void seed(u32 value)
{
	state = value;
}

u32 rand()
{
	state ^= state << 13;
	state ^= state >> 17;
	state ^= state << 5;

	return state;
}
