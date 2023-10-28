#include <AccelStepper.h>
#include <AsyncI2CMaster.h>
#include <Servo.h>
#include "cmd_parser.h"

//settings for decoder
#define I2C_ADDRESS 0x36
#define RAW_ANGLE_ADDRESS_MSB 0x0C
#define RESOLUTION 4096
#define HALF_RESOLUTION 2047
#define B0_ENCODER_POS -25855
#define B0_ENCODER_POS -25642
// debug messages
// #define DEBUG_PROC_STATE
// #define DEBUG_BUTTONS
// #define DEBUG_ENCODER
// #define DEBUG_ENCODER_STEPER
// #define DEBUG_I2C_STATUS
// #define DEBUG_SERVO_STATE
// stepper motor settings
#define STEPS_PER_REVOLUTION 200 * 2
#define DIR_PIN 4
#define STEP_PIN 7
#define ENABLE_PIN 8
#define START_MAX_SPEED (600)
#define START_MAX_ACCELERATION (7000)
#define MAX_SPEED (5000)
#define MAX_ACCELERATION (7000)
#define DISABLE_AT_STOP
#define MAX_ENC_STEPER_OFFSET 30
//buttons defines
#define BUTTON_0 9
#define BUTTON_1 10
#define BUTTON_2 11
#define BUTTON_3 12
#define BUTTON_4 A0
#define BUTTON_5 13
#define BUTTON_6 A3
#define BUTTON_7 A2
#define BUTTON_8 A1
#define BUTTON_END_LEFT 2
#define BUTTON_END_RIGHT 3
// used for debounce procedure
#define DEBOUNCE_DELAY 40
#define START_POS -50
#define LEFT_END_BUTTON_ID 9
#define RIGHT_END_BUTTON_ID 10
// servos settings
#define SERVO_HOOK 5
#define SERVO_DOOR 6
#define SERVO_DOOR_CLOSE_POS 47
#define SERVO_DOOR_OPEN_POS 0
#define SERVO_HOOK_INIT_POS 0
#define SERVO_HOOK_PRESS_POS 92
#define SERVO_HOOK_RELEASE_POS 76
#define SERVO_PRESS_RELEASE_TIMEOUT 20
#define SERVO_INIT_TIMEOUT 300
#define MOVE_SERVO_INIT_TIMEOUT 1500
#define DOOR_LOCK_TIME 1000
#define SEND_BTN_STATES_PERIOD_MS 100

#define VERSION "1.0"
#define AUTHOR "alexlexx1@gmail.com"

#define MAX_COMMAND 1
#define MAX_DEBUG_LEVEL 1

static int current_mode = 0;
static int debug_level = 0;
static int last_command = 0;
static bool enable_send_btn_state = false;
static int door_state = 0;
static int finger_state = 0;
static int finger_pos = -1600;


//description of button state
struct ButtonState {
	char state;
	int pos;
	unsigned long s_time;
};

//used for message BS
struct BoxState {
    short state; //bit 0 - BUTTON_0 ... bit 8 - BUTTON_8; bit 9 - BUTTON_END_LEFT; bit 10 - BUTTON_END_RIGHT, bit 11-12 - state of finger, bit 13 - state of door
    short pos; //position of finger
    byte cs; // Checksum = 0, not used now
};

//used for mapping buttons by index
static int buttons_map[] = {
	BUTTON_0, BUTTON_1, BUTTON_2, BUTTON_3, BUTTON_4, BUTTON_5,
	BUTTON_6, BUTTON_7, BUTTON_8, BUTTON_END_LEFT, BUTTON_END_RIGHT};
//contain button states
static ButtonState buttons_states[] = {
	{1, -2500, 0},
	{1, -2200, 0},
	{1, -1900, 0},
	{1, -1600, 0},
	{1, -1300, 0},
	{1, -1000, 0},
	{1, -700, 0},
	{1, -400, 0},
	{1, -100, 0},
	{1, 0, 0},
	{1, 0, 0}
};
static Servo servo_1;
static Servo servo_2;
static AccelStepper stepper(AccelStepper::DRIVER, STEP_PIN, DIR_PIN);
static AsyncI2CMaster i2c_master;

struct EncoderSate {
	unsigned long s_time;
	long revolutions;		// number of revolutions the encoder has made
	long position;			// contain current position of encoder
	int lastOutput;			// last output from AS5600
	long offset;			// used for set to zerro encoder pos
};
static EncoderSate encoder_state = {millis(), 0, 0, 0, 0};

