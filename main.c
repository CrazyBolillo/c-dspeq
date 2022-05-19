#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sndfile.h>
#include <unistd.h>
#include <portaudio.h>
#include <string.h>
#include <signal.h>
#include <stdbool.h>

#define HANDLE_PA_ERROR(x) if ((x) != paNoError) { printf("PortAudio error: %s\n", Pa_GetErrorText(x)); raise(SIGTERM); }
#define FRAMES 256

struct SF_INFO audinf;
SNDFILE *audfl;
float *samples;
PaStream *stream;
PaError err;

struct AudioData {
    SNDFILE *file;
    SF_INFO *info;
    float *buffer;
    float lowpass;
    float highpass;
    float bandpass1;
    float bandpass2;
    bool stop;
} audiodt;


float lowpass_filter(float sample);
float highpass_filter(float sample);
float bandpass1_filter(float sample);
float bandpass2_filter(float sample);

void sigterm_handler(int signum);
void sigint_handler(int signum);

int pacallback(const void *input, void *output, unsigned long frameCount, const PaStreamCallbackTimeInfo *timeinfo,
               PaStreamCallbackFlags flags, void *userdata) {

    sf_count_t i;
    int filters;
    float filsum;
    sf_count_t rdsampl;
    float *out = (float*)output;
    struct AudioData *data = (struct AudioData*)userdata;
    float* buffer_cp = data->buffer;

    rdsampl = sf_read_float(data->file, data->buffer, frameCount * data->info->channels);

    for (i = 0; i < rdsampl; i++) {
        filters = 0;
        filsum = 0;
        if (data->lowpass != 0) {
            filsum += data->lowpass * lowpass_filter(*buffer_cp);
            filters++;
        }
        if (data->highpass != 0) {
            filsum += data->highpass * highpass_filter(*buffer_cp);
            filters++;
        }
        if (data -> bandpass1 != 0) {
            filsum += data->bandpass1 * bandpass1_filter(*buffer_cp);
            filters++;
        }
        if (data -> bandpass2 != 0) {
            filsum += data->bandpass2 * bandpass2_filter(*buffer_cp);
            filters++;
        }
        if (filters == 0) {
            filsum = *buffer_cp;
        }
        else {
            filsum /= (float) filters;
        }
        *out++ = filsum;
        buffer_cp++;
    }

    if (rdsampl < frameCount) {
        printf("\n\nTrack has ended");
        return paComplete;
    }
    else if (data->stop) {
        return paComplete;
    }

    return paContinue;
}

void pafinishcall(void *userdata) {
    raise(SIGTERM);
}

int main(int argc, char *argv[]) {
    char *audpath;
    int dev_null, bak_stdout;
    char uinput[256];
    char command[6];
    float gain;
    int matches;

    signal(SIGTERM, sigterm_handler);
    signal(SIGINT, sigint_handler);

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

    audiodt.file = audfl;
    audiodt.info = &audinf;
    audiodt.buffer = samples;
    audiodt.lowpass = 0;
    audiodt.highpass = 0;
    audiodt.bandpass2 = 0;
    audiodt.bandpass1 = 0;
    audiodt.stop = 0;

    err = Pa_OpenDefaultStream(
            &stream,
            0,
            audinf.channels,
            paFloat32,
            audinf.samplerate,
            FRAMES,
            pacallback,
            &audiodt
    );
    HANDLE_PA_ERROR(err)
    err = Pa_SetStreamFinishedCallback(stream, pafinishcall);
    HANDLE_PA_ERROR(err);
    err = Pa_StartStream(stream);
    HANDLE_PA_ERROR(err)
    printf("Playing\n\n");

    while (Pa_IsStreamActive(stream)) {
        printf("$> ");
        fgets(uinput, 255, stdin);
        matches = sscanf(uinput, "%4s %f", command, &gain);

        if (matches == 2) {
            if (strcmp(command, "lp") == 0) {
                audiodt.lowpass = gain;
                continue;
            }
            else if (strcmp(command, "hp") == 0) {
                audiodt.highpass = gain;
                continue;
            }
            else if (strcmp(command, "bp1") == 0) {
                audiodt.bandpass1 = gain;
                continue;
            }
            else if (strcmp(command, "bp2") == 0) {
                audiodt.bandpass2 = gain;
                continue;
            }
        }
        else if (matches == 1) {
            if (strcmp(command, "none") == 0 ) {
                audiodt.highpass = 0;
                audiodt.bandpass1 = 0;
                audiodt.bandpass2 = 0;
                audiodt.lowpass = 0;
                continue;
            }
            else if (strcmp(command, "exit") == 0) {
                break;
            }
        }
        printf("Command not recognized\n");
    }
    sigterm_handler(SIGTERM);

    return 0;
}

void sigterm_handler(int sig ) {
    signal(SIGTERM, SIG_DFL);

    printf("\nStopping\n");
    err = Pa_CloseStream(stream);
    if (err != paNoError) {
        printf("%s\n", Pa_GetErrorText(err));
    }
    err = Pa_Terminate();
    if (err != paNoError) {
        printf("%s\n", Pa_GetErrorText(err));
    }
    sf_close(audfl);
    free(samples);

    raise(SIGTERM);
}

void sigint_handler(int sig) {
    printf("\n");
    audiodt.stop = 1;
}

/**
 * Fourth order IIR Butterworth filter.
 * Fc = 200Hz
 * Fs = 48000
 * @param sample
 * @return
 */
float lowpass_filter(float sample) {
    const float b0 = 0.00000003f;
    const float b1 = 0.00000011f;
    const float b2 = 0.00000017f;
    const float b3 = 0.00000011f;
    const float b4 = 0.00000003f;
    const float a1 = -3.93158947f;
    const float a2 = 5.7970987f;
    const float a3 = -3.79938277f;
    const float a4 = 0.93387399f;

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
