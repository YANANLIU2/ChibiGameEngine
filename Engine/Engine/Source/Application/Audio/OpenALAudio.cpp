// © 2025 Yanan Liu <yanan.liu0325@gmail.com>

#include "OpenALAudio.h"
#include <algorithm>
#include <chrono>
#include <thread>
#define DR_WAV_IMPLEMENTATION // Include dr_wav implementation only once in the project
#define DR_FLAC_IMPLEMENTATION
#define DR_MP3_IMPLEMENTATION
#include "AL/dr_wav.h"
#include "AL/dr_flac.h" 
#include "AL/dr_mp3.h"

namespace Engine
{
    OpenALAudio::OpenALAudio()
        : m_Device(nullptr)
        , m_Context(nullptr)
        , m_Initialized(false)
        , m_MusicVolume(1.0f)
        , m_CurrentMusicSource(0)
        , m_MusicPaused(false)
        , m_MusicFading(false)
        , m_MusicFinishedCallback(nullptr)
        , m_FadeStartVolume(0.0f)
        , m_FadeTargetVolume(0.0f)
        , m_FadeTimeRemaining(0.0f)
        , m_FadeDuration(0.0f)
		, m_CurrentMusicPathKey(0)
    {
    }

    OpenALAudio::~OpenALAudio()
    {
        // Delete all sources and buffers
        for (auto& pair : m_AudioBuffers)
        {
            for (ALuint source : pair.second.sources)
            {
                alDeleteSources(1, &source);
            }
            alDeleteBuffers(1, &pair.second.buffer);
        }

        if (m_Context)
        {
            alcMakeContextCurrent(nullptr);
            alcDestroyContext(m_Context);
        }

        if (m_Device)
        {
            alcCloseDevice(m_Device);
        }
    }

    bool OpenALAudio::Init()
    {
        m_Device = alcOpenDevice(nullptr);
        if (!m_Device)
        {
            return false;
        }

        m_Context = alcCreateContext(m_Device, nullptr);
        if (!m_Context)
        {
            alcCloseDevice(m_Device);
            return false;
        }

        if (!alcMakeContextCurrent(m_Context))
        {
            alcDestroyContext(m_Context);
            alcCloseDevice(m_Device);
            return false;
        }

        m_Initialized = true;
        return true;
    }

    void OpenALAudio::CleanupBuffer(const std::string& filepath)
    {
        uint32_t audioKey = GenerateAudioKey(filepath.c_str());
        auto it = m_AudioBuffers.find(audioKey);
        if (it != m_AudioBuffers.end())
        {
            // Stop and delete all sources using this buffer
            for (ALuint source : it->second.sources)
            {
                alSourceStop(source);
                alDeleteSources(1, &source);
            }

            // Delete the buffer
            alDeleteBuffers(1, &it->second.buffer);
            m_AudioBuffers.erase(it);
            m_AudioKeyToPath.erase(audioKey);

            // Clear current music if this was the current music
            if (audioKey == m_CurrentMusicPathKey)
            {
                m_CurrentMusicPathKey = 0;
                m_CurrentMusicSource = 0;
            }
        }
    }

    ALuint OpenALAudio::CreateSource()
    {
        ALuint source;
        alGenSources(1, &source);

        // Set default source properties
        alSource3f(source, AL_POSITION, 0.0f, 0.0f, 0.0f);
        alSource3f(source, AL_VELOCITY, 0.0f, 0.0f, 0.0f);
        alSource3f(source, AL_DIRECTION, 0.0f, 0.0f, 0.0f);
        alSourcef(source, AL_PITCH, 1.0f);
        alSourcef(source, AL_GAIN, 1.0f);
        alSourcei(source, AL_LOOPING, AL_FALSE);

        return source;
    }