struct ProcessState {
	int state;
	int nearest_button;
	unsigned long start_time;
	unsigned long wait_time;
};

CommandParser command_parser;

//state of process
ProcessState proc = {0, -1, 0, SERVO_INIT_TIMEOUT};

//write error to port
void print_er(const char * prefix, const char * msg) {
	assert(prefix);
	assert(msg);

	char buffer[60];
	int prefix_len = strlen(prefix);
	int len;
	if (prefix_len) {
		len = snprintf(buffer, sizeof(buffer), "ER%03X%%%s%%%s\r\n", strlen(msg) + prefix_len + 2, prefix, msg);
	} else {
		len = snprintf(buffer, sizeof(buffer), "ER%03X%s\r\n", strlen(msg), msg);
	}
	Serial.write(buffer, len);
}

//write responce msg 
void print_re(const char * prefix, const char * msg) {
	assert(prefix);
	assert(msg);

	char buffer[60];
	int prefix_len = strlen(prefix);
	int msg_len = strlen(msg);
	int len;

	if (prefix_len) {
		len = snprintf(buffer, sizeof(buffer), "RE%03X%%%s%%%s\r\n", strlen(prefix) + msg_len + 2, prefix, msg);
		Serial.write(buffer, len);
	} else if (msg_len) {
		len = snprintf(buffer, sizeof(buffer), "RE%03X%s\r\n",  msg_len, msg);
		Serial.write(buffer, len);
	}
}

void command_parser_cmd_cb(const char * prefix, const char * cmd, const char * parameter, const char * value) {
	//echo command
	if (!strlen(prefix) && !strlen(cmd)) {
		print_re(prefix, "%%");
		return;
	}

	//cheking print command
	if (strcmp(cmd, "print") == 0) {
		//print current version
		if (strcmp(parameter, "/par/version") == 0) {
			print_re(prefix, VERSION);
		//print author
		} else if (strcmp(parameter, "/par/author") == 0)  {
			print_re(prefix, AUTHOR);
		//print current mode: auto or manual
		} else if (strcmp(parameter, "/par/mode") == 0)  {
			print_re(prefix, String(current_mode).c_str());
		//print current state of state machine in auto mode
		} else if (strcmp(parameter, "/par/auto/state") == 0)  {
			print_re(prefix, String(proc.state).c_str());
		//print current debug level
		} else if (strcmp(parameter, "/par/debug") == 0)  {
			print_re(prefix, String(debug_level).c_str());
		//print last command
		} else if (strcmp(parameter, "/par/manual/finger") == 0)  {
			print_re(prefix, String(finger_state).c_str());
		} else if (strcmp(parameter, "/par/manual/door") == 0)  {
			print_re(prefix, String(door_state).c_str());
		} else if (strcmp(parameter, "/par/manual/pos") == 0)  {
			print_re(prefix, String(stepper.currentPosition()).c_str());
		} else {
			print_er(prefix, "{6,wrong parameter}");
		}
	//process set commands
	} else if (strcmp(cmd, "set") == 0) {
		//wrong value
		if (value == nullptr) {
			print_er(prefix, "{7,wrong value}");
			return;
		}
		//set mode
		if (strcmp(parameter, "/par/mode") == 0) {
			int _mode = String(value).toInt();
			if (_mode == 0) {
				current_mode = _mode;
				print_re(prefix, "");
			} else if (_mode == 1) {
				current_mode = 1;
				print_re(prefix, "");
			} else {
				print_er(prefix, "{7,wrong value}");
			}
		//set current finger state for manual mode
		} else if (strcmp(parameter, "/par/manual/finger") == 0) {
			int cur_finger_state = String(value).toInt();
			if (cur_finger_state >= 0 && cur_finger_state < 3) {
				finger_state = cur_finger_state;
				print_re(prefix, "");
			} else {
				print_er(prefix, "{7,wrong value}");
			}
		//set current door state for manual mode
		} else if (strcmp(parameter, "/par/manual/door") == 0) {
			int cur_door_state = String(value).toInt();
			if (cur_door_state >= 0 && cur_door_state < 2) {
				door_state = cur_door_state;
				print_re(prefix, "");
			} else {
				print_er(prefix, "{7,wrong value}");
			}
		//set current pos for manual mode
		} else if (strcmp(parameter, "/par/manual/pos") == 0) {
			int cur_finger_pos = String(value).toInt();
			if (cur_finger_pos >= 100 && cur_finger_pos <= 2500) {
				finger_pos = -cur_finger_pos;
				print_re(prefix, "");
			} else {
				print_er(prefix, "{7,wrong value}");
			}
		//set debug level
		} else if (strcmp(parameter, "/par/debug") == 0) {
			int cur_debug = String(value).toInt();
			if (cur_debug >= 0 && cur_debug < MAX_DEBUG_LEVEL) {
				print_re(prefix, "");
			} else {
				print_er(prefix, "{7,wrong value}");
			}
		} else {
			print_er(prefix, "{6,wrong parameter}");
		}
	//enable messages
	} else if (strcmp(cmd, "em") == 0) {
		//enable buttons messages
		if (strcmp(parameter, "state") == 0) {
			enable_send_btn_state = true;
			print_re(prefix, "");
		} else {
			print_er(prefix, "{6,wrong parameter}");
		}
	//disable messages
	} else if (strcmp(cmd, "dm") == 0) {
		//enable buttons messages
		if (strcmp(parameter, "state") == 0) {
			enable_send_btn_state = false;
			print_re(prefix, "");
		//disable all messages
		} else {
			enable_send_btn_state = false;
			debug_level = 0;
			print_re(prefix, "");
		}
	//wrong command
	} else {
		print_er(prefix, "{8,wrong command}");
	}
}

