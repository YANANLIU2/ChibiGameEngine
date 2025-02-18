// © 2025 Yanan Liu <yanan.liu0325@gmail.com>

#pragma once

#include "IAudio.h"
#include "AL/al.h"
#include "AL/alc.h"
#include <unordered_map>
#include <string>
#include <vector>

namespace Engine
{
    // Buffer ID and sources using this buffer
    struct AudioBuffer
    {
        // Buffer ID which helps to identify the sound dat
        ALuint buffer;

        // Sources using this buffer. Multiple sources can use the same buffer (eg: footsteps, gunshots, etc.)
        std::vector<ALuint> sources;

		// Default constructor
		AudioBuffer() : buffer(0) {}
    };

    // OpenAL audio system
    class OpenALAudio : public IAudio
    {
    public:
        // Default constructor
        OpenALAudio();

        // Default destructor
        virtual ~OpenALAudio();

        // Initialize the audio system
        virtual bool Init() override;

        // Music playback functions
        virtual bool PlayMusic(const char* filepath) override;
        virtual bool PlaySoundEffect(const char* filepath) override;
        virtual void OperateCurrentMusic(EAudioAction action) override;
        virtual void OperateCurrentSounds(EAudioAction action) override;
        virtual void FadeInMusic(const char* filepath, int loops, int ms) override;
        virtual void FadeOutMusic(int ms) override;
        virtual void FreeMusicByKey(uint32_t audioKey) override;
        virtual void FreeSoundByKey(uint32_t audioKey) override;

        // Volume control
        virtual void SetMusicVolume(int volume) override;
        virtual void SetSoundVolume(const char* filepath, int volume) override;
        virtual int GetMusicVolume() override;
        virtual int GetSoundVolume(const char* filepath) override;
        virtual int GetMaxVolume() override;

        // Position and callback
        virtual void SetMusicPosition(double position_x, double position_y) override;
        virtual void SetFinishMusicCallback(void(*music_finished)()) override;

        // Status queries
        virtual EAudioFormat GetMusicType(const char* filepath) override;
        virtual bool IsMusicPlaying() override;
        virtual bool IsMusicPaused() override;
        virtual bool IsMusicFading() override;

    private:
        // Helper functions
        bool LoadWAVFile(const char* filepath, ALuint& buffer);
        void CleanupBuffer(const std::string& filepath);
        ALuint CreateSource();
        void UpdateFading();
        void CleanupFinishedSources();

    private:
        ALCdevice* m_Device;     // Pointer to the audio device
        ALCcontext* m_Context;   // Audio context for this device
      
        // Audio buffer map using audio path key as a unique identifier
        std::unordered_map<uint32_t, AudioBuffer> m_AudioBuffers;

        // Path key of the current music tracking
        uint32_t m_CurrentMusicPathKey;

        // Source of the current music
        ALuint m_CurrentMusicSource;
        
        // Audio state
        bool m_Initialized;
        float m_MusicVolume;
        bool m_MusicPaused;
        bool m_MusicFading;
        void(*m_MusicFinishedCallback)();

        // Fading state
        float m_FadeStartVolume;
        float m_FadeTargetVolume;
        float m_FadeTimeRemaining;
        float m_FadeDuration;
    };
}   