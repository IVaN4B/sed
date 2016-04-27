#ifndef CMD_H
#define CMD_H
#define BUFF_SIZE 	   1024
#define NUM_BUFF_SIZE  20
#define STDIN  0
#define STDOUT 1
#define STDERR 2
#define RE_CHAR '/'
#define ESCAPE_CHAR '\\'
#define NEWLINE '\n'
#define SPACE ' '
#define COMMA ','
#define COMMENT '#'
#define zalloc(N) calloc(1, N)
enum serrors{
	SUCCESS,
	EREADFILE,
	EINVALTOKEN,
	EILLEGALCHAR,
	ENOREGEX,
	EWRONGMARK,
	ERR_MAX
};

enum cmd_result{
	RNONE,
	RQUIT,
	RCONT,
};

enum scmd_type{
	SUBST,
	GROUP,
	ENDGROUP,
	BRANCH,
	DELETE,
	YANK,
	PRINT,
	TEXT,
	APPEND,
	QUIT,
	SCMD_NUM,
};

enum saddr_type{
	REGEX_ADDR,
	LINE_ADDR,

};

typedef struct sspace_t{
	char *text;
	int len;
	char *buff;
	int buff_len;
	int offset;
	char *space;
	int space_len;
	int is_deleted;
} sspace_t;

typedef struct saddr_t{
	enum saddr_type type;
	unsigned long int line;
	char regex[BUFF_SIZE]; /* TODO: Replace with regex tree */
} saddr_t;

typedef struct scmd_t{
	struct scmd_t *next;
	struct saddr_t *baddr, *eaddr;
	enum scmd_type code;
	char regex[BUFF_SIZE];
	char text[BUFF_SIZE];
	int result;
} scmd_t;

static const char tokens[] = "s{}bdypiaq";

int parse_script(const char script[], scmd_t **cmd_list);
int run_script(const char script[], int fd, int flags);
int run_line(scmd_t *cmd_list, sspace_t *pspace, sspace_t *hspace,
												 const int line);
const char *err_msg(int err_code);
#endif
