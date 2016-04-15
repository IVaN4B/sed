#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include "cmd.h"

const char *err_msgs[ERR_MAX] = {
	"Unable to read given file\n",
	"Invalid token\n"
};

const char *err_msg(int err_code){
	if( err_code < 0 || err_code > ERR_MAX ) return err_msgs[0];
	return err_msgs[err_code];
}

int run_script(const char script[], int fd, int flags){
	/* Read line
	 * Start NDA
	 * ?????
	 * PROFIT
	 */
	char buff[BUFF_SIZE];
	int bytes = read(fd, &buff, BUFF_SIZE);
	if( bytes == 0 ){
		return -1;
	}

	const char *stok = script;
	do{
		char tok = *stok;
		switch(tok){
			case S_TOK:
				write(STDOUT, stok, 1);
			break;

			case D_TOK:
				;
			break;

			case P_TOK:
				;
			break;

			case Q_TOK:
				;
			break;

			default:
				return EINVALTOKEN;
			break;

		}
		write(STDOUT, "\n", 1);
	}while( *stok++ );
	return 0;
}
