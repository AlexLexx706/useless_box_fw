# FW for useless box, Arduino NANO

Project description are [here](https://alexlexx1.gitbook.io/useless-box/)

# Addition libraries for build

* arduino-cli lib install --git-url <https://github.com/swissbyte/AccelStepper.git>
* arduino-cli lib install --git-url <https://github.com/cskarai/asynci2cmaster.git>
* arduino-cli lib install --git-url <https://github.com/arduino-libraries/Servo.git>

# complie

* arduino-cli compile -b arduino:avr:nano -v ./useless_box_fw.ino

# upload

arduino-cli upload -p /dev/ttyUSB0 -b arduino:avr:nano:cpu=atmega328old ./useless_box_fw.ino -v

# Command interface

Current FW support simple text command interface like [GRIL interface](https://www.ecomexico.net/proyectos/soporte/TOPCON-SOKKIA/GPS/GRX1/ARRANQUE%20EN%20ESTATICO/GRILver2_3.pdf)

## Supported command

### `print` command

- `print,/par/version` - string, display currend version FW
* `print,/par/author` - string, show author info
* `print,/par/mode` - int, show curent mode:
  * `0` - auto mode,  player can play with box, default
  * `1` - manual mode, allow to control box from external host
* `print,/par/auto/state` - int, print current state of state machine in auto mode
* `print,/par/debug` - int, print current debug level
* `print,/par/manual/finger` - int, print state of finger: 0 - `hide`, 1 - `ready`, 2 - `press`
* `print,/par/manual/door` - int, print state of door: 0 - `close`, 1 - `open`
* `print,/par/manual/pos` - int, print position of finger: range from `0...100`

### `set` command

- `set,/par/mode,mode` - int, modes:
  * `0` - auto mode, allow interact user with box in auto mode
  * `1` - manual mode, allow to send control commands for ext controller
* `set,/par/manual/finger` - int, set position of finger: 0 - `hide`, 1 - `ready`, 2 - `press`
* `set,/par/manual/door` - int, set position of door: 0 - `close`, 1 - `open`
* `set,/par/manual/pos` - int, set position of finger: range from `100..2500`
* `set,/par/debug` - int, set current debug level: ...

### `em` commands

- `em,state` - activate sending message `BS004`..., contained current state of box

### `dm` commands

- `dm,state` - disable sending message `BS004`
* `dm` - disable sending all messaged from device

## Messages

### `BS004` - box state message, display current state of box

```
struct BoxState {
    uint16 state; //bit 0 - BUTTON_0 ... bit 8 - BUTTON_8; bit 9 - BUTTON_END_LEFT; bit 10 - BUTTON_END_RIGHT, bit 11-12 - state of finger, bit 13 - state of door
    uint8 pos; //position of finger
    uint8 cs; // Checksum = 0, not used now
};
```
