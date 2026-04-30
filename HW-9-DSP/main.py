import matplotlib.pyplot as plt
import numpy as np
import csv

def readCsv(file_name, t, data1):
    with open(file_name) as f:
        reader = csv.reader(f)
        for row in reader:
            t.append(float(row[0]))
            data1.append(float(row[1]))

def fft(time, signal):
    Fs = 10000
    Ts = 1.0/Fs
    ts = np.arange(0, time[-1], Ts)
    y = np.interp(ts, time, signal)
    n = len(y)
    k = np.arange(n)
    T = n/Fs
    frq = k/T
    frq = frq[range(int(n/2))]
    Y = np.fft.fft(y)/n
    Y = Y[range(int(n/2))]
    fig, (ax1, ax2) = plt.subplots(2, 1)
    ax1.plot(ts, y, 'b')
    ax1.set_xlabel('Time')
    ax1.set_ylabel('Amplitude')
    ax2.loglog(frq, abs(Y), 'b')
    ax2.set_xlabel('Freq (Hz)')
    ax2.set_ylabel('|Y(freq)|')
    plt.show()
def MovingAverageFilt(data, x):
    datanew = []
    p = len(data)/x
    h = 0
    l = 0
    for i in range(len(data)):
        h += data[i]
        l+=1
        if(l==x or i==len(data)-1):
            h/=x
            datanew.append(h)
            h=0
            l=0
    return datanew

def IIR(data, A, B):

    datanew = []
    y_prev = 0
    for i in range(len(data)):
        y = A * data[i] + (B) * y_prev
        datanew.append(y)
        y_prev = y
    return datanew
def FIR(data, h):
    n = len(data)
    m = len(h)
    datanew = []
    for i in range(n):
        y = 0.0
        for j in range(m):
            if i - j >= 0:
                y += h[j] * data[i - j]
        datanew.append(y)
    return datanew
tA, dataA = [], []
readCsv("sigA.csv", tA, dataA)
fft(tA, dataA)
tB, dataB = [], []
readCsv("sigB.csv", tB, dataB)
fft(tB, dataB)

tC, dataC = [], []
readCsv("sigC.csv", tC, dataC)
fft(tC, dataC)

tD, dataD = [], []
readCsv("sigD.csv", tD, dataD)
fft(tD, dataD)

MAFA = MovingAverageFilt(dataA, 100)

MAFB = MovingAverageFilt(dataB, 100)

MAFC = MovingAverageFilt(dataC, 10)

MAFD = MovingAverageFilt(dataD, 100)

IIRA = IIR(dataA, 0.1, .9)

IIRB = IIR(dataB, 0.1, .9)

IIRC = IIR(dataC, 0.1, .9)

IIRD = IIR(dataD, 0.1, .9)

h = [
    0.000000000000000000,
    0.000025695329810175,
    0.000109001586731170,
    0.000261383834469497,
    0.000497284187006939,
    0.000834034055775751,
    0.001291444409523049,
    0.001891070645768172,
    0.002655173095064218,
    0.003605418278370960,
    0.004761388110941483,
    0.006138982684280024,
    0.007748815659053334,
    0.009594708572398906,
    0.011672390798343837,
    0.013968505240990876,
    0.016460006299700354,
    0.019114016903342568,
    0.021888186575993940,
    0.024731564041054279,
    0.027585967536853457,
    0.030387805705078649,
    0.033070273559075526,
    0.035565823484471772,
    0.037808792095928072,
    0.039738051371860414,
    0.041299547699896848,
    0.042448595699213505,
    0.043151804843812071,
    0.043388535390380012,
    0.043151804843812071,
    0.042448595699213505,
    0.041299547699896848,
    0.039738051371860407,
    0.037808792095928072,
    0.035565823484471785,
    0.033070273559075540,
    0.030387805705078645,
    0.027585967536853460,
    0.024731564041054296,
    0.021888186575993951,
    0.019114016903342579,
    0.016460006299700354,
    0.013968505240990888,
    0.011672390798343854,
    0.009594708572398910,
    0.007748815659053343,
    0.006138982684280031,
    0.004761388110941481,
    0.003605418278370961,
    0.002655173095064216,
    0.001891070645768171,
    0.001291444409523047,
    0.000834034055775751,
    0.000497284187006940,
    0.000261383834469497,
    0.000109001586731170,
    0.000025695329810175,
    0.000000000000000000,
]

FIRA = FIR(dataA, h)


FIRB = FIR(dataB, h)


FIRC = FIR(dataC, h)


FIRD = FIR(dataD, h)
# --- Plotting filtered vs unfiltered ---

def plot_filtered(time, original, filtered, title):
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(10, 6))
    fig.suptitle(title)
    
    ax1.plot(time, original, 'k', label='Unfiltered')
    ax1.set_xlabel('Time')
    ax1.set_ylabel('Amplitude')
    ax1.legend()
    ax1.set_title('Unfiltered')

    t_filt = time[:len(filtered)]
    ax2.plot(t_filt, filtered, 'r', label='Filtered')
    ax2.set_xlabel('Time')
    ax2.set_ylabel('Amplitude')
    ax2.legend()
    ax2.set_title('Filtered')

    plt.tight_layout()
    plt.show()


# Compute FIR cutoff frequency for title
# h is a low-pass FIR; cutoff ≈ Fs * (sum of h up to center, normalized)
# Simpler: estimate from frequency response
Fs = 10000
H = np.fft.fft(h, 1024)
freqs = np.fft.fftfreq(1024, d=1/Fs)
mag = np.abs(H[:512])
half_power = mag.max() / np.sqrt(2)
cutoff_idx = np.argmin(np.abs(mag - half_power))
fir_cutoff = freqs[cutoff_idx]

# MAF plots
plot_filtered(tA, dataA, MAFA, f'MAF - Signal A (N=100 points)')
plot_filtered(tB, dataB, MAFB, f'MAF - Signal B (N=100 points)')
plot_filtered(tC, dataC, MAFC, f'MAF - Signal C (N=10 points)')
plot_filtered(tD, dataD, MAFD, f'MAF - Signal D (N=100 points)')

# IIR plots (A=0.1, B=0.9)
plot_filtered(tA, dataA, IIRA, f'IIR - Signal A (A=0.1, B=0.9)')
plot_filtered(tB, dataB, IIRB, f'IIR - Signal B (A=0.1, B=0.9)')
plot_filtered(tC, dataC, IIRC, f'IIR - Signal C (A=0.1, B=0.9)')
plot_filtered(tD, dataD, IIRD, f'IIR - Signal D (A=0.1, B=0.9)')

# FIR plots
plot_filtered(tA, dataA, FIRA, f'FIR - Signal A (cutoff ≈ {fir_cutoff:.1f} Hz)')
plot_filtered(tB, dataB, FIRB, f'FIR - Signal B (cutoff ≈ {fir_cutoff:.1f} Hz)')
plot_filtered(tC, dataC, FIRC, f'FIR - Signal C (cutoff ≈ {fir_cutoff:.1f} Hz)')
plot_filtered(tD, dataD, FIRD, f'FIR - Signal D (cutoff ≈ {fir_cutoff:.1f} Hz)')