    bool OpenALAudio::PlayMusic(const char* filepath)
    {
        if (!m_Initialized) return false;

        uint32_t audioKey = GenerateAudioKey(filepath);

        // Stop current music if playing
        if (m_CurrentMusicSource)
        {
            alSourceStop(m_CurrentMusicSource);
            alDeleteSources(1, &m_CurrentMusicSource);
            m_CurrentMusicSource = 0;
        }

        // Load or get existing buffer
        auto it = m_AudioBuffers.find(audioKey);
        if (it == m_AudioBuffers.end())
        {
            AudioBuffer newBuffer;
            alGenBuffers(1, &newBuffer.buffer);

            if (!LoadWAVFile(filepath, newBuffer.buffer))
            {
                alDeleteBuffers(1, &newBuffer.buffer);
                return false;
            }

            it = m_AudioBuffers.insert({ audioKey, newBuffer }).first;
        }

        // Create and setup source
        ALuint source = CreateSource();
        if (source == 0) return false;

        alSourcei(source, AL_BUFFER, it->second.buffer);
        alSourcef(source, AL_GAIN, m_MusicVolume);

        m_CurrentMusicSource = source;
        m_CurrentMusicPathKey = audioKey;
        it->second.sources.push_back(source);

        alSourcePlay(source);
        m_MusicPaused = false;
        m_MusicFading = false;

        return true;
    }

    bool OpenALAudio::PlaySoundEffect(const char* filepath)
    {
        if (!m_Initialized) return false;

        uint32_t audioKey = GenerateAudioKey(filepath);
        
        // Load or get existing buffer
        auto it = m_AudioBuffers.find(audioKey);
        if (it == m_AudioBuffers.end())
        {
            AudioBuffer newBuffer;
            alGenBuffers(1, &newBuffer.buffer);

            if (!LoadWAVFile(filepath, newBuffer.buffer))
            {
                alDeleteBuffers(1, &newBuffer.buffer);
                return false;
            }

            it = m_AudioBuffers.insert({ audioKey, newBuffer }).first;
        }

        // Create and play source
        ALuint source = CreateSource();
        if (source == 0) return false;

        alSourcei(source, AL_BUFFER, it->second.buffer);
        alSourcePlay(source);
        
        it->second.sources.push_back(source);
        CleanupFinishedSources();
        
        return true;
    }

    void OpenALAudio::OperateCurrentMusic(EAudioAction action)
    {
        if (!m_CurrentMusicSource) return;

        switch (action)
        {
        case EAudioAction::kStop:
            alSourceStop(m_CurrentMusicSource);
            break;
        case EAudioAction::kPause:
            alSourcePause(m_CurrentMusicSource);
            m_MusicPaused = true;
            break;
        case EAudioAction::kResume:
            alSourcePlay(m_CurrentMusicSource);
            m_MusicPaused = false;
            break;
        case EAudioAction::kReplay:
            alSourceRewind(m_CurrentMusicSource);
            alSourcePlay(m_CurrentMusicSource);
            break;
        case EAudioAction::kLoop:
            alSourcei(m_CurrentMusicSource, AL_LOOPING, AL_TRUE);
            break;
        case EAudioAction::kStopLoop:
            alSourcei(m_CurrentMusicSource, AL_LOOPING, AL_FALSE);
            break;
        case EAudioAction::kMute:
            alSourcef(m_CurrentMusicSource, AL_GAIN, 0.0f);
            break;
        case EAudioAction::kUnmute:
            alSourcef(m_CurrentMusicSource, AL_GAIN, m_MusicVolume);
            break;
        case EAudioAction::kVolumeUp:
            m_MusicVolume = std::min(m_MusicVolume + 0.1f, 1.0f);
            alSourcef(m_CurrentMusicSource, AL_GAIN, m_MusicVolume);
            break;
        case EAudioAction::kVolumeDown:
            m_MusicVolume = std::max(m_MusicVolume - 0.1f, 0.0f);
            alSourcef(m_CurrentMusicSource, AL_GAIN, m_MusicVolume);
            break;
        case EAudioAction::kRewind:
            alSourceRewind(m_CurrentMusicSource);
            break;
        default:
            printf("Invalid audio action\n");
            break;
        }
    }

