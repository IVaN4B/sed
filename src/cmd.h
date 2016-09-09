#ifndef CMD_H
#define CMD_H
#define BUFF_SIZE 	   1024
#define NUM_BUFF_SIZE  20
#define STACK_SIZE 32
#define STDIN  0
#define STDOUT 1
#define STDERR 2
#define RE_CHAR '/'
#define ESCAPE_CHAR '\\'
#define NEWLINE '\n'
#define SEMICOLON ';'
#define SPACE ' '
#define COMMA ','
#define COMMENT '#'
#define NFLAG 	1
#define EFLAG   (NFLAG   << 1)
#define SFLAG_G (EFLAG   << 1)
#define SFLAG_I (SFLAG_G << 1)
#define SFLAG_P (SFLAG_I << 1)

#define IS_NUM(tok) (tok >= '0' && tok <= '9')

#define TRY_REALLOC(text, size) {					\
		char *__tmp = realloc(text, size); 			\
		if( __tmp == NULL ){						\
			return -errno;	 						\
		} 								 			\
		text = __tmp;								\
	}

enum serror{
	SUCCESS,
	EREADFILE,
	EPARSE,
	EINVALTOKEN,
	EILLEGALCHAR,
	ENOREGEX,
	EWRONGMARK,
	EWRONGREGEX,
	EMALLOC,
	ESOVERFLOW,
	EGROUPNOEND,
	ENOCMD,
	ENOLABEL,
	ENOPOS,
	EUNEXPECTED,
	ERR_MAX
};

enum cmd_result{
	RNONE,
	RQUIT,
	RCONT,
	RPRINT,
};

enum saddr_type{
	REGEX_ADDR,
	LINE_ADDR,

};

typedef struct sspace_t{
	char *text;
	size_t len;
	char buff[BUFF_SIZE];
	int offset;
	int nread;
	char *space;
	int space_len;
	int is_deleted;
} sspace_t;

typedef struct saddr_t{
	enum saddr_type type;
	unsigned long int line;
	regex_t *regex;
} saddr_t;

typedef struct scmd_t{
	struct scmd_t *next;
	struct scmd_t *cmd; /* Pointer to cmd in group */
	struct saddr_t *baddr, *eaddr;
	char code;
	regex_t *regex;
	char text[BUFF_SIZE];
	unsigned int flags;
	int result;
} scmd_t;

static const char tokens[] = "s{}bdypiaq:ghx!";

int parse_script(const char script[], scmd_t **cmd_list, unsigned int eflags);
int run_script(const char script[], int fd, unsigned int flags);
int run_line(scmd_t *cmd_list, sspace_t *pspace, sspace_t *hspace,
												 const int line,
												 unsigned int flags);
const char *err_msg(int err_code);
#endif
