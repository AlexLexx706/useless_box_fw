#include <AccelStepper.h>
#include <Servo.h>
#include <AS5600.h>

//enable debug messages
#define DEBUG_PROC_STATE
// #define DEBUG_BUTTONS

#define STEPS_PER_REVOLUTION 200 * 2
#define DIR_PIN 4
#define STEP_PIN 7
#define ENABLE_PIN 8

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

#define SERVO_1 5
#define SERVO_2 6

#define DEBOUNCE_DELAY 70 // Время задержки для проверки
#define START_POS -50

#define LEFT_END_BUTTON_ID 9
#define RIGHT_END_BUTTON_ID 10

#define SERVO_1_INIT_POS 0
#define SERVO_1_PRESS_POS 80
#define SERVO_1_RELEASE_POS 65


#define MAX_SPEED (5000)
#define MAX_ACCELERATION (7000)

// #define DISABLE_AT_STOP

//description of button state
struct ButtonState {
	char state;
	int pos;
	char trigger;
	unsigned long s_time;
};

//used for mapping buttons by index
static int buttons_map[] = {
	BUTTON_0, BUTTON_1, BUTTON_2, BUTTON_3, BUTTON_4, BUTTON_5,
	BUTTON_6,BUTTON_7, BUTTON_8, BUTTON_END_LEFT, BUTTON_END_RIGHT};

//contain button states
static ButtonState buttons_states[] = {
	{0, -2500, 0, 0},
	{0, -2200, 0, 0},
	{0, -1900, 0, 0},
	{0, -1600, 0, 0},
	{0, -1300, 0, 0},
	{0, -1000, 0, 0},
	{0, -700, 0, 0},
	{0, -400, 0, 0},
	{0, -100, 0, 0},
	{0, 0, 0, 0},
	{0, 0, 0, 0}
};

static Servo servo_1;
static Servo servo_2;
static AccelStepper stepper(AccelStepper::DRIVER, STEP_PIN, DIR_PIN);
static AS5600 encoder;

struct EncoderSate {
	unsigned long start_time;
	long revolutions;		// number of revolutions the encoder has made
	double position;		// the calculated value the encoder is at
	int lastOutput;			// last output from AS5600
};

static EncoderSate encoder_state = {millis(), 0, 0, 0};

struct ProcessState {
	int state;
	int nearest_button;
	unsigned long start_time;
	unsigned long wait_time;
};

//state of process
ProcessState proc = {0, -1, 0, 300};


void setup() {
	// 0. setup serial
	Serial.begin(115200);

	//1. setup stepper driver
	pinMode(ENABLE_PIN, OUTPUT);
	digitalWrite(ENABLE_PIN, LOW);

	stepper.setMaxSpeed(200 * 3);
	stepper.setAcceleration(500 * 2);
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
	uint16_t output = encoder.getPosition();
	encoder_state.lastOutput = output;
	encoder_state.position = output / 4096. * 360;

	#ifdef DEBUG_PROC_STATE
		Serial.println("0. move to begin");
	#endif
}

void test_encoder() {
	// get the raw value of the encoder
	unsigned long cur_time = millis();

	//read decoder data 10 HZ
	if (cur_time - encoder_state.start_time >= 100) {
		encoder_state.start_time = cur_time;
		int output = encoder.getPosition();

		// check if a full rotation has been made
		if ((encoder_state.lastOutput - output) > 2047 ) {
			encoder_state.revolutions++;
		}
		
		if ((encoder_state.lastOutput - output) < -2047 ) {
			encoder_state.revolutions--;
		}
		encoder_state.lastOutput = output;

		// calculate the position the the encoder is at based off of the number of revolutions
		encoder_state.position = (encoder_state.revolutions * 4096 + output) / 4096. * 360;
		Serial.println(encoder_state.position);
	}
}


void update_buttons(unsigned long cur_time){
	int cur_state;
	#ifdef DEBUG_BUTTONS
		unsigned short button_mask = 0;
	#endif

	for (size_t i = 0; i < sizeof(buttons_states)/ sizeof(buttons_states[0]); i++) {
		cur_state = digitalRead(buttons_map[i]);

		//1. check for state changed
		if (cur_state != buttons_states[i].state && !buttons_states[i].trigger) {
			buttons_states[i].s_time = millis();
			buttons_states[i].trigger = 1;
		//2. wait timeout and update state
		} else if (buttons_states[i].trigger && (cur_time - buttons_states[i].s_time) > DEBOUNCE_DELAY) {
			buttons_states[i].state = cur_state;
			buttons_states[i].trigger = 0;
		}
		#ifdef DEBUG_BUTTONS
			button_mask |= (buttons_states[i].state ? 1 : 0) << i;
		#endif
	}

	#ifdef DEBUG_BUTTONS
		Serial.print("buttons:");
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
				proc.state = 1;
				#ifdef DEBUG_PROC_STATE
					Serial.println("0. reset completed, start move a bit from start");
				#endif
			}
			break;
		}
		//2. move a little bit from start
		case 1: {
			//2.2 check for complete
			if(stepper.distanceToGo() == 0) {
				proc.state = 2;
				stepper.stop();
				#ifdef DISABLE_AT_STOP
					digitalWrite(ENABLE_PIN, HIGH);
				#endif
				#ifdef DEBUG_PROC_STATE
					Serial.println("1. move complete, start find nearest");
				#endif
			}
			break;
		}
		//3. find nearest button
		case 2: {
			proc.nearest_button  = find_nearest_button();
			//move to the nearest point
			if (proc.nearest_button != -1) {
				proc.state = 3;
				stepper.moveTo(buttons_states[proc.nearest_button].pos);
				#ifdef DISABLE_AT_STOP
					digitalWrite(ENABLE_PIN, LOW);
				#endif
				#ifdef DEBUG_PROC_STATE
					Serial.print("2. start move to nearest:");
					Serial.println(proc.nearest_button);
				#endif
			}
			break;
		//4. move to button
		} case 3: {
			int cur_nearest_button = find_nearest_button();
			// 4.1 user unpress button
			if (cur_nearest_button == -1) {
				#ifdef DEBUG_PROC_STATE
					Serial.println("3. stop stepper, button released by user");
				#endif
					stepper.stop();
				#ifdef DISABLE_AT_STOP
					digitalWrite(ENABLE_PIN, HIGH);
				#endif
				proc.state = 2;
			//4.2 switch to nearest button
			} else if (cur_nearest_button != proc.nearest_button) {
				proc.nearest_button = cur_nearest_button;
				stepper.moveTo(buttons_states[proc.nearest_button].pos);
				#ifdef DISABLE_AT_STOP
					digitalWrite(ENABLE_PIN, LOW);
				#endif
				#ifdef DEBUG_PROC_STATE
					Serial.print("3. nearest button changed, move to new button:");
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
					Serial.println("3. complete move, start wait servo 1 press button");
				#endif
			}
			break;
		//5. wait for press button complete
		} case 4: {
			if ((cur_time - proc.start_time) > proc.wait_time) {
				proc.wait_time = 80;
				servo_1.write(SERVO_1_RELEASE_POS);
				proc.start_time = cur_time;
				proc.state = 5;
				#ifdef DEBUG_PROC_STATE
					Serial.println("4. button pressed, start wait servo 1 release");
				#endif
			}
			break;
		} case 5: {
			if ((cur_time - proc.start_time) > proc.wait_time) {
				proc.state = 2;
				#ifdef DEBUG_PROC_STATE
					Serial.println("5. servo released, start find neareset");
				#endif
			}
			break;
		}
	}
	stepper.run();
}