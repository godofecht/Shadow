#include "AudioFile.h" // For WAV file loading
#include <SDL2/SDL.h>
#include <vector>
#include <string>
#include <iostream>

template <class T>
class AudioBuffer
{
    std::vector<T> samples;
    int numChannels;
    int numSamplesPerChannel;

public:
    void fromVector (const std::vector<T> &samples)
    {
        this->samples = samples;
    }
    std::vector<T> toVector() { return samples; }
    int getNumChannels() { return numChannels; }
    int getNumSamplesPerChannel() { return numSamplesPerChannel; }
};

class AudioHelperFunctions
{
public:
    AudioFile<float> loadWavFile (const std::string &filePath)
    {
        AudioFile<float> audioFile;
        audioFile.load(filePath);
        return audioFile;
    }

    std::vector<float> getSamplesAsVector (const AudioFile<float> &audioFile)
    {
        std::vector<float> samples;
        for (int i = 0; i < audioFile.getNumSamplesPerChannel(); i++)
        {
            samples.push_back (audioFile.samples[0][i]);
        }
        return samples;
    }

    template <class T>
    AudioBuffer<T> getSamplesAsAudioBuffer (const AudioFile<T> &audioFile)
    {
        AudioBuffer<T> audioBuffer;
        audioBuffer.fromVector (audioFile.samples[0]);
        return audioBuffer;
    }
};

class AudioPlayer
{
    AudioFile<float> audioFile;
    SDL_AudioSpec audioSpec;
    Uint8 *audioBuffer = nullptr;
    Uint32 audioLength = 0;

    static void audioCallback (void *userData, Uint8 *stream, int len)
    {
        // Static callback for feeding audio data to the device
        AudioPlayer *player = (AudioPlayer *)userData;
        if (player->audioLength == 0)
        {
            return; // No more audio to play
        }

        len = (len > player->audioLength) ? player->audioLength : len;
        SDL_memcpy (stream, player->audioBuffer, len);
        player->audioBuffer += len;
        player->audioLength -= len;
    }

public:
    AudioPlayer (const std::string &filePath)
    {
        if (!audioFile.load (filePath))
        {
            std::cerr << "Failed to load audio file: " << filePath << std::endl;
        }

        SDL_Init (SDL_INIT_AUDIO);

        // Set audio specifications
        SDL_AudioSpec desiredSpec;
        SDL_zero (desiredSpec);
        desiredSpec.freq = audioFile.getSampleRate();
        desiredSpec.format = AUDIO_F32; // Assuming 32-bit float samples
        desiredSpec.channels = audioFile.getNumChannels();
        desiredSpec.samples = 4096;
        desiredSpec.callback = audioCallback;
        desiredSpec.userdata = this;

        // Open audio device
        if (SDL_OpenAudio (&desiredSpec, &audioSpec) < 0)
        {
            std::cerr << "Failed to open audio: " << SDL_GetError() << std::endl;
        }

        // Convert audio file samples to buffer
        std::vector<float> samples = audioFile.samples[0];
        audioLength = samples.size() * sizeof (float);
        audioBuffer = (Uint8 *) SDL_malloc (audioLength);
        if (audioBuffer == nullptr)
        {
            std::cerr << "Failed to allocate audio buffer" << std::endl;
        }
        SDL_memcpy (audioBuffer, samples.data(), audioLength);
    }

    void trigger()
    {
        // Start playing
        SDL_PauseAudio (0); // Start the audio device
        while (audioLength > 0)
        {
            SDL_Delay (100); // Wait until the audio is done playing
        }
        SDL_CloseAudio();
        SDL_Quit();
    }

    std::string getId() { return "id"; }
};

class MediaGroup{};

// Updated playAudio function to actually play the loaded audio
class AudioMediaGroup : public MediaGroup
{
public:
    std::vector<AudioPlayer> audioPlayers;

    AudioMediaGroup()
    {
        audioPlayers.push_back (AudioPlayer ("C:/Users/abhis/gamedev/Shadow/fly.wav"));
    }

    void addAudioPlayer (const std::string &filePath)
    {
        audioPlayers.push_back (AudioPlayer (filePath));
    }

    void playAudio (const std::string &id)
    {
        for (auto &player : audioPlayers)
        {
            if (player.getId() == id)
            {
                player.trigger();
            }
        }
    }
};

// Your AudioEngine class
class AudioEngine
{
public:
    std::vector<AudioMediaGroup> audioMediaGroups;
};

// Main entry point for testing purposes
int main()
{
    AudioMediaGroup group;
    group.playAudio ("id");
    return 0;
}
