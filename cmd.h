#define BUFF_SIZE 1024
#define STDIN  0
#define STDOUT 1
#define STDERR 2
enum{
	EREADFILE,
	EINVALTOKEN,
	ERR_MAX
};

enum{
	S_TOK = 's',
	D_TOK = 'd',
	P_TOK = 'p',
	Q_TOK = 'q',
};
extern const char *err_msgs[ERR_MAX];
int run_script(const char script[], int fd, int flags);
const char *err_msg(int err_code);
