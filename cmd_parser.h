#ifndef CMD_PARSER_H_
#define CMD_PARSER_H_
#include <Arduino.h>


#define ALLOW_CMD_PARSER_DEBUG

class CommandParser {
	enum Commands {
		OPEN_DOOR = 1,			   // CmdWithoutParams
		CLOSE_DOOR = 2,			   // CmdWithoutParams
		PRESS_BUTTON = 3,		   // CmdMoveFinger
		MOVE_SERVO = 4,			   // CmdMoveServo
		MOVE_FINGER = 5,		   // CmdMoveFinger
		ACTIVATE_STATE_STREAM = 6, // CmdWithoutParams
		STOP_STATE_STREAM = 7,	   // CmdWithoutParams
	};

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

	static const int buffer_size = 32;
	static const byte max_len = 28;
	byte buffer[buffer_size];
	byte *first;
	byte *last;
	byte *prt;
	byte state;
	byte len;
	byte crc;

public:
	CommandParser() : first(buffer), last(buffer), prt(buffer), state(0) {}

	void process_symbol(byte symbol) {
		switch (state) {
			// 1. check header
			case 0: {
				if (symbol == 'x') {
					state = 1;
					*(prt++) = symbol;
					crc = symbol;
				}
				return;
			}
			// 2. check len
			case 1: {
				// 2.1 wrong len
				if (symbol > max_len) {
					prt = buffer;
					first = buffer + 1;
					state = 0;
					return;
				}
				// 2.2 good len
				len = symbol;
				*(prt++) = symbol;
				state = 2;
				crc = byte((crc << 2) | (crc >> 6)) ^ symbol;
				return;
			}
			// 3. collect data
			case 2: {
				if (len--) {
					crc = byte((crc << 2) | (crc >> 6)) ^ symbol;
					*(prt++) = symbol;
				}
				if (!len) {
					state = 3;
				}
				return;
			}
			// 4. CRC checking
			case 3: {
				*(prt++) = symbol;
				if (byte((crc << 2) | (crc >> 6)) == symbol) {
					process_packet();
					prt = buffer;
				}
				else {
					prt = buffer;
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
				first = prt;
				last = first;
			} else {
				return;
			}
		}
	}

	void process_packet() {
		switch (buffer[2]) {
			// CmdWithoutParams
			case OPEN_DOOR: {
                #ifdef ALLOW_CMD_PARSER_DEBUG
                    Serial.print(F("cmd open door:"));
                    Serial.println(reinterpret_cast<CmdWithoutParams *>(buffer)->cmd, DEC);
                #endif
				break;
			}
			// CmdWithoutParams
			case CLOSE_DOOR: {
                #ifdef ALLOW_CMD_PARSER_DEBUG
                    Serial.print(F("cmd close door:"));
                    Serial.println(reinterpret_cast<CmdWithoutParams *>(buffer)->cmd, DEC);
                #endif
                break;
			}
			// CmdMoveFinger
			case PRESS_BUTTON: {
                #ifdef ALLOW_CMD_PARSER_DEBUG
                    Serial.print(F("cmd press button cmd:"));
                    Serial.print(reinterpret_cast<CmdMoveFinger *>(buffer)->cmd, DEC);
                    Serial.print(F(" btn:"));
                    Serial.println(reinterpret_cast<CmdMoveFinger *>(buffer)->btn, DEC);
                #endif
				break;
			}
			// CmdMoveServo
			case MOVE_SERVO: {
                #ifdef ALLOW_CMD_PARSER_DEBUG
                    Serial.print(F("cmd move servo cmd:"));
                    Serial.print(reinterpret_cast<CmdMoveServo *>(buffer)->cmd, DEC);
                    Serial.print(F(" num:"));
                    Serial.print(reinterpret_cast<CmdMoveServo *>(buffer)->num, DEC);
                    Serial.print(F(" val:"));
                    Serial.println(reinterpret_cast<CmdMoveServo *>(buffer)->value, DEC);
                #endif
				break;
			}
			// CmdMoveFinger
			case MOVE_FINGER: {
                #ifdef ALLOW_CMD_PARSER_DEBUG
                    Serial.print(F("cmd move finger cmd:"));
                    Serial.print(reinterpret_cast<CmdMoveFinger *>(buffer)->cmd, DEC);
                    Serial.print(F(" btn:"));
                    Serial.println(reinterpret_cast<CmdMoveFinger *>(buffer)->btn, DEC);
                #endif
				break;
			}
			// CmdWithoutParams
			case ACTIVATE_STATE_STREAM: {
                #ifdef ALLOW_CMD_PARSER_DEBUG
                    Serial.print(F("cmd activate state stream:"));
                    Serial.println(reinterpret_cast<CmdWithoutParams *>(buffer)->cmd, DEC);
                #endif
				break;
			}
			// CmdWithoutParams
			case STOP_STATE_STREAM: {
                #ifdef ALLOW_CMD_PARSER_DEBUG
                    Serial.print(F("cmd stop state stream:"));
                    Serial.println(reinterpret_cast<CmdWithoutParams *>(buffer)->cmd, DEC);
                #endif
				break;
			}
            // wrong cmd 
			default: {
                #ifdef ALLOW_CMD_PARSER_DEBUG
                    Serial.print(F("wrong cmd:"));
                    Serial.println(buffer[2], DEC);
                #endif
                break;
			}
		}
	}
};

#endif