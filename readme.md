# FW for useless box, Arduino NANO:
Project description are [here](https://alexlexx1.gitbook.io/useless-box/)

# Build tools:
https://arduino.github.io/arduino-cli/0.31/installation/

# Addition libraries for build:
* arduino-cli lib install --git-url https://github.com/swissbyte/AccelStepper.git
* arduino-cli lib install --git-url https://github.com/cskarai/asynci2cmaster.git
* arduino-cli lib install --git-url https://github.com/arduino-libraries/Servo.git

# complie:
* arduino-cli compile -b arduino:avr:nano -v ./useless_box_fw.ino


# upload:
arduino-cli upload -p /dev/ttyUSB0 -b arduino:avr:nano:cpu=atmega328old ./useless_box_fw.ino -v
