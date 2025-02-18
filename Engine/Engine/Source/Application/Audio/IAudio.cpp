#include "IAudio.h"
#include "OpenALAudio.h"

using Engine::IAudio;

std::unique_ptr<IAudio> Engine::IAudio::CreateAudioSystem()
{
	return std::make_unique<OpenALAudio>();
}