    void OpenALAudio::OperateCurrentSounds(EAudioAction action)
    {
        if (!m_Initialized) return;

        // Clean up finished sources first
        CleanupFinishedSources();

        // Operate on all sound effect sources
        for (const auto& pair : m_AudioBuffers)
        {
            for (ALuint source : pair.second.sources)
            {
                if (source != m_CurrentMusicSource)  // Skip music source
                {
                    switch (action)
                    {
                    case EAudioAction::kStop:
                        alSourceStop(source);
                        break;
                    case EAudioAction::kPause:
                        alSourcePause(source);
                        break;
                    case EAudioAction::kResume:
                        alSourcePlay(source);
                        break;
                    case EAudioAction::kReplay:
                        alSourceRewind(source);
                        alSourcePlay(source);
                        break;
                    case EAudioAction::kRewind:
                        alSourceRewind(source);
                        break;
                    case EAudioAction::kMute:
                        alSourcef(source, AL_GAIN, 0.0f);
                        break;
                    case EAudioAction::kUnmute:
                        alSourcef(source, AL_GAIN, 1.0f);  // Or stored original volume
                        break;
                    case EAudioAction::kLoop:
                        alSourcei(source, AL_LOOPING, AL_TRUE);
                        break;
                    case EAudioAction::kStopLoop:
                        alSourcei(source, AL_LOOPING, AL_FALSE);
                        break;
                    case EAudioAction::kVolumeUp:
                        {
                            float currentVolume;
                            alGetSourcef(source, AL_GAIN, &currentVolume);
                            alSourcef(source, AL_GAIN, std::min(currentVolume + 0.1f, 1.0f));
                        }
                        break;
                    case EAudioAction::kVolumeDown:
                        {
                            float currentVolume;
                            alGetSourcef(source, AL_GAIN, &currentVolume);
                            alSourcef(source, AL_GAIN, std::max(currentVolume - 0.1f, 0.0f));
                        }
                        break;
                    }
                }
            }
        }
    }

    void OpenALAudio::SetMusicPosition(double position_x, double position_y)
    {
        if (m_CurrentMusicSource)
        {
            alSource3f(m_CurrentMusicSource, AL_POSITION,
                static_cast<float>(position_x),
                static_cast<float>(position_y),
                0.0f);
        }
    }

    void OpenALAudio::FadeInMusic(const char* filepath, int loops, int ms)
    {
        if (PlayMusic(filepath))
        {
            m_FadeStartVolume = 0.0f;
            m_FadeTargetVolume = m_MusicVolume;
            m_FadeTimeRemaining = static_cast<float>(ms) / 1000.0f;
            m_FadeDuration = m_FadeTimeRemaining;
            m_MusicFading = true;

            alSourcef(m_CurrentMusicSource, AL_GAIN, 0.0f);
            alSourcei(m_CurrentMusicSource, AL_LOOPING, loops == -1 ? AL_TRUE : AL_FALSE);
        }
    }

    void OpenALAudio::FadeOutMusic(int ms)
    {
        if (m_CurrentMusicSource)
        {
            m_FadeStartVolume = m_MusicVolume;
            m_FadeTargetVolume = 0.0f;
            m_FadeTimeRemaining = static_cast<float>(ms) / 1000.0f;
            m_FadeDuration = m_FadeTimeRemaining;
            m_MusicFading = true;
        }
    }

    void OpenALAudio::UpdateFading()
    {
        if (!m_MusicFading || !m_CurrentMusicSource) return;

        m_FadeTimeRemaining -= 0.016f; // Assuming 60 FPS
        if (m_FadeTimeRemaining <= 0.0f)
        {
            m_MusicFading = false;
            alSourcef(m_CurrentMusicSource, AL_GAIN, m_FadeTargetVolume);

            if (m_FadeTargetVolume == 0.0f)
            {
                alSourceStop(m_CurrentMusicSource);
                if (m_MusicFinishedCallback)
                {
                    m_MusicFinishedCallback();
                }
            }
        }
        else
        {
            float t = 1.0f - (m_FadeTimeRemaining / m_FadeDuration);
            float currentVolume = m_FadeStartVolume + (m_FadeTargetVolume - m_FadeStartVolume) * t;
            alSourcef(m_CurrentMusicSource, AL_GAIN, currentVolume);
        }
    }

