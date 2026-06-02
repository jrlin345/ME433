import serial
import time
import matplotlib.pyplot as plt
import csv

PORT = "COM8"      # change if your Nucleo is on a different COM port
BAUD = 115200

ser = serial.Serial(PORT, BAUD, timeout=5)
time.sleep(2)

ser.reset_input_buffer()

# Send command to STM32
ser.write(b"a\n")

indices = []
desired = []
current = []
position = []

reading_data = False

while True:
    line = ser.readline().decode(errors="ignore").strip()

    if not line:
        continue

    print(line)

    if line == "index,desired,current,position":
        reading_data = True
        continue

    if line == "Done":
        break

    if "SAFETY STOP" in line:
        print("Motor stopped by safety limit.")
        continue

    if reading_data:
        parts = line.split(",")

        if len(parts) == 4:
            try:
                indices.append(int(parts[0]))
                desired.append(int(parts[1]))
                current.append(int(parts[2]))
                position.append(int(parts[3]))
            except ValueError:
                pass

ser.close()

# Save data as CSV
with open("current_control_data.csv", "w", newline="") as f:
    writer = csv.writer(f)
    writer.writerow(["index", "desired", "current","position"])

    for i, d, c, p in zip(indices, desired, current, position):
        writer.writerow([i, d, c, p])

# Plot
plt.figure()
plt.plot(indices, desired, label="Desired current")
plt.plot(indices, current, label="Measured current")
plt.xlabel("Sample index")
plt.ylabel("Current raw units")
plt.title("Current Control Test")
plt.legend()
plt.grid(True)

plt.show()
