# FanController
A fan controller program for controlling fans based on temperature sensors using boost IPC. I have written a small gui based linux application to test it.

# Requirements:
We have an industrial system with many subsystems that each generate their own individual temperature measurement.
There are also multiple fans onboard the robot that are used to cool the electronics to prevent overheating.
Because the fans are very loud when running at 100% duty cycle, and because the robot operates alongside
humans, the fan speeds are set so that the noise level is minimized without endangering the electronics.

Tasks:
Develop an application to control fan speeds. The application should meet the following
requirements:

• The temperature of each subsystem is provided as a 32-bit floating point number in °C via IPC.\
• The number of subsystems and the number of fans present should both be configurable at startup, but you
may assume that each of these numbers has an upper bound. You may assume that the number of each
is constant after startup.\
• The speed of each fan is set by writing a 32-bit unsigned integer to a specific hardware register that is
different for each fan. This integer is in PWM counts and is proportional to fan duty cycle.\
• The PWM counts corresponding to 100% duty cycle may be different for different fans. You may assume
that 0 PWM counts always represents 0% duty cycle\
The fan control algorithm should behave as follows:\
• The most recent temperature measurements from each subsystem should be collected, and the fan duty
cycle should be computed from the maximum of the most recent temperatures of all subsystems\
• All fans should be set to the same duty cycle.\
• If the temperature is 25° C or below, the fans should run at 20% duty cycle\
• If the temperature is 75° C or above, the fans should run at 100% duty cycle.\
• If the maximum measured subsystem temperature is in between 25° C and 75° C, the fans should run at a
duty cycle linearly interpolated between 20% and 100% duty cycle.\


This project can be run on LINUX only, since only the linux bindings needed for the GUI library is included.
GUI library (Dear ImGui) is a cut down version from https://github.com/ocornut/imgui/tree/master/docs (All code inside folder fan_controller/external.)


# How to Build

    On linux
        cd fan_controller
        make
        ./fan_controller "sensor_count" "fan_count"

# Tuning 
To Change maximum values for sensor, fan counts and other configs - Refer to fan_controller/common.h

# Dependencies 
Requires boost installation - After that in case of errors related to libboost symlink creation may be required like in ex: below:
    sudo ln -s /usr/lib/x86_64-linux-gnu/libboost_thread.so.1.65.1 /usr/lib/x86_64-linux-gnu/libboost_thread.so

# Notes
In case of sdl errors please install
    sudo apt-get install libglfw3 libglfw3-dev
    sudo apt-get install libsdl2-dev
