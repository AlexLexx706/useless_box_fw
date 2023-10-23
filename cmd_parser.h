
#ifndef CMD_GRIL_PARSER_H_
#define CMD_GRIL_PARSER_H_
#define ALLOW_CMD_PARSER_DEBUG
#include <assert.h>

class CommandParser {
public:
    typedef void (*error_cb_t)(const char * msg);
    typedef void (*cmd_cb_t)(const char * prefix, const char * cmd, const char * parameter);

private:
    error_cb_t error_cb = nullptr;
    cmd_cb_t cmd_cb = nullptr;

    int state = 0;

    char prefix[9];
    int prefix_index = 0;

    char cmd[9];
    int cmd_index = 0;

    char parameter[33];
    int parameter_index = 0;

public:
    CommandParser() {}

    void set_callback(error_cb_t error_cb_, cmd_cb_t cmd_cb_) {
        error_cb = error_cb_;
        cmd_cb = cmd_cb_;
    }
    void process_symbol(char symbol) {
        switch (state) {
            //start reading prefix or command
            case 0: {
                //prefix
                if (symbol == '%') {
                    state = 1;
                //commands
                } else if (symbol >= 0X61 && symbol <= 0x7A) {
                    prefix[prefix_index] = 0;
                    cmd[cmd_index++] = symbol;
                    state = 2;
                //error: wrong symbols
                } else if (symbol == '\n') {
                    //echo prefix
                    if (prefix_index) {
                        assert(cmd_cb);
                        cmd_cb(prefix, nullptr, nullptr);
                    //wrong symbol
                    } else {
                        assert(error_cb);
                        error_cb("{1,wrong syntax}");
                    }
                    prefix_index = 0;
                    cmd_index = 0;
                    parameter_index = 0;
                }
                break;
            }
            //collect prefix
            case 1: {
                //complete prefix
                if (symbol == '%') {
                    state = 2;
                    prefix[prefix_index] = 0;
                //checking prefix alphabet 
                } else if (
                        symbol >= 0x30 && symbol <= 0x39 ||
                        symbol >= 0X41 && symbol <= 0x5A ||
                        symbol >= 0X61 && symbol <= 0x7A ||
                        symbol == 0x5F) {
                    //collect prefix
                    if (prefix_index < sizeof(prefix) - 1) {
                        prefix[prefix_index++] = symbol;
                    //error: wrong prefix len
                    } else {
                        assert(error_cb);
                        error_cb("{2,wrong prefix len}");

                        state = 0;
                        prefix_index = 0;
                        cmd_index = 0;
                        parameter_index = 0;
                    }
                //error: wrong prefix symbols
                } else {
                    assert(error_cb);
                    error_cb("{3,wrong prefix symbols}");
                    state = 0;
                    prefix_index = 0;
                    cmd_index = 0;
                    parameter_index = 0;
                }
                break;
            }
            //collect command
            case 2: {
                //commands symbols
                if (symbol >= 0X61 && symbol <= 0x7A) {
                    //collect cmd
                    if (cmd_index < sizeof(cmd) - 1) {
                        cmd[cmd_index++] = symbol;
                    //error: commad size
                    } else {
                        assert(error_cb);
                        error_cb("{4,wrong cmd size}");
                        state = 0;
                        prefix_index = 0;
                        cmd_index = 0;
                        parameter_index = 0;
                    }
                //switch to parameter reading
                } else if (symbol == ',') {
                    cmd[cmd_index] = 0;
                    state = 3;
                //error: cmd incompleted
                } else {
                    assert(error_cb);
                    error_cb("{6,wrong parameter}");
                    state = 0;
                    prefix_index = 0;
                    cmd_index = 0;
                    parameter_index = 0;
                }
                break;
            }
            //collect parmeter
            case 3: {
                if (symbol >= 0x30 && symbol <= 0x39 ||
                        symbol >= 0X41 && symbol <= 0x5A ||
                        symbol >= 0X61 && symbol <= 0x7A ||
                        symbol == 0x5F || symbol == 0x2F) {
                    //collect parameter
                    if (parameter_index < sizeof(parameter) - 1) {
                        parameter[parameter_index++] = symbol;
                    //error: wrong parmeter size
                    } else {
                        assert(error_cb);
                        error_cb("{5,wrong param size}");
                        state = 0;
                        prefix_index = 0;
                        cmd_index = 0;
                        parameter_index = 0;
                    }
                //complete reading
                } else if (symbol == '\n') {
                    parameter[parameter_index] = 0;
                    assert(cmd_cb);
                    cmd_cb(prefix, cmd, parameter);
                    state = 0;
                }
                break;
            }
        }
    }
};

#endif