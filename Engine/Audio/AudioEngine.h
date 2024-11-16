// #include "AudioFile.h" // For WAV file loading
// #include <SDL2/SDL.h>
// #include <RtAudio.h>
// #include <iostream>
// #include <vector>
// #include <string>


// template <class T>
// class AudioBuffer
// {
//     std::vector<T> samples;
//     int numChannels;
//     int numSamplesPerChannel;

// public:
//     void fromVector (const std::vector<T> &samples)
//     {
//         this->samples = samples;
//     }
//     std::vector<T> toVector() { return samples; }
//     int getNumChannels() { return numChannels; }
//     int getNumSamplesPerChannel() { return numSamplesPerChannel; }
// };

// class AudioHelperFunctions
// {
// public:
//     AudioFile<float> loadWavFile (const std::string &filePath)
//     {
//         AudioFile<float> audioFile;
//         audioFile.load(filePath);
//         return audioFile;
//     }

//     std::vector<float> getSamplesAsVector (const AudioFile<float> &audioFile)
//     {
//         std::vector<float> samples;
//         for (int i = 0; i < audioFile.getNumSamplesPerChannel(); i++)
//         {
//             samples.push_back (audioFile.samples[0][i]);
//         }
//         return samples;
//     }

//     template <class T>
//     AudioBuffer<T> getSamplesAsAudioBuffer (const AudioFile<T> &audioFile)
//     {
//         AudioBuffer<T> audioBuffer;
//         audioBuffer.fromVector (audioFile.samples[0]);
//         return audioBuffer;
//     }
// };

// class AudioPlayer
// {
//     AudioFile<float> audioFile;
//     SDL_AudioSpec audioSpec;
//     Uint8 *audioBuffer = nullptr;
//     Uint32 audioLength = 0;

//     static void audioCallback (void *userData, Uint8 *stream, int len)
//     {
//         // // Static callback for feeding audio data to the device
//         // AudioPlayer *player = (AudioPlayer*)userData;
//         // if (player->audioLength == 0)
//         // {
//         //     return; // No more audio to play
//         // }

//         // len = (len > player->audioLength) ? player->audioLength : len;
//         // SDL_memcpy (stream, player->audioBuffer, len);
//         // player->audioBuffer += len;
//         // player->audioLength -= len;
//     }

// public:
//     AudioPlayer (const std::string &filePath, const std::string &_id) : id (_id)
//     {
//         if (!audioFile.load (filePath))
//         {
//             std::cerr << "Failed to load audio file: " << filePath << std::endl;
//         }

//         SDL_Init (SDL_INIT_AUDIO);

//         // Set audio specifications
//         SDL_AudioSpec desiredSpec;
//         SDL_zero (desiredSpec);
//         desiredSpec.freq = audioFile.getSampleRate();
//         desiredSpec.format = AUDIO_F32; // Assuming 32-bit float samples
//         desiredSpec.channels = audioFile.getNumChannels();
//         desiredSpec.samples = 4096;
//         desiredSpec.callback = audioCallback;
//         desiredSpec.userdata = this;

//         // Open audio device
//         if (SDL_OpenAudio (&desiredSpec, &audioSpec) < 0)
//         {
//             std::cerr << "Failed to open audio: " << SDL_GetError() << std::endl;
//         }

//         // Convert audio file samples to buffer
//         std::vector<float> samples = audioFile.samples[0];
//         audioLength = samples.size() * sizeof (float);
//         audioBuffer = (Uint8 *) SDL_malloc (audioLength);
//         if (audioBuffer == nullptr)
//         {
//             std::cerr << "Failed to allocate audio buffer" << std::endl;
//         }
//         SDL_memcpy (audioBuffer, samples.data(), audioLength);
//     }

//     void trigger()
//     {
//         // Start playing
//         // SDL_PauseAudio (0); // Start the audio device
//         // while (audioLength > 0)
//         // {
//         //     SDL_Delay (100); // Wait until the audio is done playing
//         // }
//         // SDL_CloseAudio();
//         // SDL_Quit();

//     }

//     void playLoop()
//     {
//         // Start playing
//         SDL_PauseAudio (0); // Start the audio device
//     }

//     std::string getId() { return id; }
//     void setId (const std::string &_id) { id = _id; }

//     std::string id;
// };

// class MediaGroup
// {
//     public:

//     MediaGroup()
//     {

//     }
// };

// static void audio_callback (void* userdata, Uint8* stream, int len) 
// {
//     // Implement the callback functionality here
// }