void command_parser_parce_stream() {
	// 1. read data
	while (1) {
		if (Serial.available()) {
			int symbol = Serial.read();
			command_parser.process_symbol(symbol);
		} else {
			return;
		}
	}
}

void dump_error_status(uint8_t status) {
	switch(status) {
		case I2C_STATUS_DEVICE_NOT_PRESENT:
			Serial.println(F("Device not present!"));
			break;
		case I2C_STATUS_TRANSMIT_ERROR:
			Serial.println(F("Bus error"));
			break;
		case I2C_STATUS_NEGATIVE_ACKNOWLEDGE:
			Serial.println(F("Negative acknowledge"));
			break;
		case I2C_STATUS_OUT_OF_MEMORY:
			Serial.println(F("memory not enought"));
			break;
	}
}

void send_btn_state() {
	char buffer[3];
	BoxState packet;

	packet.state =
		buttons_states[0].state << 0 |
		buttons_states[1].state << 1 |
		buttons_states[2].state << 2 |
		buttons_states[3].state << 3 |
		buttons_states[4].state << 4 |
		buttons_states[5].state << 5 |
		buttons_states[6].state << 6 |
		buttons_states[7].state << 7 |
		buttons_states[8].state << 8 |
		buttons_states[9].state << 9 |
		buttons_states[10].state << 10 |
		finger_state << 11 |
		door_state << 13;
	packet.pos = -stepper.currentPosition();
	packet.cs = 0;

	//send packet to port
	Serial.print("BS004");
	Serial.write(reinterpret_cast<byte *>(&packet), sizeof(packet));
	Serial.println();
}


