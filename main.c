#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <fcntl.h>
#include <sndfile.h>
#include <unistd.h>
#include <portaudio.h>


#define FRAMES 256

struct AudioData {
    SNDFILE *file;
    SF_INFO *info;
    float *buffer;
};

int pacallback(const void *input, void *output, unsigned long frameCount, const PaStreamCallbackTimeInfo *timeInfo,
               PaStreamCallbackFlags statusFlags, void *userData) {

    sf_count_t rdsampl;
    float *out = (float*)output;
    struct AudioData *data = (struct AudioData*)userData;

    rdsampl = sf_read_float(data->file, out, frameCount * data->info->channels);
    //*out = *data->buffer;

    if (rdsampl < frameCount) {
        return paComplete;
    }

    return paContinue;
}

int main(int argc, char *argv[]) {
    char *audpath;
    struct SF_INFO audinf;
    SNDFILE *audfl;
    float *samples;
    PaStream *stream;
    PaError err;
    int dev_null, bak_stdout;

    dev_null = open("/dev/null", O_WRONLY);

    printf("C-DSPEQ\n");
    printf("Using %s\n", Pa_GetVersionInfo()->versionText);
    if (argc < 2) {
        printf("Missing file to be processed!\n");
        return 1;
    }

    audpath = argv[1];
    printf("Reading file at %s\n", audpath);

    audinf.format = 0;
    audfl = sf_open(audpath, SFM_READ, &audinf);
    printf("Sampling rate: %d\nChannels: %d\n", audinf.samplerate, audinf.channels);

    unsigned long smplsize = sizeof(float) * audinf.samplerate * audinf.channels;
    unsigned long buffsize =  smplsize * FRAMES;
    samples = malloc(buffsize);
    if (samples == NULL) {
        printf("Error allocating memory\n");
        return 2;
    }

    fflush(stderr);
    bak_stdout = dup(STDERR_FILENO);
    dup2(dev_null, STDERR_FILENO);
    close(dev_null);

    err = Pa_Initialize();
    if (err != paNoError) {
        printf("PortAudio error: %s\n", Pa_GetErrorText(err));
        return 1;
    }
    fflush(stderr);
    dup2(bak_stdout, STDERR_FILENO);
    close(bak_stdout);

    struct AudioData data;
    data.file = audfl;
    data.info = &audinf;
    data.buffer = samples;

    err = Pa_OpenDefaultStream(
            &stream,
            0,
            audinf.channels,
            paFloat32,
            audinf.samplerate,
            FRAMES,
            pacallback,
            &data
    );
    if (err != paNoError) {
        printf("Error opening audio stream: %s\n", Pa_GetErrorText(err));
        return 1;
    }



    err = Pa_StartStream(stream);
    while(Pa_IsStreamActive(stream))
    {
        Pa_Sleep(100);
    }


    sf_close(audfl);
    err = Pa_CloseStream(stream);
    if (err != paNoError) {
        printf("Error closing audio stream %s\n", Pa_GetErrorText(err));
        return 1;
    }
    err = Pa_Terminate();
    if (err != paNoError) {
        printf("PortAudio error: %s\n", Pa_GetErrorText(err));
        return 1;
    }

    return 0;
}