// // Updated playAudio function to actually play the loaded audio
// class AudioMediaGroup : public MediaGroup
// {
//     std::string id;
// public:
//     std::vector<AudioPlayer> audioPlayers;

//     AudioMediaGroup (const std::string& _id) : id (_id)
//     {
//     }

//     void addAudioPlayer (const std::string &filePath, const std::string &id)
//     {
//         audioPlayers.push_back (AudioPlayer (filePath, id));
//     }

//     void triggerAudio (const std::string &id)
//     {
//         for (auto &player : audioPlayers)
//         {
//             if (player.getId() == id)
//             {
//                 player.trigger();
//             }
//         }
//     }

//     void playLoop (const std::string &id)
//     {
//         for (auto &player : audioPlayers)
//         {
//             if (player.getId() == id)
//             {
//                 player.playLoop();
//             }
//         }
//     }

//     std::string getId() { return id; }
// };

// // Your AudioEngine class
// class AudioEngine
// {
//     std::shared_ptr<RtAudio> audio;  // RtAudio object
//     unsigned int bufferFrames = 256; // Buffer size for low latency
// public:
//     std::vector<AudioMediaGroup> audioMediaGroups;

//     AudioEngine()
//     {

//     }

//     ~AudioEngine()
//     {
//         // Clean up
//         if (audio->isStreamOpen()) 
//         {
//             audio->closeStream();
//         }
//     }

//     void addAudioMediaGroup(const std::string &id)
//     {
//         audioMediaGroups.push_back(AudioMediaGroup(id));
//     }

//     AudioMediaGroup& getAudioMediaGroupByIndex(int index)
//     {
//         return audioMediaGroups[index];
//     }

//     AudioMediaGroup& getAudioMediaGroupById(const std::string &id)
//     {
//         for (auto &group : audioMediaGroups) {
//             if (group.getId() == id) {
//                 return group;
//             }
//         }
//         audioMediaGroups.emplace_back(id); // Add a new group if not found
//         return audioMediaGroups.back();
//     }

//     bool initialize()
//     {
//         audio = std::make_shared<RtAudio>(RtAudio::Api::WINDOWS_ASIO);
//         // Find the Focusrite device (ASIO) by name
//         RtAudio::DeviceInfo info;
//         int deviceId = -1;

//         // Print all audio device names
//         for (unsigned int i = 0; i < audio->getDeviceCount(); i++) 
//         {
//             int id = audio->getDefaultOutputDevice();
//             info = audio->getDeviceInfo (id);
//             std::cout<<info.name<<std::endl;
//             if (info.name == "FlexASIO") 
//             {
//                 deviceId = i;
//                 break;
//             }
//         }


//         if (audio->getDeviceCount() < 1) 
//         {
//             std::cerr << "No audio devices found!" << std::endl;
//         }

//         if (deviceId == -1) 
//         {
//             std::cerr << "Focusrite ASIO device not found!" << std::endl;
//             return false;
//         }

//         // Set up the stream parameters for output
//         RtAudio::StreamParameters parameters;
//         parameters.deviceId = deviceId;
//         parameters.nChannels = 2;               // Stereo
//         parameters.firstChannel = 0;

//         // Start the stream with the callback function
//         try {
//             audio->openStream(&parameters, nullptr, RTAUDIO_FLOAT32, 44100, &bufferFrames, &audio_callback);
//             audio->startStream();
//         } catch (const std::exception &e) {
//             std::cerr << "Error initializing RtAudio: " << e.what() << std::endl;
//             return false;
//         }

//         return true;
//     }

//     static int audio_callback(void *outputBuffer, void *inputBuffer, unsigned int nBufferFrames,
//                               double streamTime, RtAudioStreamStatus status, void *userData)
//     {
//         // Your audio processing code here
//         float *buffer = static_cast<float*>(outputBuffer);
//         if (status) {
//             std::cerr << "Stream underflow detected!" << std::endl;
//         }

//         // Fill the buffer with audio samples (e.g., from your AudioMediaGroups)
//         // This example just zeroes out the buffer
//         for (unsigned int i = 0; i < nBufferFrames * 2; i++) {
//             buffer[i] = 0.0f;  // Replace with actual audio sample data
//         }

//         return 0;
//     }
// };

#include "AudioFile.h" // For WAV file loading
#include <SDL2/SDL.h>
#include <SDL_mixer.h>
#include <iostream>
#include <vector>
#include <string>