    void OpenALAudio::FreeMusicByKey(uint32_t audioKey)
    {
        auto it = m_AudioBuffers.find(audioKey);
        if (it != m_AudioBuffers.end())
        {
            // Stop and delete all sources using this buffer
            for (ALuint source : it->second.sources)
            {
                alSourceStop(source);
                alDeleteSources(1, &source);
            }

            // Delete the buffer
            alDeleteBuffers(1, &it->second.buffer);
            m_AudioBuffers.erase(it);
            m_AudioKeyToPath.erase(audioKey);

            // Clear current music if this was the current music
            if (audioKey == m_CurrentMusicPathKey)
            {
                m_CurrentMusicPathKey = 0;
                m_CurrentMusicSource = 0;
            }
        }
    }

    void OpenALAudio::FreeSoundByKey(uint32_t audioKey)
    {
        auto it = m_AudioBuffers.find(audioKey);
        if (it != m_AudioBuffers.end())
        {
            // Stop and delete all sources using this buffer
            for (ALuint source : it->second.sources)
            {
                alSourceStop(source);
                alDeleteSources(1, &source);
            }

            // Delete the buffer
            alDeleteBuffers(1, &it->second.buffer);
            m_AudioBuffers.erase(it);
            m_AudioKeyToPath.erase(audioKey);
        }
    }

    void OpenALAudio::SetMusicVolume(int volume)
    {
        // Convert from 0-100 range to 0.0-1.0 range
        m_MusicVolume = std::max(0.0f, std::min(static_cast<float>(volume) / 100.0f, 1.0f));

        if (m_CurrentMusicSource)
        {
            alSourcef(m_CurrentMusicSource, AL_GAIN, m_MusicVolume);
        }
    }

    void OpenALAudio::SetSoundVolume(const char* filepath, int volume)
    {
        float normalizedVolume = std::max(0.0f, std::min(static_cast<float>(volume) / 100.0f, 1.0f));

        uint32_t audioKey = GenerateAudioKey(filepath);
        auto it = m_AudioBuffers.find(audioKey);
        if (it != m_AudioBuffers.end())
        {
            for (ALuint source : it->second.sources)
            {
                alSourcef(source, AL_GAIN, normalizedVolume);
            }
        }
    }

    int OpenALAudio::GetMusicVolume()
    {
        if (m_CurrentMusicSource)
        {
            float volume;
            alGetSourcef(m_CurrentMusicSource, AL_GAIN, &volume);
            return static_cast<int>(volume * 100.0f);
        }
        return static_cast<int>(m_MusicVolume * 100.0f);
    }

    int OpenALAudio::GetSoundVolume(const char* filepath)
    {
        uint32_t audioKey = GenerateAudioKey(filepath);
        auto it = m_AudioBuffers.find(audioKey);
        if (it != m_AudioBuffers.end() && !it->second.sources.empty())
        {
            float volume;
            alGetSourcef(it->second.sources[0], AL_GAIN, &volume);
            return static_cast<int>(volume * 100.0f);
        }
        return 0;
    }

    int OpenALAudio::GetMaxVolume()
    {
        return 100; // OpenAL uses 0.0-1.0, we convert to 0-100 range
    }

    void OpenALAudio::SetFinishMusicCallback(void(*music_finished)())
    {
        m_MusicFinishedCallback = music_finished;

        // Note: OpenAL doesn't provide direct callback functionality
        // You'll need to check the music state in your game loop
        // and call the callback when the music finishes
    }

