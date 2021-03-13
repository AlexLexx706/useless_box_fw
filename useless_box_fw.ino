#include <AccelStepper.h>
#include <Servo.h>
#include <AsyncI2CMaster.h>

//settings for decoder
#define I2C_ADDRESS 0x36
#define RAW_ANGLE_ADDRESS_MSB 0x0C
#define RESOLUTION 4096
#define HALF_RESOLUTION 2047
#define B0_ENCODER_POS -25855
// debug messages
#define DEBUG_PROC_STATE
// #define DEBUG_BUTTONS
// #define DEBUG_ENCODER

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
#define SERVO_1 5
#define SERVO_2 6
#define SERVO_1_INIT_POS 0
#define SERVO_1_PRESS_POS 80
#define SERVO_1_RELEASE_POS 65
#define SERVO_PRESS_RELEASE_TIMEOUT 90
#define SERVO_INIT_TIMEOUT 300
#define MOVE_SERVO_INIT_TIMEOUT 1500

//description of button state
struct ButtonState {
	char state;
	int pos;
	unsigned long s_time;
};

//used for mapping buttons by index
static int buttons_map[] = {
	BUTTON_0, BUTTON_1, BUTTON_2, BUTTON_3, BUTTON_4, BUTTON_5,
	BUTTON_6,BUTTON_7, BUTTON_8, BUTTON_END_LEFT, BUTTON_END_RIGHT};

//contain button states
static ButtonState buttons_states[] = {
	{0, -2500, 0},
	{0, -2200, 0},
	{0, -1900, 0},
	{0, -1600, 0},
	{0, -1300, 0},
	{0, -1000, 0},
	{0, -700, 0},
	{0, -400, 0},
	{0, -100, 0},
	{0, 0, 0},
	{0, 0, 0}
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

//state of process
ProcessState proc = {0, -1, 0, SERVO_INIT_TIMEOUT};

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

void setup() {
	// 0. setup serial
	Serial.begin(115200);

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
	servo_1.attach(SERVO_1);
	servo_2.attach(SERVO_2);

	servo_1.write(SERVO_1_INIT_POS);

	//4. init encoder
	i2c_master.begin();
	uint8_t arg = RAW_ANGLE_ADDRESS_MSB;
	uint8_t status;
	if (status = i2c_master.request(I2C_ADDRESS, &arg, sizeof(arg), 2, request_callback, NULL) != I2C_STATUS_OK) {
		dump_error_status(status);
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
			dump_error_status(status);
		}
	} else {
		dump_error_status(status);
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
		if (buttons_states[i].state == HIGH) {
			tmp =  abs(stepper.currentPosition() - buttons_states[i].pos);
			if (tmp < min_pos) {
				nearest_button = i;
				min_pos = tmp;
			}
		}
	}
	return nearest_button;
}

void loop() {
	unsigned long cur_time = millis();

	//update buttons
	update_buttons(cur_time);

	switch (proc.state) {
		//1. move to start pos
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
		//2. wait for complete of moveing a little bit from start
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
		//3. find nearest button and wait for move servo to init state
		case 2: {
			proc.nearest_button  = find_nearest_button();
			// 3.1 move to the nearest point
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
			// 3.2 move servo to init pos
			} else if (cur_time - proc.start_time > MOVE_SERVO_INIT_TIMEOUT) {
				servo_1.write(SERVO_1_INIT_POS);
				proc.start_time = cur_time;
				proc.wait_time = SERVO_INIT_TIMEOUT;
				proc.state = 6;
			}
			break;
		//4. move to button
		} case 3: {
			int cur_nearest_button = find_nearest_button();
			// 4.1 user unpress button
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
			//4.2 switch to nearest button
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
			//4.3 check for complete
			} else if (stepper.distanceToGo() == 0) {
				stepper.stop();
				#ifdef DISABLE_AT_STOP
					digitalWrite(ENABLE_PIN, HIGH);
				#endif
				proc.state = 4;
				proc.start_time = cur_time;
				servo_1.write(SERVO_1_PRESS_POS);

				#ifdef DEBUG_PROC_STATE
					Serial.println(F("3. complete move, start wait servo 1 press button"));
				#endif
			}
			break;
		//5. wait for press button complete
		} case 4: {
			if ((cur_time - proc.start_time) > proc.wait_time) {
				proc.wait_time = SERVO_PRESS_RELEASE_TIMEOUT;
				servo_1.write(SERVO_1_RELEASE_POS);
				proc.start_time = cur_time;
				proc.state = 5;
				#ifdef DEBUG_PROC_STATE
					Serial.println(F("4. button pressed, start wait servo 1 release"));
				#endif
			}
			break;
		//6. wait fot servo complete move to releace state
		} case 5: {
			if ((cur_time - proc.start_time) > proc.wait_time) {
				proc.state = 2;
				proc.start_time = cur_time;
				#ifdef DEBUG_PROC_STATE
					Serial.println(F("5. servo released, start find neareset 2"));
				#endif
			}
			break;
		// wait for servo compete move to init pos
		} case 6: {
			if ((cur_time - proc.start_time) > proc.wait_time) {
				proc.start_time = cur_time;
				proc.state = 7;
				#ifdef DEBUG_PROC_STATE
					Serial.println(F("6. servo in init state, start find neareset 7"));
				#endif
			}
			break;
		// wait for press button
		} case 7: {
			proc.nearest_button  = find_nearest_button();
			//move to the nearest point
			if (proc.nearest_button != -1) {
				proc.state = 3;
				stepper.moveTo(buttons_states[proc.nearest_button].pos);
				#ifdef DISABLE_AT_STOP
					digitalWrite(ENABLE_PIN, LOW);
				#endif
				#ifdef DEBUG_PROC_STATE
					Serial.print(F("7. start move to nearest:"));
					Serial.println(proc.nearest_button);
				#endif
			//move servo to init pos
			}
			break;
		}
	}

	long enc_pos = get_encoder_to_stepper_pos();
	long offset = abs(enc_pos - stepper.currentPosition());
	//reset stepper pos
	if (proc.state > 0 && offset > MAX_ENC_STEPER_OFFSET) {
		#ifdef DEBUG_PROC_STATE
			Serial.print(F("6. stepper motor position correction, offset:"));
			Serial.println(offset);
		#endif
		stepper.setCurrentPosition(enc_pos);
	}

	stepper.run();
	i2c_master.loop();

}