class AudioPlayer
{
    Mix_Chunk* chunk = nullptr;  // For short sounds (WAV format)
    Mix_Music* music = nullptr;  // For streamed music (MP3 or OGG format)
    bool isMusic = false;

public:
    AudioPlayer (const std::string &filePath, const std::string &_id) : id (_id)
    {
        if (filePath.substr (filePath.find_last_of (".") + 1) == "mp3" || filePath.substr(filePath.find_last_of(".") + 1) == "ogg") {
            isMusic = true;
            music = Mix_LoadMUS (filePath.c_str());
            if (!music) 
            {
                std::cerr << "Failed to load music file: " << Mix_GetError() << std::endl;
            }
        } 
        else 
        {
            isMusic = false;
            chunk = Mix_LoadWAV (filePath.c_str());
            if (!chunk) 
            {
                std::cerr << "Failed to load WAV file: " << Mix_GetError() << std::endl;
            }
        }
    }

    ~AudioPlayer()
    {
        if (isMusic && music) 
        {
            Mix_FreeMusic (music);
        } 
        else if (chunk) 
        {
            Mix_FreeChunk (chunk);
        }
    }

    void play(bool loop = false)
    {
        if (isMusic && music) 
        {
            Mix_PlayMusic (music, loop ? -1 : 1);
        } 
        else if (chunk) 
        {
            Mix_PlayChannel (-1, chunk, loop ? -1 : 0);
        }
    }

    void stop()
    {
        if (isMusic) 
        {
            Mix_HaltMusic();
        } 
        else 
        {
            Mix_HaltChannel (-1);
        }
    }

    std::string getId() { return id; }
    void setId (const std::string &_id) { id = _id; }

private:
    std::string id;
};

class AudioMediaGroup
{
public:
    std::vector<AudioPlayer> audioPlayers;
    std::string id;

    AudioMediaGroup (const std::string &_id) : id(_id) {}

    void addAudioPlayer (const std::string &filePath, const std::string &id)
    {
        audioPlayers.emplace_back (filePath, id);
    }

    void playAudio (const std::string &id, bool loop = false)
    {
        for (auto &player : audioPlayers)
        {
            if (player.getId() == id)
            {
                player.play(loop);
            }
        }
    }

    void stopAudio(const std::string &id)
    {
        for (auto &player : audioPlayers)
        {
            if (player.getId() == id)
            {
                player.stop();
            }
        }
    }
};

class AudioEngine
{
public:
    std::vector<AudioMediaGroup> audioMediaGroups;

    AudioEngine()
    {

    }

    bool initialize()
    {
        // Initialize SDL and SDL_mixer
        if (SDL_Init (SDL_INIT_AUDIO) < 0) 
        {
            std::cerr << "Failed to initialize SDL: " << SDL_GetError() << std::endl;
            return false;
        }
        if (Mix_OpenAudio (44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) 
        {
            std::cerr << "Failed to initialize SDL_mixer: " << Mix_GetError() << std::endl;
            return false;
        }
        return true;
    }

    ~AudioEngine()
    {
        // Clean up SDL_mixer and SDL
        Mix_CloseAudio();
        SDL_Quit();
    }

    void addAudioMediaGroup (const std::string &id)
    {
        audioMediaGroups.emplace_back (id);
    }

    AudioMediaGroup& getAudioMediaGroupById (const std::string &id)
    {
        for (auto &group : audioMediaGroups) 
        {
            if (group.id == id) 
            {
                return group;
            }
        }
        audioMediaGroups.emplace_back (id); // Add a new group if not found
        return audioMediaGroups.back();
    }

    AudioMediaGroup& getAudioMediaGroupByIndex (int index)
    {
        return audioMediaGroups[index];
    }

    void playAudioInGroup (const std::string &groupId, const std::string &audioId, bool loop = false)
    {
        for (auto &group : audioMediaGroups) 
        {
            if (group.id == groupId) 
            {
                group.playAudio (audioId, loop);
                break;
            }
        }
    }

    void stopAudioInGroup (const std::string &groupId, const std::string &audioId)
    {
        for (auto &group : audioMediaGroups) 
        {
            if (group.id == groupId) 
            {
                group.stopAudio (audioId);
                break;
            }
        }
    }

    void stopAudioInAllGroups()
    {
        for (auto &group : audioMediaGroups) 
        {
            for (auto &player : group.audioPlayers) 
            {
                player.stop();
            }
        }
    }
};