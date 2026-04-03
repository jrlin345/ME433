import board
import pwmio # get access to PWM
import time
# control GP14 with PWM
servo = pwmio.PWMOut(board.GP16, variable_frequency=True)
servo.frequency = 50  # in hz
servo.duty_cycle = 0 # initially off, at 16bit number so max on is 65535

while True:

    for i in range(int(65535*0.5/20), int(65535*2.5/20), 100):
        servo.duty_cycle = i
        time.sleep(0.01)
    for i in range(int(65535*2.5/20), int(65535*0.5/20), -100):
        servo.duty_cycle = i
        time.sleep(0.01)