void setup() {
	// 0. setup serial
	Serial.begin(115200);
	command_parser.set_callback(print_er, command_parser_cmd_cb);
	//1. setup stepper driver
	pinMode(ENABLE_PIN, OUTPUT);
	digitalWrite(ENABLE_PIN, LOW);
	stepper.setMaxSpeed(START_MAX_SPEED);
	stepper.setAcceleration(START_MAX_ACCELERATION);
	stepper.moveTo(10000);
	//2. setup buttons and init states
	for (size_t i = 0; i < sizeof(buttons_states)/ sizeof(buttons_states[0]); i++) {
		pinMode(buttons_map[i], INPUT_PULLUP);
		buttons_states[i].state = digitalRead(buttons_map[i]);
	}
	//3. init servos
	servo_1.attach(SERVO_HOOK);
	servo_2.attach(SERVO_DOOR);
	servo_1.write(SERVO_HOOK_INIT_POS);
	servo_2.write(SERVO_DOOR_CLOSE_POS);
	//wait for servo stop moving and detach servos
	delay(MOVE_SERVO_INIT_TIMEOUT);
	servo_1.detach();
	servo_2.detach();
	#ifdef DEBUG_SERVO_STATE
		Serial.println(F("state init: SERVO_HOOK detached"));
		Serial.println(F("state init: SERVO_DOOR detached"));
	#endif
	//4. init encoder
	i2c_master.begin();
	uint8_t arg = RAW_ANGLE_ADDRESS_MSB;
	uint8_t status;
	if (status = i2c_master.request(I2C_ADDRESS, &arg, sizeof(arg), 2, request_callback, NULL) != I2C_STATUS_OK) {
		#ifdef DEBUG_I2C_STATUS
		  dump_error_status(status);
	  #endif
	}
	#ifdef DEBUG_PROC_STATE
		Serial.println(F("0. move to begin"));
	#endif
}
void request_callback(uint8_t status, void *arg, uint8_t * data, uint8_t datalen) {
	if( status == I2C_STATUS_OK ) {
		//1. decode pos data:
		int output = (data[0] << 8) | (data[1]);
		// check if a full rotation has been made
		if ((encoder_state.lastOutput - output) > HALF_RESOLUTION ) {
			encoder_state.revolutions++;
		}
		if ((encoder_state.lastOutput - output) < -HALF_RESOLUTION ) {
			encoder_state.revolutions--;
		}
		encoder_state.lastOutput = output;
		// calculate the position the the encoder is at based off of the number of revolutions
		encoder_state.position = (encoder_state.revolutions * RESOLUTION + output);
		// Serial.println(encoder_state.position);
		#ifdef DEBUG_ENCODER
			unsigned long c_time = millis();
			if (c_time - encoder_state.s_time >= 100) {
				encoder_state.s_time = c_time;
				Serial.print(F("e:")); Serial.println(encoder_state.position);
			}
		#endif
		//1. request new position:
		uint8_t arg = RAW_ANGLE_ADDRESS_MSB;
		if (i2c_master.request(I2C_ADDRESS, &arg, sizeof(arg), 2, request_callback, NULL) != I2C_STATUS_OK) {
			#ifdef DEBUG_I2C_STATUS
			  dump_error_status(status);
      #endif
		}
	} else {
    #ifdef DEBUG_I2C_STATUS
		  dump_error_status(status);
    #endif
	}
}
long get_encoder_pos() {
	return encoder_state.position - encoder_state.offset;
}
long get_encoder_to_stepper_pos() {
	return get_encoder_pos() / float(B0_ENCODER_POS) * buttons_states[0].pos;
}
void reset_encoder_pos() {
	encoder_state.offset = encoder_state.position;
}
void update_buttons(unsigned long cur_time){
	int cur_state;
	#ifdef DEBUG_BUTTONS
		unsigned short button_mask = 0;
	#endif
	for (size_t i = 0; i < sizeof(buttons_states)/ sizeof(buttons_states[0]); i++) {
		cur_state = digitalRead(buttons_map[i]);
		//1. check for state changed
		if (cur_state == buttons_states[i].state) {
			buttons_states[i].s_time = cur_time;
		//2. wait timeout and update state
		} else if ((cur_time - buttons_states[i].s_time) > DEBOUNCE_DELAY) {
			buttons_states[i].state = cur_state;
		}
		#ifdef DEBUG_BUTTONS
			button_mask |= (buttons_states[i].state ? 1 : 0) << i;
		#endif
	}
	#ifdef DEBUG_BUTTONS
		Serial.print(F("buttons:"));
		Serial.println(button_mask, BIN);
	#endif
}
int find_nearest_button() {
	int min_pos = 10000;
	int nearest_button = -1;
	int tmp;
	for (size_t i = 0; i < sizeof(buttons_states) / sizeof(buttons_states[0]) - 2; i++) {
		if (buttons_states[i].state == LOW) {
			tmp =  abs(stepper.currentPosition() - buttons_states[i].pos);
			if (tmp < min_pos) {
				nearest_button = i;
				min_pos = tmp;
			}
		}
	}
	return nearest_button;
}

