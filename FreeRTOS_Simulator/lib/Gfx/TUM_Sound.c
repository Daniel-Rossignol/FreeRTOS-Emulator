#include <string.h>

#include <SDL2/SDL_mixer.h>

#define SAMPLE_FOLDER   "/../resources/"

#define NUM_WAVEFORMS   2

#define AUDIO_CHANNELS  2
#define MIXING_CHANNELS 4

#define GEN_FULL_SAMPLE_PATH(SAMPLE) SAMPLE_FOLDER #SAMPLE,

#define SAMPLES(SAMPLE)      \
                        SAMPLE(game_over.wav)\
                        SAMPLE(pong.wav) \


char *waveFileNames[] = {
    SAMPLES(GEN_FULL_SAMPLE_PATH)
};

char *fullWaveFileNames[NUM_WAVEFORMS] = {0};

static void logSDLError(char *msg) {
    if(msg)
        fprintf(stderr, "[ERROR] %s, %s\n", msg, SDL_GetError());
}

Mix_Chunk *samples[NUM_WAVEFORMS] = {0};

void vInitAudio(char *bin_dir_str) {
    size_t bin_dir_len = strlen(bin_dir_str);

    for (unsigned int i = 0; i < NUM_WAVEFORMS; i++){
        fullWaveFileNames[i] = 
            calloc(1, sizeof(char) * 
                    (strlen(waveFileNames[i]) + bin_dir_len + 1));
        strcpy(fullWaveFileNames[i], bin_dir_str);
        strcat(fullWaveFileNames[i], waveFileNames[i]);
    }

    int result = Mix_OpenAudio(22050, AUDIO_S16SYS, AUDIO_CHANNELS, 512);

    if (result){
        logSDLError("Mix_OpenAudio");
        exit(EXIT_FAILURE);
    }

    result = Mix_AllocateChannels(MIXING_CHANNELS);

    if (result < 0){
        logSDLError("Mix_AllocateChannels");
        exit(EXIT_FAILURE);
    }

    for (unsigned int i = 0; i < NUM_WAVEFORMS; i++){
        samples[i] = Mix_LoadWAV(fullWaveFileNames[i]);
        if (!samples[i]){
            logSDLError("MixLoadWAV");
            exit(EXIT_FAILURE);
        }
    }
}

void vExitAudio(void) {

    for (unsigned int i = 0; i < NUM_WAVEFORMS; i++)
        Mix_FreeChunk(samples[i]);
}

void vPlayHit(void) {
    Mix_PlayChannel(-1, samples[1], 0);
}

void vPlayFail(void) {
    Mix_PlayChannel(-1, samples[0], 0);
}