    IAudio::EAudioFormat OpenALAudio::GetMusicType(const char* filepath)
    {
        std::string path(filepath);
        std::string ext;

        size_t dotPos = path.find_last_of('.');
        if (dotPos != std::string::npos)
        {
            ext = path.substr(dotPos + 1);
            // Convert to lowercase for comparison
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        }

        if (ext == "wav") return EAudioFormat::kWav;
        if (ext == "ogg") return EAudioFormat::kOgg;
        if (ext == "mp3") return EAudioFormat::kMp3;
        if (ext == "flac") return EAudioFormat::kFlac;
        if (ext == "mid" || ext == "midi") return EAudioFormat::kMidi;
        if (ext == "mod") return EAudioFormat::kMod;
        if (ext == "aiff") return EAudioFormat::kAiff;
        if (ext == "raw") return EAudioFormat::kRaw;

        return EAudioFormat::kOthers;
    }

    bool OpenALAudio::IsMusicPlaying()
    {
        if (!m_CurrentMusicSource)
            return false;

        ALint state;
        alGetSourcei(m_CurrentMusicSource, AL_SOURCE_STATE, &state);
        return state == AL_PLAYING;
    }

    bool OpenALAudio::IsMusicPaused()
    {
        if (!m_CurrentMusicSource)
            return false;

        ALint state;
        alGetSourcei(m_CurrentMusicSource, AL_SOURCE_STATE, &state);
        return state == AL_PAUSED;
    }

    bool OpenALAudio::IsMusicFading()
    {
        return m_MusicFading;
    }

    // Helper method to clean up sources that have finished playing
    void OpenALAudio::CleanupFinishedSources()
    {
        for (auto& pair : m_AudioBuffers)
        {
            auto& sources = pair.second.sources;
            sources.erase(
                std::remove_if(sources.begin(), sources.end(),
                    [](ALuint source) {
                        ALint state;
                        alGetSourcei(source, AL_SOURCE_STATE, &state);
                        if (state == AL_STOPPED)
                        {
                            alDeleteSources(1, &source);
                            return true;
                        }
                        return false;
                    }
                ),
                sources.end()
            );
        }
    }

    bool OpenALAudio::LoadAudioFile(const char *filepath, ALuint &buffer)
    {
        // Detect file format
        EAudioFormat format = GetMusicType(filepath);
        
        // Load based on format
        switch (format)
        {
        case EAudioFormat::kWav:
            return LoadWAVFile(filepath, buffer);
            
        case EAudioFormat::kMp3:
            return LoadMP3File(filepath, buffer);
            
        case EAudioFormat::kFlac:
            return LoadFLACFile(filepath, buffer);
            
        default:
			printf("Error: Unsupported audio format for file '%s'\n", filepath);
            return false;
        }
    }

    bool OpenALAudio::LoadWAVFile(const char *filepath, ALuint &buffer)
    {
        // Initialize WAV decoder
        drwav wav;
        if (!drwav_init_file(&wav, filepath, nullptr))
        {
            // Log error or handle failure
            return false;
        }

        // Allocate buffer for audio data
        std::vector<uint8_t> audioData(wav.totalPCMFrameCount * wav.channels * sizeof(int16_t));
        
        // Read PCM frames as 16-bit signed integers
        drwav_uint64 framesRead = drwav_read_pcm_frames_s16(&wav, wav.totalPCMFrameCount,
            reinterpret_cast<int16_t*>(audioData.data()));

        // If framesRead == 0, it means no valid audio was read
        if (framesRead == 0)
        {
			printf("Error: WAV file '%s' contains no valid audio data.\n", filepath);
			drwav_uninit(&wav);
            return false;
        }

        // check if framesRead < totalPCMFrameCount in case the file is truncated
        if (framesRead < wav.totalPCMFrameCount)
        {
            // Log a warning instead of failing completely
			printf("Warning: WAV file '%s' may be truncated. Expected %d frames but read %d.\n",
				filepath, wav.totalPCMFrameCount, framesRead);
        }

        // Determine format (mono or stereo)
        ALenum format = (wav.channels == 1) ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;
        
        // Load data into OpenAL buffer
        alBufferData(buffer, format, audioData.data(), 
            static_cast<ALsizei>(audioData.size()), wav.sampleRate);

        // Clean up WAV decoder
        drwav_uninit(&wav);

        // Check for OpenAL errors
        ALenum error = alGetError();
        if (error != AL_NO_ERROR)
        {
            // Log error or handle failure
            return false;
        }

        return true;
    }

