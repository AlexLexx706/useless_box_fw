#ifndef CMD_PARSER_H_
#define CMD_PARSER_H_
#include <Arduino.h>


#define ALLOW_CMD_PARSER_DEBUG

class CommandParser {
public:
	enum {
		OPEN_DOOR = 1,			   // CmdWithoutParams
		CLOSE_DOOR = 2,			   // CmdWithoutParams
		PRESS_BUTTON = 3,		   // CmdMoveFinger
		MOVE_SERVO = 4,			   // CmdMoveServo
		MOVE_FINGER = 5,		   // CmdMoveFinger
		ACTIVATE_STATE_STREAM = 6, // CmdWithoutParams
		STOP_STATE_STREAM = 7,	   // CmdWithoutParams
	};

	typedef void (*protocol_cb_t)(char * buffer, int size);

	struct __attribute__((__packed__)) CmdWithoutParams {
		byte head;
		byte size;
		byte cmd;
		byte crc;
	};

	struct __attribute__((__packed__)) CmdMoveServo {
		byte head;
		byte size;
		byte cmd;
		byte num;
		byte value;
		byte crc;
	};

	struct __attribute__((__packed__)) CmdMoveFinger {
		byte head;
		byte size;
		byte cmd;
		byte btn;
		byte crc;
	};

private:
	static const int buffer_size = 32;
	static const byte max_len = 28;
	byte buffer[buffer_size];
	byte *first;
	byte *last;
	byte *ptr;
	byte state;
	byte len;
	byte crc;
	protocol_cb_t callback = nullptr;

public:
	CommandParser() : first(buffer), last(buffer), ptr(buffer), state(0) {}
	void set_callback(protocol_cb_t fun) {
		callback = fun;
	}
	void process_symbol(byte symbol) {
		switch (state) {
			// 1. check header
			case 0: {
				if (symbol == 'x') {
					state = 1;
					*(ptr++) = symbol;
					crc = symbol;
				}
				return;
			}
			// 2. check len
			case 1: {
				// 2.1 wrong len
				if (symbol > max_len) {
					ptr = buffer;
					first = buffer + 1;
					state = 0;
					return;
				}
				// 2.2 good len
				len = symbol;
				*(ptr++) = symbol;
				state = 2;
				crc = byte((crc << 2) | (crc >> 6)) ^ symbol;
				return;
			}
			// 3. collect data
			case 2: {
				if (len--) {
					crc = byte((crc << 2) | (crc >> 6)) ^ symbol;
					*(ptr++) = symbol;
				}
				if (!len) {
					state = 3;
				}
				return;
			}
			// 4. CRC checking
			case 3: {
				*(ptr++) = symbol;
				if (byte((crc << 2) | (crc >> 6)) == symbol) {
					//call handler
					if (callback) {
						callback(buffer, ptr - buffer);
					}
					ptr = buffer;
				}
				else {
					ptr = buffer;
					first = buffer + 1;
				}
				state = 0;
				return;
			}
		}
	}

	void parce_stream() {
		// 1. read data
		while (1) {
			if (Serial.available()) {
				*(last++) = Serial.read();
				while (first < last) {
					// 2. move pointers
					process_symbol(*(first++));
				}
				// move to ptr
				first = ptr;
				last = first;
			} else {
				return;
			}
		}
	}
};

#endif