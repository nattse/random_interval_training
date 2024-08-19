import yaml
import os
import sys
import threading
import datetime
import serial
import time
import subprocess
import signal

prompt = 'Invalid arguments\nCall this program with the following two (2) arguments:\
         \n[name of config file in /configs folder, no file extension] [output file name]'

if len(sys.argv) != 2:
    print(prompt)
    raise NameError(prompt)
    
working_dir = os.path.split(__file__)[0]                                       # If we are in the directory the run_script.py file is in, os.path.split will return just ''
if working_dir:                                                                # In the case that we are not in the same location as where the code is,
    os.chdir(working_dir)                                                      # We need to switch so that the yaml file is present and so that we can use the cwd as a default output location
working_dir = os.getcwd()
with open('config.yaml' , 'r') as file:
    settings = yaml.safe_load(file)
if not settings['output_folder']:                                              # If left blank in config.yaml, just output everything
    settings['output_folder'] = working_dir                                    # in the current working directory
start_time = time.time()

def start_logging():
    #filename = os.path.join(settings['output_folder'], sys.argv[1])
    filename = os.path.join(settings['output_folder'], 'test')
    log = open(filename + '.txt', 'w')
    log.write(filename + '.txt\n')
    log.write(f'Experiment began: {datetime.datetime.now().strftime("%d/%m/%Y %H:%M:%S")}\n')
    log.write(f'Using this config_file: {os.path.join(working_dir, "config.yaml")}\n')
    for key, value in settings.items():
        log.write(f'   {key} : {value}\n')
    return log

def arduino_setup(log):
    print('AS: starting')
    try:
        started = False
        ser = serial.Serial(f'/dev/{settings["arduino"]}', 9600, timeout=0.5)
        ser.setDTR(False)                                                      # Need to force the arduino to reset and start from the top
        time.sleep(2)
        ser.flushInput()
        ser.setDTR(True)
        while True:
            if ardu_stop.is_set():
                print('AS: Terminating early...')
                break
            reports = ser.readline().decode().strip()
            if len(reports) > 1:
                if reports == 'Waiting...':
                    print('AS: Arduino is set up and waiting for go signal')
                    ardu_rts.set()
                print(reports)
                log.write(f'{reports}\n')
                if reports == 'FINISHED':
                    ardu_stop.set()
                    print('AS: Arduino finished normally')
                    break
            if ardu_rts.is_set() and cam_rts.is_set() and not started:
                ser.write(bytes('g', encoding='utf8'))
                started = True
                log.write(f'AS: Arduino go signal sent at {time.time()}')
        ser.close()
        print('AS: Successfully closed')
    except (KeyboardInterrupt, SystemExit):
        print('AS: Something really went wrong here...')
        ser.close()
        log.close()
        time.sleep(2)
        sys.exit('AS: Ended via KeyboardInterrupt')
    
def video_setup(log, ardu_thread):
    started = False
    try:
        gstr_arg = f'gst-launch-1.0 --gst-debug-level=4 v4l2src device={settings["camera"]} num-buffers=-1 do-timestamp=true ! image/jpeg,width=1920,height=1080,framerate=30/1 ! queue ! avimux ! filesink location={filename + ".avi"} -e'
        print(gstr_arg + '\n')
        while not ardu_rts.is_set():
            pass
        print('VS: Got the arduino RTS')
        #gstr_cmd = gstr_arg.split()
        #video_capture = subprocess.Popen(gstr_cmd, stdout = subprocess.PIPE, stderr = subprocess.PIPE, universal_newlines = True)
        video_capture = subprocess.Popen('echo hello'.split(), stdout = subprocess.PIPE, stderr = subprocess.PIPE, universal_newlines = True)
        vid_id = video_capture.pid
        gst_time = time.time()
        outputs = []
        while not ardu_stop.is_set():
            output = video_capture.stderr.readline()
            output = "completed state change to PLAYING"
            outputs.append(output)
            if 'error' in output and 'is busy' in output:                      # Prefer to get this error over a general EOS error
                os.kill(vid_id, signal.SIGINT)
                print(f'VS: Your device is busy doing something else\n\n{output}')
                break
            if time.time() - gst_time > 10:                                    # So we only check for general errors after some time
                if 'Waiting for EOS' in video_capture.stdout.readline():
                    os.kill(vid_id, signal.SIGINT)
                    print(f'VS: Some error encountered\n\n{output}')
                    break
            if video_capture.poll() is not None:
                print('VS: Some error occured but killed the process naturally, check out these errors:\n\n')
                possible_problems = [i for i in outputs if 'ERROR' in i or 'Error' in i or 'error' in i]
                print(*possible_problems)
                break
            if ("completed state change to PLAYING" in output) and not started: # Or try: "v4l2src gstv4l2src.c:957:gst_v4l2src_create:<v4l2src0>"
                print('VS: starting for the first time')
                cam_rts.set()
                log.write(f'Cam recording started at {time.time()}')
                started = True
        sys.exit('Ended via completion of Arduino protocol')
    except (KeyboardInterrupt, SystemExit):
        print('VS: video stopped')
        try:
            if video_capture.poll is None:
                os.kill(vid_id,signal.SIGINT)
        except:
            pass
        ardu_stop.set()
        ardu_thread.join()
        log.close()
        sys.exit('VS: Ended via KeyboardInterrupt')
        
def check_filenames():
    #filename = os.path.join(settings['output_folder'], sys.argv[1])
    filename = os.path.join(settings['output_folder'], 'test')
    #if os.path.isfile(filename + '.txt'):
    #    raise NameError(f'File {filename}.txt already exists')
    #if os.path.isfile(filename + '.avi'):
    #    raise NameError(f'File {filename}.avi already exists')
    return filename
    
filename = check_filenames()
print('done checking filenames')
log = start_logging()
print('started log')
if __name__ == "__main__":
    ardu_rts = threading.Event()
    ardu_stop = threading.Event()
    cam_rts = threading.Event()
    ardu_thread = threading.Thread(target = arduino_setup, args = (log,))      # Don't forget that args needs to be a tuple, meaning (args) needs to be (args,)
    ardu_thread.start()
    video_setup(log, ardu_thread)