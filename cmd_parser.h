
#ifndef CMD_PARSER_H_
#define CMD_PARSER_H_
#define ALLOW_CMD_PARSER_DEBUG
#include <assert.h>

/**
 * @brief Simple parser of commands in style: [%prefix%]cmd[,paramters][,value]
 * 
 */
class CommandParser {
public:
    /**
     * @brief callback function type, called if during parsing error riased
     * @param prefix - current prefix, c string
     * @param msg - description of error
     */
    typedef void (*error_cb_t)(const char * prefix, const char * msg);
    /**
     * @brief callback function type, called when parsing conpleted well
     * @param prefix - current prefix, c string, can be empty string
     * @param cmd - current command, c string, can be empty string in case of empty prefix - echo command
     * @param parameter - current parameter, c string, can be empty
     * @param value - current value, c string, can be empty
     */
    typedef void (*cmd_cb_t)(const char * prefix, const char * cmd, const char * parameter, const char * value);

private:
    error_cb_t error_cb = nullptr;
    cmd_cb_t cmd_cb = nullptr;

    int state;

    char prefix[9];
    int prefix_index;

    char cmd[9];
    int cmd_index;

    char parameter[33];
    int parameter_index;

    char value[33];
    int value_index;
    /**
     * @brief reset parser state
     * 
     */
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
    CommandParser() {
        reset();
    }

    /**
     * @brief Set the callback functions, this function must called before using process_symbol
     * 
     * @param error_cb_ - error callback function, called when parsing error presents
     * @param cmd_cb_ - commands callback function, called when parsing success 
     */
    void set_callback(error_cb_t error_cb_, cmd_cb_t cmd_cb_) {
        assert(error_cb_);
        assert(cmd_cb_);
        error_cb = error_cb_;
        cmd_cb = cmd_cb_;
    }

    /**
     * @brief process symbol though parser
     * 
     * @param symbol - char, stream simbol
     */
    void process_symbol(char symbol) {
        assert(error_cb);
        assert(cmd_cb);

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
                } else {
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
                        error_cb("", "{1,wrong syntax}");
                        reset();
                    }
                //error: wrong prefix symbols
                } else {
                    error_cb("", "{1,wrong syntax}");
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
                        error_cb(prefix, "{4,wrong cmd size}");
                        reset();
                    }
                //checking of condition to move next step
                } else if (symbol == ',') {
                    //can't be empty command: wrong syntax
                    if (!cmd_index) {
                        error_cb(prefix, "{1,wrong syntax}");
                        reset();
                    //switch to parameter reading
                    } else {
                        state = 3;
                    }
                //commad or prefix complete
                } else if (symbol == '\n' || symbol == '\r') {
                    cmd_cb(prefix, cmd, nullptr, nullptr);
                    reset();
                //error or only prefix
                } else {
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
                        symbol == 0x5F ||
                        symbol == 0x2F ||
                        symbol == 0x2D) {
                    //collect parameter
                    if (parameter_index < sizeof(parameter) - 1) {
                        parameter[parameter_index++] = symbol;
                        parameter[parameter_index] = 0;
                    //error: wrong parmeter size
                    } else {
                        error_cb(prefix, "{5,wrong param size}");
                        reset();
                    }
                //complete reading parameter
                } else if (symbol == '\n' || symbol == '\r') {
                    //wrong sintax
                    if (!parameter_index) {
                        error_cb(prefix, "{1,wrong syntax}");
                        reset();
                    //switch to valiable
                    } else {
                        cmd_cb(prefix, cmd, parameter, nullptr);
                        reset();
                    }
                //checking of condition to move next step
                } else if (symbol == ',') {
                    //can't be empty parameter
                    if (!parameter_index) {
                        error_cb(prefix, "{1,wrong syntax}");
                        reset();
                    //switch to valiable
                    } else {
                        state = 4;
                    }
                //wrong syntax
                } else {
                    error_cb(prefix, "{1,wrong syntax}");
                    reset();
                }
                break;
            }
            //collect value
            case 4: {
                if (symbol >= 0x30 && symbol <= 0x39 ||
                        symbol >= 0X41 && symbol <= 0x5A ||
                        symbol >= 0X61 && symbol <= 0x7A ||
                        symbol == 0x5F ||
                        symbol == 0x2D ||
                        symbol == 0x2B) {
                    //collect value
                    if (value_index < sizeof(value) - 1) {
                        value[value_index++] = symbol;
                        value[value_index] = 0;
                    //error: wrong value size
                    } else {
                        error_cb(prefix, "{7,wrong value size}");
                        reset();
                    }
                //checking contition of exit
                } else if (symbol == '\n' || symbol == '\r') {
                    //can't be empty value
                    if (!value_index) {
                        error_cb(prefix, "{1,wrong syntax}");
                        reset();
                    //complete reading value
                    } else {
                        cmd_cb(prefix, cmd, parameter, value);
                        reset();
                    }
                //wrong syntax
                } else {
                    error_cb(prefix, "{1,wrong syntax}");
                    reset();
                }
                break;
            }
        }
    }
};

#endif //CMD_PARSER_H_