void process_auto_mode(unsigned long cur_time) {
	switch (proc.state) {
		//0. move to start pos
		case 0: {
			// 1.2 reset position
			if (buttons_states[RIGHT_END_BUTTON_ID].state == LOW) {
				stepper.setCurrentPosition(0);
				stepper.stop();
				stepper.setMaxSpeed(MAX_SPEED);
				stepper.setAcceleration(MAX_ACCELERATION);
				stepper.moveTo(START_POS);
				//reset encoder pos to zerro
				reset_encoder_pos();
				proc.state = 1;
				#ifdef DEBUG_PROC_STATE
					Serial.println(F("0. reset completed, start move a bit from start"));
				#endif
			}
			break;
		}
		//1. wait for complete of moveing a little bit from start
		case 1: {
			//2.2 check for complete
			if(stepper.distanceToGo() == 0) {
				proc.state = 2;
				proc.start_time = cur_time;
				stepper.stop();
				#ifdef DISABLE_AT_STOP
					digitalWrite(ENABLE_PIN, HIGH);
				#endif
				#ifdef DEBUG_PROC_STATE
					Serial.println(F("1. move complete, start find nearest"));
				#endif
			}
			break;
		}
		//2. find nearest button and wait for move servo to init state
		case 2: {
			proc.nearest_button  = find_nearest_button();
			// 2.1 move to the nearest point
			if (proc.nearest_button != -1) {
				proc.state = 3;
				stepper.moveTo(buttons_states[proc.nearest_button].pos);
				#ifdef DISABLE_AT_STOP
					digitalWrite(ENABLE_PIN, LOW);
				#endif
				#ifdef DEBUG_PROC_STATE
					Serial.print(F("2. start move to nearest:"));
					Serial.println(proc.nearest_button);
				#endif
			// 2.2 move servo to init pos
			} else if (cur_time - proc.start_time > MOVE_SERVO_INIT_TIMEOUT) {
				if (!servo_1.attached()) {
					servo_1.attach(SERVO_HOOK);
					#ifdef DEBUG_SERVO_STATE
						Serial.println(F("state 2: SERVO_HOOK attached"));
					#endif
				}
				servo_1.write(SERVO_HOOK_INIT_POS);
				proc.start_time = cur_time;
				proc.wait_time = SERVO_INIT_TIMEOUT;
				proc.state = 7;
			}
			break;
		//3. move to button
		} case 3: {
			int cur_nearest_button = find_nearest_button();
			// 3.1 user unpress button
			if (cur_nearest_button == -1) {
				#ifdef DEBUG_PROC_STATE
					Serial.println(F("3. stop stepper, button released by user, start find neareset 2"));
				#endif
				stepper.stop();
				#ifdef DISABLE_AT_STOP
					digitalWrite(ENABLE_PIN, HIGH);
				#endif
				proc.state = 2;
				proc.start_time = cur_time;
			//3.2 switch to nearest button
			} else if (cur_nearest_button != proc.nearest_button) {
				proc.nearest_button = cur_nearest_button;
				stepper.moveTo(buttons_states[proc.nearest_button].pos);
				#ifdef DISABLE_AT_STOP
					digitalWrite(ENABLE_PIN, LOW);
				#endif
				#ifdef DEBUG_PROC_STATE
					Serial.print(F("3. nearest button changed, move to new button:"));
					Serial.println(proc.nearest_button);
				#endif
			//3.3 check for complete
			} else if (stepper.distanceToGo() == 0) {
				stepper.stop();
				#ifdef DISABLE_AT_STOP
					digitalWrite(ENABLE_PIN, HIGH);
				#endif
				proc.state = 4;
				proc.start_time = cur_time;
				if (!servo_1.attached()) {
					servo_1.attach(SERVO_HOOK);
					#ifdef DEBUG_SERVO_STATE
						Serial.println(F("state 3: SERVO_HOOK attached"));
					#endif
				}
				if (!servo_2.attached()) {
					servo_2.attach(SERVO_DOOR);
					#ifdef DEBUG_SERVO_STATE
						Serial.println(F("state 3: SERVO_DOOR attached"));
					#endif
				}
				servo_1.write(SERVO_HOOK_PRESS_POS);
				servo_2.write(SERVO_DOOR_OPEN_POS);
				#ifdef DEBUG_PROC_STATE
					Serial.println(F("3. complete move, start wait servo 1 press button"));
				#endif
			}
			break;
		//4. wait for press button complete
		} case 4: {
			if (buttons_states[proc.nearest_button].state == HIGH) {
			// if ((cur_time - proc.start_time) > proc.wait_time) {
				proc.wait_time = SERVO_PRESS_RELEASE_TIMEOUT;
				servo_1.write(SERVO_HOOK_RELEASE_POS);
				proc.start_time = cur_time;
				proc.state = 5;
				#ifdef DEBUG_PROC_STATE
					Serial.println(F("4. button pressed, start wait servo 1 release"));
				#endif
			}
			break;
		//5. wait fot servo complete move to releace state
		} case 5: {
			if ((cur_time - proc.start_time) > proc.wait_time) {
				proc.state = 2;
				proc.start_time = cur_time;
				#ifdef DEBUG_PROC_STATE
					Serial.println(F("5. servo released, start find neareset 2"));
				#endif
			}
			break;
		//7. wait for servo compete move to init pos
		} case 7: {
			if ((cur_time - proc.start_time) > proc.wait_time) {
				servo_1.detach();
				#ifdef DEBUG_SERVO_STATE
					Serial.println(F("state 7: SERVO_HOOK detached"));
				#endif
				proc.start_time = cur_time;
				proc.wait_time = SERVO_INIT_TIMEOUT;
				proc.state = 8;
				servo_2.write(SERVO_DOOR_CLOSE_POS);
				#ifdef DEBUG_PROC_STATE
					Serial.println(F("7. servo in init state, start find neareset 8"));
				#endif
			}
			break;
		//8. wait for press button
		} case 8: {
			//turn off servo
			if (servo_2.attached() && (cur_time - proc.start_time) > proc.wait_time) {
				servo_2.detach();
				#ifdef DEBUG_SERVO_STATE
					Serial.println(F("state 8: SERVO_DOOR detached"));
				#endif
			}
			proc.nearest_button  = find_nearest_button();
			//move to the nearest point
			if (proc.nearest_button != -1) {
				proc.state = 3;
				stepper.moveTo(buttons_states[proc.nearest_button].pos);
				#ifdef DISABLE_AT_STOP
					digitalWrite(ENABLE_PIN, LOW);
				#endif
				#ifdef DEBUG_PROC_STATE
					Serial.print(F("8. start move to nearest:"));
					Serial.println(proc.nearest_button);
				#endif
			//move servo to init pos
			}
			break;
		}
	}
}

