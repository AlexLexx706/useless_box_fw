#include <AccelStepper.h>
#include <Servo.h>
#include <AS5600.h>

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

#define DEBOUNCE_DELAY 50 // Время задержки для проверки


static int buttons[] = {
	BUTTON_0, BUTTON_1, BUTTON_2, BUTTON_3, BUTTON_4, BUTTON_5,
	BUTTON_6,BUTTON_7, BUTTON_8, BUTTON_END_LEFT, BUTTON_END_RIGHT};

//string states[] = {"LOW", "LOW","LOW","LOW","LOW","LOW","LOW","LOW","LOW"};
//string last_states[] = {"LOW", "LOW","LOW","LOW","LOW","LOW","LOW","LOW","LOW"};
int last_states[] = {0,0,0,0,0,0,0,0,0};
int states[] = {0,0,0,0,0,0,0,0,0};
int but_positions[] = {
	-2500,
	-2200,
	-1900,
	-1600,
	-1300,
	-1000,
	-700,
	-400,
	-100,
};

Servo servo_1;
Servo servo_2;
AccelStepper stepper(AccelStepper::DRIVER, STEP_PIN, DIR_PIN);


AS5600 encoder;
long revolutions = 0;	// number of revolutions the encoder has made
double position;		// the calculated value the encoder is at
double output;			// raw value from AS5600
long lastOutput;		// last output from AS5600
int start = 0;
int pos = 0;


unsigned long lastDebounceTime = 0; // Время нажатия кнопки

void setup() {
	// 0. setup serial
	Serial.begin(115200);

	

	//1. setup stepper driver
	pinMode(ENABLE_PIN, OUTPUT);
	digitalWrite(ENABLE_PIN, LOW);

	stepper.setMaxSpeed(300 * 3);
	stepper.setAcceleration(500 * 2);
	stepper.moveTo(10000);

	//2. setup buttons
	for (int i = 0; i < sizeof(buttons)/ sizeof(buttons[0]); i++) {
		pinMode(buttons[i], INPUT_PULLUP); 
	}

	//3. init servos
	servo_1.attach(SERVO_1);
	servo_2.attach(SERVO_2);

	servo_1.write(0);

	//4. init encoder
	output = encoder.getPosition();
	lastOutput = output;
	position = output;
}

void test_encoder() {
	// get the raw value of the encoder
	static unsigned long start_time = millis();
	unsigned long cur_time = millis();

	//read decoder data 10 HZ
	if (cur_time - start_time >= 100) {
		start_time = cur_time;
		output = encoder.getPosition();

		// check if a full rotation has been made
		if ((lastOutput - output) > 2047 ) {
			revolutions++;
		}
		
		if ((lastOutput - output) < -2047 ) {
			revolutions--;
		}
		lastOutput = output; 

		// calculate the position the the encoder is at based off of the number of revolutions
		position = (revolutions * 4096 + output) / 4096. * 360;
		Serial.println(position);
	}
}

void test_buttons() {
	unsigned short button_mask = 0;
	for (int i = 0; i < sizeof(buttons)/ sizeof(buttons[0]); i++) {
		button_mask = button_mask | (digitalRead(buttons[i]) ? 1 : 0) << i;
	}
	Serial.println(button_mask, BIN);
}

void test_stepper() {
	if (stepper.distanceToGo() == 0) {
		stepper.moveTo(-stepper.currentPosition());
	}
	stepper.run();
}

void test_servo() {
	static unsigned long start_time = millis();
	static char state = 0;

	unsigned long cur_time = millis();

	if (state == 0) {
		if ((cur_time - start_time) > 700) {
			servo_1.write(90);
			start_time = cur_time;
			state = 1;
		}
	} else {
		if ((cur_time - start_time) > 300) {
			servo_1.write(0);
			start_time = cur_time;
			state = 0;
		}
	}
}

void set_pos_to_but(){
	int reading;
	for (int i = 0; i < sizeof(buttons)/ sizeof(buttons[0])-2; i++) {
		//Serial.println("step 1");

		reading = digitalRead(buttons[i]);
		if (reading != last_states[i]) {
			lastDebounceTime = millis();
			//Serial.println("step 2");
		}

		if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
			//Serial.println("step 3");
			if (reading != states[i]) {
				states[i] = reading;
				//Serial.println("step 4");
				//delay(3000);
				
			}
		}
		last_states[i] = reading;

		//Serial.println(states[i]);
		if (states[i] == 1) {
			//Serial.print("Нажата кнопочка ");
			//Serial.println(i);
			stepper.moveTo(but_positions[i]);
			if(stepper.distanceToGo() == 0){
				off_button();
				//delay(1000);
			}
			
		}

	}
}


void off_button(){
	static unsigned long start_time = millis();
	static char state = 0;

	unsigned long cur_time = millis();

	if (state == 0) {
		if ((cur_time - start_time) > 300) {
			servo_1.write(90);
			start_time = cur_time;
			state = 1;
		}
	} else {
		if ((cur_time - start_time) > 300) {
			servo_1.write(0);
			start_time = cur_time;
			state = 0;
		}
	}
}

void loop() {
	if (digitalRead(BUTTON_END_LEFT) == LOW || digitalRead(BUTTON_END_RIGHT) == LOW) {
		//Serial.println("stop");
		stepper.setMaxSpeed(0);
		stepper.setAcceleration(0);
		stepper.stop();
		stepper.setMaxSpeed(5000 * 3);
		stepper.setAcceleration(7000 * 2);
		stepper.setCurrentPosition(0);
		start = 1;
	} 
	

	if(start)
		set_pos_to_but();

	stepper.run();

	// if(stepper.distanceToGo() == 0){
	// 	off_button();
	// 	delay(1200);
	// }

	//Serial.println(stepper.targetPosition());
	//set_pos_to_but();
	//start_config();
	//test_servo();
	//test_buttons();
	//test_stepper();
	//test_encoder();
}