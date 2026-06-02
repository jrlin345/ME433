import time
import matplotlib.pyplot as plt
from statistics import mean
import serial
def read_plot_matrix():
    n_str = ser.read_until(b'\n'); # get the number of data points to receive
    n_int = float(n_str) # turn it into an int
    print('Data length = ' + str(n_int))
    ref = []
    data = []
    data_received = 0
    while data_received < n_int:
        dat_str = ser.read_until(b'\n'); # get the data as a string, ints seperated by spaces
        dat_f = list(map(float, dat_str.split()))
        ref.append(dat_f[1])   # desired_current is column 1
        data.append(dat_f[2])  # actual_current is column 2
        data_received = data_received + 1
    meanzip = zip(ref,data)
    meanlist = []
    for i,j in meanzip:
        meanlist.append(abs(i-j))
    score = mean(meanlist)
    t = range(len(ref)) # index array
    plt.plot(t,ref,'r*-',t,data,'b*-')
    plt.title('Score = ' + str(score))
    plt.ylabel('value')
    plt.xlabel('index')
    plt.show()

ser = serial.Serial('COM8')
selection = input('\n')
read_plot_matrix()