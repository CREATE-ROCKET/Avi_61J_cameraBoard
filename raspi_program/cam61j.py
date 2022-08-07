from picamera import PiCamera
from time import sleep
import os
import RPi.GPIO as GPIO
import time
import subprocess

# gpio init
input_pin = 23
output_pin = 24
GPIO.setmode(GPIO.BCM)
GPIO.setup(input_pin, GPIO.IN, pull_up_down=GPIO.PUD_DOWN)
GPIO.setup(output_pin, GPIO.OUT, initial=GPIO.HIGH)

cp = subprocess.run(['vcgencmd', 'get_camera'], capture_output=True)
if(b'supported=1 detected=1\n' == cp.stdout):
    # camera ok
    GPIO.output(output_pin, GPIO.LOW)

    camera = PiCamera(sensor_mode=4)

    # wait until GPIO goes HIGH.
    while(GPIO.input(input_pin) == GPIO.LOW):
        time.sleep(1)

    # start recording
    print('start recording')
    savedir = '/home/pi/cam61/h264'
    os.makedirs(savedir, exist_ok=True)
    h264_exist_count = sum(os.path.isfile(os.path.join(savedir, name))
                           for name in os.listdir(savedir))
    savefilename = savedir + '/' + str(h264_exist_count+1).zfill(4) + '.h264'
    camera.framerate = 30
    camera.resolution = (1640, 1232)
    camera.start_recording(savefilename)

    # wait until GPIO goes LOW.
    while(GPIO.input(input_pin) == GPIO.HIGH):
        time.sleep(1)

    # stop recording
    print('stop recording')
    camera.stop_recording()
    GPIO.output(output_pin, GPIO.HIGH)
    time.sleep(1)

# wait until GPIO goes LOW.
    while(GPIO.input(input_pin) == GPIO.HIGH):
        time.sleep(1)

else:
    time.sleep(60)

# shutdown
GPIO.cleanup(input_pin)
command_list = ['sudo', 'shutdown', 'now']
subprocess.run(command_list)
