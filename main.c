#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <fcntl.h>
#include <sndfile.h>
#include <unistd.h>
#include <portaudio.h>

#define HANDLE_PA_ERROR(x) if (x != paNoError) { printf("PortAudio error: %s", Pa_GetErrorText(x)); return 1; }
#define FRAMES 256

struct AudioData {
    SNDFILE *file;
    SF_INFO *info;
    float *buffer;
};

float lowpass_filter(float sample);
float highpass_filter(float sample);
float bandpass1_filter(float sample);
float bandpass2_filter(float sample);

int pacallback(const void *input, void *output, unsigned long frameCount, const PaStreamCallbackTimeInfo *timeInfo,
               PaStreamCallbackFlags statusFlags, void *userData) {

    sf_count_t i;
    sf_count_t rdsampl;
    float *out = (float*)output;
    struct AudioData *data = (struct AudioData*)userData;
    float* buffer_cp = data->buffer;

    rdsampl = sf_read_float(data->file, data->buffer, frameCount * data->info->channels);
    for (i = 0; i < rdsampl; i++) {
        *out++ = highpass_filter(*buffer_cp++);
    }

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
    HANDLE_PA_ERROR(err)

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
    HANDLE_PA_ERROR(err)



    err = Pa_StartStream(stream);
    HANDLE_PA_ERROR(err)
    while (Pa_IsStreamActive(stream)) {

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

/**
 * Fourth order IIR Butterworth filter.
 * Fc = 500Hz
 * Fs = 48000
 * @param sample
 * @return
 */
float lowpass_filter(float sample) {
    const float b0 = 0.00000105;
    const float b1 = 0.00000422;
    const float b2 = 0.00000633;
    const float b3 = 0.00000422;
    const float b4 = 0.00000105;
    const float a1 = -3.8289861;
    const float a2 = 5.50142959;
    const float a3 = -3.51519387;
    const float a4 = 0.84276724;

    static float x1, x2, x3, x4;
    static float y1, y2, y3, y4;
    float output = 0;

    output += b0 * sample;
    output += b1 * x1;
    output += b2 * x2;
    output += b3 * x3;
    output += b4 * x4;
    output -= a1 * y1;
    output -= a2 * y2;
    output -= a3 * y3;
    output -= a4 * y4;

    x4 = x3;
    x3 = x2;
    x2 = x1;
    x1 = sample;

    y4 = y3;
    y3 = y2;
    y2 = y1;
    y1 = output;

    return output;
}

/**
 * Fourth order Butterworth filter
 * Fc = 10kHz
 * Fs = 48000
 * @param sample
 * @return
 */
float highpass_filter(float sample) {
    const float b0 = 0.15284324;
    const float b1 = -0.61137298;
    const float b2 = 0.91705946;
    const float b3 = -0.61137298;
    const float b4 = 0.15284324;
    const float a1 = -0.65147165;
    const float a2 = 0.62047212;
    const float a3 = -0.14737946;
    const float a4 = 0.02616866;

    static float x1, x2, x3, x4;
    static float y1, y2, y3, y4;
    float output = 0;

    output += b0 * sample;
    output += b1 * x1;
    output += b2 * x2;
    output += b3 * x3;
    output += b4 * x4;
    output -= a1 * y1;
    output -= a2 * y2;
    output -= a3 * y3;
    output -= a4 * y4;

    x4 = x3;
    x3 = x2;
    x2 = x1;
    x1 = sample;

    y4 = y3;
    y3 = y2;
    y2 = y1;
    y1 = output;

    return output;
}