void process_manual_mode(unsigned long cur_time) {
	if (!servo_1.attached()) {
		servo_1.attach(SERVO_HOOK);
		#ifdef DEBUG_SERVO_STATE
			Serial.println(F("state 3: SERVO_HOOK attached"));
		#endif
	}
	if (!servo_2.attached()) {
		servo_2.attach(SERVO_DOOR);
		#ifdef DEBUG_SERVO_STATE
			Serial.println(F("state 3: SERVO_DOOR attached"));
		#endif
	}

	switch (finger_state) {
		// init state
		case 0:
			servo_1.write(SERVO_HOOK_INIT_POS);
			break;
		// ready state
		case 1:
			servo_1.write(SERVO_HOOK_RELEASE_POS);
			break;
		// press state
		case 2:
			servo_1.write(SERVO_HOOK_PRESS_POS);
			break;
	}

	switch (door_state) {
		// closs state
		case 0:
			servo_2.write(SERVO_DOOR_CLOSE_POS);
			break;
		// ready state
		case 1:
			servo_2.write(SERVO_DOOR_OPEN_POS);
			break;
	}

	//move finger to finger_pos
	if (stepper.currentPosition() != finger_pos) {
		stepper.moveTo(finger_pos);
		#ifdef DISABLE_AT_STOP
			digitalWrite(ENABLE_PIN, LOW);
		#endif
	} else {
		stepper.stop();
		#ifdef DISABLE_AT_STOP
			digitalWrite(ENABLE_PIN, HIGH);
		#endif
	}
}

unsigned long last_send_btn_state_time = millis();
void loop() {
	unsigned long cur_time = millis();
	command_parser_parce_stream();
	//update buttons
	update_buttons(cur_time);

	//default, auto mode
	if (current_mode == 0) {
		process_auto_mode(cur_time);
	} else {
		process_manual_mode(cur_time);
	}

	long enc_pos = get_encoder_to_stepper_pos();
	long stepper_cur_pos = stepper.currentPosition();
	long offset = abs(enc_pos - stepper_cur_pos);
	//reset stepper pos
	if (proc.state > 0 && offset > MAX_ENC_STEPER_OFFSET) {
		#ifdef DEBUG_PROC_STATE
			Serial.print(F("6. stepper motor position correction, offset:"));
			Serial.println(offset);
		#endif
		stepper.setCurrentPosition(enc_pos);
	}
	#ifdef DEBUG_ENCODER_STEPER
		static int counter = 0;
		if (counter % 1000 == 0) {
			Serial.print("row enc pos:");
			Serial.print(get_encoder_pos());
			Serial.print(F(" enc_pos:"));
			Serial.print(enc_pos);
			Serial.print(F(" steper_pos:"));
			Serial.print(stepper_cur_pos);
			Serial.print(F(" offset:"));
			Serial.println(offset);
		}
		counter++;
	#endif
	stepper.run();
	i2c_master.loop();

	if (enable_send_btn_state && cur_time - last_send_btn_state_time >= SEND_BTN_STATES_PERIOD_MS) {
		last_send_btn_state_time = cur_time;
		send_btn_state();
	}
}
