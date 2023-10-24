
#ifndef CMD_GRIL_PARSER_H_
#define CMD_GRIL_PARSER_H_
#define ALLOW_CMD_PARSER_DEBUG
#include <assert.h>

class CommandParser {
public:
    typedef void (*error_cb_t)(const char * prefix, const char * msg);
    typedef void (*cmd_cb_t)(const char * prefix, const char * cmd, const char * parameter, const char * value);

private:
    error_cb_t error_cb = nullptr;
    cmd_cb_t cmd_cb = nullptr;

    int state = 0;

    char prefix[9] = {0};
    int prefix_index = 0;

    char cmd[9];
    int cmd_index = 0;

    char parameter[33];
    int parameter_index = 0;

    char value[33];
    int value_index = 0;

    void reset() {
        state = 0;

        prefix_index = 0;
        prefix[prefix_index] = 0;
        cmd_index = 0;
        cmd[cmd_index] = 0;
        parameter_index = 0;
        parameter[parameter_index] = 0;
        value_index = 0;
        value[value_index] = 0;
    }

public:
    CommandParser() {}
    int get_state() const {return state;}
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
                    cmd[cmd_index++] = symbol;
                    cmd[cmd_index] = 0;
                    state = 2;
                //error: wrong symbols
                } else if (symbol == '\n' || symbol == '\r') {
                    //echo prefix
                    if (prefix_index) {
                        assert(cmd_cb);
                        cmd_cb(prefix, nullptr, nullptr, nullptr);
                    //wrong symbol
                    } else if (cmd_index) {
                        assert(error_cb);
                        error_cb(prefix, "{1,wrong syntax}");
                    }
                    reset();
                }
                break;
            }
            //collect prefix
            case 1: {
                //complete prefix
                if (symbol == '%') {
                    state = 2;
                //checking prefix alphabet 
                } else if (
                        symbol >= 0x30 && symbol <= 0x39 ||
                        symbol >= 0X41 && symbol <= 0x5A ||
                        symbol >= 0X61 && symbol <= 0x7A ||
                        symbol == 0x5F) {
                    //collect prefix
                    if (prefix_index < sizeof(prefix) - 1) {
                        prefix[prefix_index++] = symbol;
                        prefix[prefix_index] = 0;
                    //error: wrong prefix len
                    } else {
                        assert(error_cb);
                        error_cb(prefix, "{2,wrong prefix len}");
                        reset();
                    }
                //error: wrong prefix symbols
                } else {
                    assert(error_cb);
                    error_cb(prefix, "{3,wrong prefix symbols}");
                    reset();
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
                        cmd[cmd_index] = 0;
                    //error: commad size
                    } else {
                        assert(error_cb);
                        error_cb(prefix, "{4,wrong cmd size}");
                        reset();
                    }
                //switch to parameter reading
                } else if (symbol == ',') {
                    state = 3;
                } else if (symbol == '\n' || symbol == '\r') {
                    assert(cmd_cb);
                    cmd_cb(prefix, cmd, nullptr, nullptr);
                    reset();
                //error or only prefix
                } else {
                    assert(error_cb);
                    error_cb(prefix, "{6,wrong parameter!!!}");
                    reset();
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
                        parameter[parameter_index] = 0;
                    //error: wrong parmeter size
                    } else {
                        assert(error_cb);
                        error_cb(prefix, "{5,wrong param size}");
                        reset();
                    }
                //complete reading command
                } else if (symbol == '\n' || symbol == '\r') {
                    assert(cmd_cb);
                    cmd_cb(prefix, cmd, parameter, nullptr);
                    reset();
                //switch to valiable
                } else if (symbol == ',') {
                    state = 4;
                //wrong syntax
                } else {
                    assert(error_cb);
                    error_cb(prefix, "{1,wrong syntax}");
                    reset();
                }
                break;
            }
            //collect parmeter
            case 4: {
                if (symbol >= 0x30 && symbol <= 0x39 ||
                        symbol >= 0X41 && symbol <= 0x5A ||
                        symbol >= 0X61 && symbol <= 0x7A ||
                        symbol == 0x5F) {
                    //collect value
                    if (value_index < sizeof(value) - 1) {
                        value[value_index++] = symbol;
                        value[value_index] = 0;
                    //error: wrong value size
                    } else {
                        assert(error_cb);
                        error_cb(prefix, "{7,wrong value size}");
                        reset();
                    }
                //complete reading command
                } else if (symbol == '\n' || symbol == '\r') {
                    assert(cmd_cb);
                    cmd_cb(prefix, cmd, parameter, value);
                    reset();
                //wrong syntax
                } else {
                    assert(error_cb);
                    error_cb(prefix, "{1,wrong syntax}");
                    reset();
                }
                break;
            }
        }
    }
};

#endif