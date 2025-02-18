// © 2025 Yanan Liu <yanan.liu0325@gmail.com>

#pragma once

#include <memory>
#include "Common.h"
#include <string>
#include <unordered_map>
#include <atomic>

namespace Engine
{
	// The class is to provide an interface for audio system
	class IAudio
	{
	public:
		// Audio playback actions
		enum class EAudioAction
		{
			kStop,          // Stop the current audio
			kResume,        // Resume the current audio
			kPause,         // Pause the current audio
			kReplay,        // Replay the current audio from the beginning
			kRewind,        // Rewind the current audio
			kMute,          // Mute the current audio
			kUnmute,        // Unmute the current audio
			kLoop,          // Loop the current audio
			kStopLoop,      // Stop looping the current audio
			kVolumeUp,      // Increase the volume of the current audio
			kVolumeDown     // Decrease the volume of the current audio
		};

		// Audio format types
		enum class EAudioFormat
		{
			kCommand,   // Command type
			kWav,       // WAV format
			kMod,       // MOD format
			kMidi,      // MIDI format
			kOgg,       // Ogg Vorbis format
			kMp3,       // MP3 format
			kFlac,      // FLAC format
			kAiff,      // AIFF format
			kRaw,       // RAW PCM format
			kOthers     // Other formats
		};

	protected:
		// Audio path key management
		std::atomic<uint32_t> m_NextAudioKey{1};  // Start from 1, 0 reserved for invalid

		// Audio path key -> filepath mapping
		std::unordered_map<uint32_t, std::string> m_AudioKeyToPath;
		
		// Cache for last key generation
		mutable struct KeyGenCache {
			std::string path;
			uint32_t key{0};
		} m_LastKeyGeneration;

		// Helper methods for key management
		uint32_t GenerateAudioKey(const char* filepath) {
			std::string path(filepath);
			
			// Check cache first
			if (m_LastKeyGeneration.path == path) {
				return m_LastKeyGeneration.key;
			}
			
			// Look for existing key
			for (const auto& pair : m_AudioKeyToPath) {
				if (pair.second == path) {
					// Cache and return found key
					m_LastKeyGeneration = {path, pair.first};
					return pair.first;
				}
			}
			
			// Generate new key
			uint32_t newKey = m_NextAudioKey++;
			m_AudioKeyToPath[newKey] = path;
			
			// Cache the new key-path pair
			m_LastKeyGeneration = {path, newKey};
			return newKey;
		}

		const std::string& GetFilePath(uint32_t key) const {
			static const std::string empty;
			auto it = m_AudioKeyToPath.find(key);
			return it != m_AudioKeyToPath.end() ? it->second : empty;
		}

	public:
		// --------------------------------------------------------------------- //
		// Default Constructor
		IAudio() {};

		// Default Destructor
		virtual ~IAudio() {};

		// initialize audio system
		virtual bool Init() = 0;

		// create a new audio system and return it
		static std::unique_ptr<IAudio> CreateAudioSystem();

		// play music under the filepath, if the file hasn't been loaded, load it
		DLLEXP virtual bool PlayMusic(const char* filepath) = 0;

		// play sound under the filepath, if the file hasn't been loaded, load it
		DLLEXP virtual bool PlaySoundEffect(const char* filepath) = 0;

		// operation one action on the current music
		DLLEXP virtual void OperateCurrentMusic(EAudioAction action) = 0;

		// operation one action on the current sound
		DLLEXP virtual void OperateCurrentSounds(EAudioAction action) = 0;

		// fade in music
		DLLEXP virtual void FadeInMusic(const char* filepath, int loops, int ms) = 0;

		// fade out music
		virtual void FadeOutMusic(int ms) = 0;

		// free music by key
		virtual void FreeMusicByKey(uint32_t audioKey) = 0;

		// free sound by key
		virtual void FreeSoundByKey(uint32_t audioKey) = 0;

	public:
		// --------------------------------------------------------------------- //
		// Accessors & Mutators
		// --------------------------------------------------------------------- //
		/** set the current music's volume */
		virtual void SetMusicVolume(int volume) = 0;

		/** set a sound's volume */
		virtual void SetSoundVolume(const char* filepath, int volume) = 0;

		/** get the current music's volume */
		virtual int GetMusicVolume() = 0;

		/** get a sound's volume */
		virtual int GetSoundVolume(const char* filepath) = 0;

		/** get volume upper limit */
		virtual int GetMaxVolume() = 0;

		/** set music to the target position  */
		virtual void SetMusicPosition(double position_x, double position_y) = 0;

		/** set up a function to be called when music playback is halted */
		virtual void SetFinishMusicCallback(void(*music_finished)()) = 0;

		// get the format of the current music
		virtual EAudioFormat GetMusicType(const char* filepath) = 0;

		// tell you if the current music is actively playing, or not
		virtual bool IsMusicPlaying() = 0;

		// tell you if the current music is paused, or not
		virtual bool IsMusicPaused() = 0;

		// tell you if the current music is fading or not
		virtual bool IsMusicFading() = 0;
	};
}