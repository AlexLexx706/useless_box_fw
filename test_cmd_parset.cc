#include "cmd_parser.h"
#include <stdio.h>
#include <string.h>

void error_cb(const char * msg) {
    printf("\nerror:%s\n", msg);
}

void cmd_cb(const char * prefix, const char * cmd, const char * parameter, const char * value) {
    printf("\ncommand processed: prefix:%s cmd:%s parameter:%s\n", prefix, cmd, parameter);
}



int main() {
    printf("TEST paraset\n");
    CommandParser parser;
    parser.set_callback(error_cb, cmd_cb);

    const char * cmd = "%23%set,/par/version,eee\new\nset,/par/info\n";
    int cmd_len = strlen(cmd);

    for (int i = 0; i < cmd_len; i++) {
        printf("%c", cmd[i]);
        parser.process_symbol(cmd[i]);
    }

    cmd = "%23%set\ndddddddddddddddd,/par/version\new\nset,/par/info\n";
    cmd_len = strlen(cmd);

    for (int i = 0; i < cmd_len; i++) {
        printf("%c", cmd[i]);
        parser.process_symbol(cmd[i]);
    }

    cmd = "%23%s\nsa\n";
    cmd_len = strlen(cmd);

    for (int i = 0; i < cmd_len; i++) {
        printf("%c", cmd[i]);
        parser.process_symbol(cmd[i]);
    }

    const char * msg = "{asfsdf}";
	char buffer[60];
	int len = snprintf(buffer, sizeof(buffer), "ER%03X%s", strlen(msg), msg);
	printf("buffer:%s len:%d\n", buffer, len);


    msg = "{asfsdf}";
    const char * prefix = "124";
	len = snprintf(buffer, sizeof(buffer), "RE%03X%%%s%%%s\n", strlen(prefix) + strlen(msg) + 2, prefix, msg);
	printf("buffer:%s len:%d\n", buffer, len);

    return 0;
}