import serial
import numpy as np
import matplotlib.pyplot as plt
import time
PORT  = "COM4" 
BAUD  = 115200
N     = 400    
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
with serial.Serial(PORT, BAUD, timeout=10) as ser:
    time.sleep(2)           # let Pico boot
    ser.reset_input_buffer()

    # Send sample count
    ser.write(f"{N}\n".encode())
    print(f"Sent: {N} samples")

    # Read lines until DONE sentinel
    lines = []
    while True:
        line = ser.readline().decode().strip()
        if line == "DONE":
            break
        if line:
            lines.append(line)

print(f"Received {len(lines)} lines")
# Parse
indices, timestamps, filtered = [], [], []
for line in lines:
    parts = line.split()
    if len(parts) == 3:
        indices.append(int(parts[0]))
        timestamps.append(int(parts[1]))
        filtered.append(int(parts[2]))

t   = np.array(timestamps, dtype=float)
iir = np.array(filtered,   dtype=float)

t -= t[0]
t_sec = t / 1000.0 
fs = (len(t) - 1) / t_sec[-1]
print(f"Actual sample rate: {fs:.1f} Hz  (target 80 Hz)")
plt.plot(t,iir,'b*-')
plt.xlabel('Time [s]')
plt.ylabel('Signal')
plt.title('Signal vs Time')
plt.show()
#fft(t, iir)