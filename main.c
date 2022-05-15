#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sndfile.h>
#include <unistd.h>
#include <portaudio.h>
#include <string.h>

#define HANDLE_PA_ERROR(x) if (x != paNoError) { printf("PortAudio error: %s\n", Pa_GetErrorText(x)); return 1; }
#define FRAMES 256

struct AudioData {
    SNDFILE *file;
    SF_INFO *info;
    float *buffer;
    float lowpass;
    float highpass;
    float bandpass1;
    float bandpass2;
};

float passall_filter(float sample);
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
    float (*filter)(float);

    rdsampl = sf_read_float(data->file, data->buffer, frameCount * data->info->channels);
    if (data->lowpass == 1) {
        filter = &lowpass_filter;
    }
    else if (data->highpass == 1) {
        filter = &highpass_filter;
    }
    else if (data -> bandpass1 == 1) {
        filter = &bandpass1_filter;
    }
    else if (data -> bandpass2 == 1) {
        filter = &bandpass2_filter;
    }
    else {
        filter = &passall_filter;
    }
    for (i = 0; i < rdsampl; i++) {
        *out++ = (*filter)(*buffer_cp++);
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
    char uinput[256];


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

    // Hack to hide non-critical errors printed by PortAudio
    fflush(stderr);
    dev_null = open("/dev/null", O_WRONLY);
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
    data.lowpass = 0;
    data.highpass = 0;

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
    printf("Playing\n\n");
    while (Pa_IsStreamActive(stream)) {
        printf("$> ");
        fgets(uinput, 255, stdin);

        if (strncmp(uinput, "lp", 2) == 0) {
            data.highpass = 0;
            data.bandpass1 = 0;
            data.bandpass2 = 0;
            data.lowpass = 1;
        }
        else if (strncmp(uinput, "hp", 2) == 0) {
            data.highpass = 1;
            data.bandpass1 = 0;
            data.bandpass2 = 0;
            data.lowpass = 0;
        }
        else if (strncmp(uinput, "bp1", 3) == 0) {
            data.highpass = 0;
            data.bandpass1 = 1;
            data.bandpass2 = 0;
            data.lowpass = 0;
        }
        else if (strncmp(uinput, "bp2", 3) == 0) {
            data.highpass = 0;
            data.bandpass1 = 0;
            data.bandpass2 = 1;
            data.lowpass = 0;
        }
        else if (strncmp(uinput, "none", 4) == 0 ) {
            data.highpass = 0;
            data.bandpass1 = 0;
            data.bandpass2 = 0;
            data.lowpass = 0;
        }
        else if (strncmp(uinput, "exit", 4) == 0) {
            break;
        }
    }

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
    sf_close(audfl);

    return 0;
}

float passall_filter(float sample) { return sample; }


/**
 * Fourth order IIR Butterworth filter.
 * Fc = 500Hz
 * Fs = 48000
 * @param sample
 * @return
 */
float lowpass_filter(float sample) {
    const float b0 = 0.00000105f;
    const float b1 = 0.00000422f;
    const float b2 = 0.00000633f;
    const float b3 = 0.00000422f;
    const float b4 = 0.00000105f;
    const float a1 = -3.8289861f;
    const float a2 = 5.50142959f;
    const float a3 = -3.51519387f;
    const float a4 = 0.84276724f;

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
    const float b0 = 0.15284324f;
    const float b1 = -0.61137298f;
    const float b2 = 0.91705946f;
    const float b3 = -0.61137298f;
    const float b4 = 0.15284324f;
    const float a1 = -0.65147165f;
    const float a2 = 0.62047212f;
    const float a3 = -0.14737946f;
    const float a4 = 0.02616866f;

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
 * Second order butterworth filter.
 * Fc = 300 and 2000 Hz
 * Fs = 48000
 * @param sample
 * @return
 */
float bandpass1_filter(float sample) {
    // First SOS
    const float s1b0 = 0.01066456f;
    const float s1b1=  0.02132913f;
    const float s1b2 = 0.01066456f;
    const float s1a1 = -1.71860877f;
    const float s1a2 = 0.76714263f;

    static float s1x1, s1x2, s1y1, s1y2;
    float s1out = 0;

    s1out += s1b0 * sample;
    s1out += s1b1 * s1x1;
    s1out += s1b2 * s1x2;
    s1out -= s1a1 * s1y1;
    s1out -= s1a2 * s1y2;

    s1x2 = s1x1;
    s1x1 = sample;
    s1y2 = s1y1;
    s1y1 = s1out;

    // Second SOS
    const float s2b0 = 1;
    const float s2b1=  -2;
    const float s2b2 = 1;
    const float s2a1 = -1.94973513f;
    const float s2a2 = 0.95160794f;

    static float s2x1, s2x2, s2y1, s2y2;
    float s2out = 0;

    s2out += s2b0 * s1out;
    s2out += s2b1 * s2x1;
    s2out += s2b2 * s2x2;
    s2out -= s2a1 * s2y1;
    s2out -= s2a2 * s2y2;

    s2x2 = s2x1;
    s2x1 = s1out;
    s2y2 = s2y1;
    s2y1 = s2out;

    return s2out;
}

/**
 * Second order butterworth filter.
 * Fc = 2000 and 5000 Hz
 * Fs = 48000
 * @param sample
 * @return
 */
float bandpass2_filter(float sample) {
    // First SOS
    const float s1b0 = 0.02995458f;
    const float s1b1=  0.05990916f;
    const float s1b2 = 0.02995458f;
    const float s1a1 = -1.40872235f;
    const float s1a2 = 0.69344263f;

    static float s1x1, s1x2, s1y1, s1y2;
    float s1out = 0;

    s1out += s1b0 * sample;
    s1out += s1b1 * s1x1;
    s1out += s1b2 * s1x2;
    s1out -= s1a1 * s1y1;
    s1out -= s1a2 * s1y2;

    s1x2 = s1x1;
    s1x1 = sample;
    s1y2 = s1y1;
    s1y1 = s1out;

    // Second SOS
    const float s2b0 = 1;
    const float s2b1=  -2;
    const float s2b2 = 1;
    const float s2a1 = -1.74998831f;
    const float s2a2 = 0.82784342f;

    static float s2x1, s2x2, s2y1, s2y2;
    float s2out = 0;

    s2out += s2b0 * s1out;
    s2out += s2b1 * s2x1;
    s2out += s2b2 * s2x2;
    s2out -= s2a1 * s2y1;
    s2out -= s2a2 * s2y2;

    s2x2 = s2x1;
    s2x1 = s1out;
    s2y2 = s2y1;
    s2y1 = s2out;

    return s2out;
}