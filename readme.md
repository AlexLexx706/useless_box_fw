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
* `print,/par/mode` - string, show curent mode:
  * `auto` - default mode (player can play with box)
  * `manual` - mode allow to control box from external host
* `print,/par/auto/state` - int, print current state of state machine in auto mode
* `print,/par/debug` - int, print current debug level
* `print,/par/manual/last_command` - int, print last command

### `set` command

- `set,/par/mode,mode` - string, modes:
  * `auto` - default, allow interact user with box in auto mode
  * `manual` - allow to send control commands for ext controller
* `set,/par/manual/cmd` - int, set current command for `manual` mode: ...
* `set,/par/debug` - int, set current debug level: ...

### `em` commands

- `em,buttons` - activate sending message `BS003`..., contained current state of buttons

### `dm` commands

- `dm,buttons` - disable sending message `BS003`
* `dm` - disable sending all messaged from device

## Messages

### `BS003` - buttons state message, display current state of buttons

```
struct ButtonsState {
    uint16 state; //bit 0 - BUTTON_0 ... bit 8 - BUTTON_8; bit 9 - BUTTON_END_LEFT; bit 10 - BUTTON_END_RIGHT
    uint8 cs; // Checksum = 0, not used now
};
```