    bool OpenALAudio::LoadMP3File(const char* filepath, ALuint& buffer)
    {
        // Initialize MP3 decoder
        drmp3 mp3;
        if (!drmp3_init_file(&mp3, filepath, nullptr))
        {
            return false;
        }

        // Retrieve the total number of PCM frames
        drmp3_uint64 totalPCMFrameCount = drmp3_get_pcm_frame_count(&mp3);
        if (totalPCMFrameCount == 0)
        {
            drmp3_uninit(&mp3);
            return false;
        }

        // Allocate a buffer for the PCM data
        std::vector<int16_t> audioData(static_cast<size_t>(totalPCMFrameCount) * mp3.channels);

        // Read the entire MP3 file into the PCM buffer
        drmp3_uint64 framesRead = drmp3_read_pcm_frames_s16(
            &mp3,
            totalPCMFrameCount,
            audioData.data()
        );

        // If framesRead == 0, it means no valid audio was read
        if (framesRead == 0)
        {
			printf("Error: MP3 file '%s' contains no valid audio data.\n", filepath);
            drmp3_uninit(&mp3);
            return false;
        }

        // check if framesRead < totalPCMFrameCount in case the file is truncated
        if (framesRead < totalPCMFrameCount)
        {
            // Log a warning instead of failing completely
			printf("Warning: MP3 file '%s' may be truncated. Expected %d frames but read %d.\n",
				filepath, totalPCMFrameCount, framesRead);
        }

        // Determine the OpenAL format based on channel count
        ALenum format = (mp3.channels == 1) ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;

        // Copy the PCM data into the OpenAL buffer
        alBufferData(
            buffer,
            format,
            audioData.data(),
            static_cast<ALsizei>(audioData.size() * sizeof(int16_t)),
            mp3.sampleRate
        );

        // Clean up the MP3 decoder
        drmp3_uninit(&mp3);

        // Check for OpenAL errors
        return (alGetError() == AL_NO_ERROR);
    }

    bool OpenALAudio::LoadFLACFile(const char* filepath, ALuint& buffer)
    {
        // Initialize FLAC decoder
        drflac* flac = drflac_open_file(filepath, nullptr);
        if (!flac)
        {
            return false;
        }

        // Read PCM data
        std::vector<int16_t> audioData(flac->totalPCMFrameCount * flac->channels);
        drflac_read_pcm_frames_s16(flac, flac->totalPCMFrameCount, audioData.data());

        // Set format based on channel count
        ALenum format = (flac->channels == 1) ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;
        
        // Load into OpenAL buffer
        alBufferData(buffer, format, audioData.data(), 
            static_cast<ALsizei>(audioData.size() * sizeof(int16_t)), flac->sampleRate);

        // Cleanup
        drflac_close(flac);
        
        return (alGetError() == AL_NO_ERROR);
    }

    ALuint OpenALAudio::LoadAudioBuffer(const char* filepath, uint32_t audioKey)
    {
        // Check if buffer is already loaded
        auto it = m_AudioBuffers.find(audioKey);
        if (it != m_AudioBuffers.end())
        {
            return it->second.buffer;
        }

        // Create new OpenAL buffer
        AudioBuffer newBuffer;
        alGenBuffers(1, &newBuffer.buffer);

        // Load audio data into buffer using format detection
        if (!LoadAudioFile(filepath, newBuffer.buffer))
        {
            alDeleteBuffers(1, &newBuffer.buffer);
            return 0;
        }

        // Cache the buffer
        m_AudioBuffers.insert({ audioKey, newBuffer });
        return newBuffer.buffer;
    }
}