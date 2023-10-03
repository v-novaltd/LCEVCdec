import time
import argparse
import subprocess
import glob
import shutil
from watchdog.observers import Observer
from watchdog.events import PatternMatchingEventHandler

adb = "adb"
watch_path = "../../../src/integration/decoder/shaders"

last_trigger_time = time.time()
should_cleanup = False
device_serial = 0


def init():
    # Startup
    global should_cleanup
    global device_serial
    global adb

    # Check to see if we should run the windows adb.exe (may be prefered for WSL windows)
    if shutil.which("adb.exe") is None:
        print("Using bash adb command")
    else:
        print("Using Windows adb.exe command")
        adb = "adb.exe"

    # Check command line arguments
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--cleanup", help="Cleans up GLSL files left on device on script exit.", action="store_true")
    parser.add_argument("-s", "--serial", help="Device serial")
    args = parser.parse_args()

    should_cleanup = args.cleanup

    # Parse connected device list and get input from user
    device_list = subprocess.run(
        [adb, "devices", "-l"], stdout=subprocess.PIPE, text=True)
    devices = device_list.stdout.split("\n")[1:]
    serials = [x[0:x.find("device")].strip() for x in devices if len(x) > 0]

    if not serials:
        print("No connected devices found!")
        exit(-1)

    if args.serial and len(args.serial) > 0:
        # Check if device serial exists in list
        if args.serial not in serials:
            print("Supplied device serial is not found in list from 'adb devices -l'")
            exit(-2)

        device_serial = args.serial
    else:
        print("Select device [{}-{}]:".format(0, len(serials) - 1))
        for i in range(0, len(serials)):
            print("{}: {}".format(i, devices[i]))

        selected_device = -1
        while (selected_device < 0) or (selected_device >= len(serials)):
            selected_device = int(input())

        print("Selected {}: {}".format(
            selected_device, devices[selected_device]))
        device_serial = serials[selected_device]


class ModifiedFileEventHandler(PatternMatchingEventHandler):
    # Workaround to stop multiple file modified events
    def on_modified(self, event):
        global last_trigger_time
        current_time = time.time()
        if (current_time - last_trigger_time) > 1:
            last_trigger_time = current_time
            push_shader(event.src_path)


def run_observer():
    # Run observer
    patterns = ["*.glsl"]
    ignore_patterns = None
    ignore_directories = False
    case_sensitive = True
    event_handler = ModifiedFileEventHandler(
        patterns, ignore_patterns, ignore_directories, case_sensitive)

    recurse_into_watch_path = True
    observer = Observer()
    observer.schedule(event_handler, watch_path,
                      recursive=recurse_into_watch_path)

    # Start the observer thread and wait until a keyboard interrupt
    # stops the script
    print("Monitoring in {}".format(watch_path))
    observer.start()
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        observer.stop()
        observer.join()


def cleanup():
    # Remove shaders from device
    subprocess.run([adb, "-s", device_serial, "shell",
                    "rm /sdcard/*.glsl"], stdout=subprocess.DEVNULL)
    print("Shaders cleaned up from device {}".format(device_serial))


def push_shader(shader_path):
    # Push shader to device
    subprocess.run([adb, "-s", device_serial, "push",
                    shader_path, "/sdcard"], stdout=subprocess.DEVNULL)
    print("Pushed {} to device.".format(shader_path))


def first_push():
    # Used once at initialisation to push all shaders from dec_il/shaders to the device
    for shader in glob.glob("{}/*.glsl".format(watch_path)):
        push_shader(shader)


def main():
    init()
    first_push()
    run_observer()

    if should_cleanup:
        cleanup()


if __name__ == "__main__":
    main()
