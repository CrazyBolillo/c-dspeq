# Design and basic testing for the four filters implemented in this project

from scipy.signal import iirfilter, freqz

import matplotlib.pyplot as plt
import numpy as np

SAMPLING_FREQ = 48000

np.set_printoptions(suppress=True)


def plot(fig, title, axis, hlines, vlines, x, y):
    ax = fig.add_subplot(1, 1, 1)
    ax.semilogx(x, 20 * np.log10(np.maximum(abs(y), 1e-5)))
    ax.set_title(title)
    ax.set_xlabel('Frequency [Hz]')
    ax.set_ylabel('Amplitude [dB]')
    ax.axis(axis)
    ax.grid(which='both', axis='both')

    for hline in hlines:
        ax.axhline(y=hline[0], xmin=hline[1], xmax=hline[2], color=hline[3])

    for vline in vlines:
        ax.axvline(x=vline[0], ymin=vline[1], ymax=vline[2], color=vline[3])


# ---------------
# Low pass design
# ----------------
lpb, lpa = iirfilter(4, 200, btype='lowpass', ftype='butter', output='ba', fs=SAMPLING_FREQ)
print("- Low-pass filter. Fc = 200, Fs = 48000")
print(f"B: {lpb}")
print(f"A: {lpa}")
w, h = freqz(lpb, lpa, fs=SAMPLING_FREQ)
fig = plt.figure()
plot(
    fig,
    '4th order Butterworth Frequency Response',
    (10, 10000, -100, 10),
    [(-3, 0, 1, 'r'), (-24, 0, 1, 'r')],
    [(500, 0, 1, 'r'), (1000, 0, 1, 'r')],
    w,
    h
)
plt.show()

# ----------------
# High pass design
# ----------------
hpb, hpa = iirfilter(4, 10000, btype='highpass', ftype='butter', output='ba', fs=SAMPLING_FREQ)
print("- High-pass filter. Fc = 10000, Fs = 48000")
print(f"B: {hpb}")
print(f"A: {hpa}")
w, h = freqz(hpb, hpa, fs=SAMPLING_FREQ)
fig = plt.figure()
plot(
    fig,
    '4th order Butterworth Frequency Response',
    (1000, 20000, -100, 10),
    [(-3, 0, 1, 'r'), (-24, 0, 1, 'r')],
    [(10000, 0, 1, 'r'), (5000, 0, 1, 'r')],
    w,
    h
)
plt.show()

# ---------------
# Bandpass design
# Using SOS
# ---------------
bp1sos = iirfilter(2, [300, 2000], btype='bandpass', ftype='butter', output='sos', fs=SAMPLING_FREQ)
print("Bandpass filter. Fc = 300 & 2000 Hz")
print(bp1sos)

# ---------------
# Bandpass design
# Using SOS
# ---------------
bp2sos = iirfilter(2, [2000, 6000], btype='bandpass', ftype='butter', output='sos', fs=SAMPLING_FREQ)
print("Bandpass filter. Fc = 2000 & 5000 Hz")
print(bp2sos)
