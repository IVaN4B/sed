#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <regex.h>
#include <assert.h>

#include "cmd.h"
#include "common.h"

static const char *err_msgs[ERR_MAX] = {
	"Success",
	"Unable to read given file",
	"Unable to compile script",
	"Invalid token",
	"Illegal character",
	"Regex must not be empty",
	"Wrong mark given for substr command",
	"Unable to compile regex: ",
	"Malloc failed",
	"Too many groups",
	"Unfinished group",
	"No commands",
	"No such label: ",
	"Expected position not found in script",
	"Unexpected end of script",
};

static const char parse_err_fmt[] =
	"Parse error, char #%d: '%c', line #%d: %s%s\n";

static const char *no_args = "{}dpqghx!";
static const char *sb_args = "sy";
static const char *tx_args = ":biat";

const char *err_msg(int err_code){
	if( err_code < 0 || err_code > ERR_MAX ) return err_msgs[0];
	return err_msgs[err_code];
}

static void parse_error(char tok, size_t pos, size_t linenum,
						enum serror err_code, const char ctx[]){
	fmtprint(STDERR, parse_err_fmt, pos, tok, linenum, err_msg(err_code), ctx);
}

static scmd_t *find_label(scmd_t *cmd_list, char *label){
	assert(cmd_list != NULL);
	assert(label != NULL);
	scmd_t *iter = cmd_list;
	scmd_t *nextp = iter->next;
	for(; iter; iter = nextp){
		nextp = iter->next;
		if( iter->code == '{' ){
			nextp = iter->cmd;
		}
		if( iter->code == ':' && strcmp(iter->text, label) == 0 ){
			return iter;
		}
	}
	return NULL;
}

static int set_labels(scmd_t *cmd_list, char *label_buff){
	assert(cmd_list != NULL);
	assert(label_buff != NULL);
	scmd_t *iter = cmd_list;
	scmd_t *nextp = iter->next;
	for(; iter; iter = nextp){
		nextp = iter->next;
		if( iter->code == '{' ){
			nextp = iter->cmd;
		}
		if( iter->code == 'b' || iter->code == 't' ){
			iter->cmd = find_label(cmd_list, iter->text);
			if( iter->cmd == NULL ){
				strncpy(label_buff, iter->text, BUFF_SIZE);
				return ENOLABEL;
			}
		}
	}
	return 0;
}

static int is_tok_in(const char *type, const char tok){
	const char *iter = type;
	for(; *iter; iter++){
		if( tok == *iter ){
			return 1;
		}
	}
	return 0;
}

static int read_buff(const char **pos, char savech, char *buff, int *linenum,
					 int end_on_null, int write_escape){
	assert(pos != NULL);
	assert(*pos != NULL);
	assert(buff != NULL);
	assert(linenum != NULL);
	char *iter = buff, prev = '\0';
	const char *tok = *pos;
	int i;
	for(i = 0; i < BUFF_SIZE; i++){
		if( !*tok ){
			*pos = --tok;
			if( end_on_null ){
				*iter = '\0';
				return SUCCESS;
			}
			return EUNEXPECTED;
		}
		if( *tok == NEWLINE ){
			(*linenum)++;
			if( prev != ESCAPE_CHAR && savech != NEWLINE ){
				*pos = tok;
				return EUNEXPECTED;
			}
		}
		if( savech != '\0' ){
			if( *tok == savech && prev != ESCAPE_CHAR ){
				break;
			}
		}
		if( *tok == ESCAPE_CHAR && !write_escape ){
			if( (IS_NUM(*(tok+1))) || *(tok+1) == ESCAPE_CHAR
				|| *(tok+1) == 'n' ){
			}else{
				tok++;
			}
		}
		prev = *tok;
		*iter++ = *tok++;
	}
	*iter = '\0';
	*pos = tok;
	return SUCCESS;
}

#define CURPOS (tok-script)+1
int parse_script(const char script[], scmd_t **cmd_list, unsigned int eflags){
	char num_buff[NUM_BUFF_SIZE] = {'\0'},
		 txt_buff[BUFF_SIZE] 	 = {'\0'},
		 reg_buff[BUFF_SIZE] 	 = {'\0'},
		 *iter = txt_buff, code = '\0';
	int is_cmd = 0, is_end = 0, linenum = 1, reg_flags = 0, cmd_flags = 0,
		stacki = 0;
	scmd_t *current = NULL, *grpstack[STACK_SIZE];
	saddr_t *baddr = NULL, *eaddr = NULL;
	regex_t *cmd_re = NULL;
	reg_flags |= REG_NEWLINE;
	if( eflags & EFLAG ){
		reg_flags |= REG_EXTENDED;
	}
	const char *tok = script;
	while( 1 ){
		if( *tok == COMMENT ){
			while( *tok && *tok != NEWLINE ){
				tok++;
			}
			linenum++;
			tok++; /* Skip newline */
			continue;
		}
		check_pos:
		if( (IS_NUM(*tok)) || *tok == RE_CHAR || is_end ){
			saddr_t *p = NULL;
			if( IS_NUM(*tok) ){
				iter = num_buff;
				while( IS_NUM(*tok) ){
					*iter++ = *tok++;
				}
				*iter = '\0';
				goto check_next;
			}
			if( *tok == RE_CHAR ){
				tok++; /* Skip RE_CHAR */
				int res = read_buff(&tok, RE_CHAR, txt_buff, &linenum, 0, 1);
				if( res != SUCCESS ){
					parse_error(*tok, CURPOS, linenum, res, "");
					return EPARSE;
				}
				tok++; /* Skip RE_CHAR */
				goto check_next;
			}
			if( is_end ){
				parse_error(*tok ,CURPOS, linenum, ENOPOS, "");
				return EPARSE;
			}

			check_next:
			p = malloc(sizeof(saddr_t));
			if( p == NULL ){
				return EMALLOC;
			}
			int has_pos = 0;
			if( num_buff[0] != '\0' ){
				int num = atoi(num_buff);
				num_buff[0] = '\0';
				p->type = LINE_ADDR;
				p->line = num;
				has_pos = 1;
			}

			if( txt_buff[0] != '\0' ){
				p->type = REGEX_ADDR;
				p->regex = malloc(sizeof(regex_t));
				if( p->regex == NULL ){
					return EMALLOC;
				}
				if( txt_buff[0] == '\0' ){
					parse_error(*tok, CURPOS, linenum, ENOREGEX, "");
					return EPARSE;
				}
				int rc = regcomp(p->regex, txt_buff, reg_flags);
				if( rc != 0 ){
					parse_error(*tok, CURPOS, linenum, EWRONGREGEX,
								txt_buff);
					free(p->regex);
					free(p);
					return EPARSE;
				}
				txt_buff[0] = '\0';
				has_pos = 1;
			}

			if( !has_pos ){
				parse_error(*tok, CURPOS, linenum, ENOPOS, "");
				return EPARSE;
			}

			if( !is_end ){
				baddr = p;
			}else{
				eaddr = p;
				is_end = 0;
			}

			if( *tok == COMMA ){
				if( is_end ) return ENOPOS;
				is_end = 1;
				tok++;
				goto check_pos;
			}
		}

		if( is_tok_in(no_args, *tok) ){
			is_cmd = 1;
			code = *tok;
			goto add_cmd;
		}

		if( is_tok_in(sb_args, *tok) ){
			is_cmd = 1;
			code = *tok++;
			if( !*tok ){
				parse_error(*(tok-1), CURPOS, linenum, EUNEXPECTED, "");
				return EPARSE;
			}
			char savech = *tok++;
			int res = read_buff(&tok, savech, reg_buff, &linenum, 0, 1);
			if( res != SUCCESS ){
				parse_error(*tok, CURPOS, linenum, res, "");
				return EPARSE;
			}
			tok++; /* Skip savech */
			/* Add replace text */
			res = read_buff(&tok, savech, txt_buff, &linenum, 0, 0);
			if( res != SUCCESS ){
				parse_error(*tok, CURPOS, linenum, res, "");
				return EPARSE;
			}
			tok++; /* Skip savech */
			/* View flags */
			while( *tok && *tok != NEWLINE && *tok != SEMICOLON ){
				int has_mark = 0;
				switch(*tok++){
					case 'g':
						has_mark = 1;
						cmd_flags |= SFLAG_G;
					break;
					case 'i':
						has_mark = 1;
						cmd_flags |= SFLAG_I;
						reg_flags |= REG_ICASE;
					break;
					case 'p':
						has_mark = 1;
						cmd_flags |= SFLAG_P;
					break;
				}
				if( !has_mark ){
					parse_error(*tok, CURPOS, linenum, EWRONGMARK, "");
					return EPARSE;
				}
			}
			/* Compile regex */
			if( reg_buff[0] == '\0' ){
				parse_error(*tok, CURPOS, linenum, ENOREGEX, "");
				return EPARSE;
			}
			cmd_re = malloc(sizeof(regex_t));
			if( cmd_re == NULL ){
				return EMALLOC;
			}
			int rc = regcomp(cmd_re, reg_buff, reg_flags);
			if( rc != 0 ){
				parse_error(*tok, CURPOS, linenum, EWRONGREGEX,
							reg_buff);
				free(cmd_re);
				return EPARSE;
			}
			reg_buff[0] = '\0';
			goto add_cmd;
		}

		if( is_tok_in(tx_args, *tok) ){
			is_cmd = 1;
			code = *tok++;
			/* Skip spaces */
			while( *tok == SPACE ) tok++;
			int res = read_buff(&tok, NEWLINE, txt_buff, &linenum, 1, 0);
			if( res != SUCCESS ){
				parse_error(*tok, CURPOS, linenum, res, "");
				return EPARSE;
			}
		}

		add_cmd:
		if( is_cmd ){
			scmd_t *prev = NULL;
			if( *cmd_list == NULL ){
				current = malloc(sizeof(scmd_t));
				*cmd_list = current;
				current->next = NULL;
				prev = current;
			}else{
				prev = current;
				current->next = malloc(sizeof(scmd_t));
				current = current->next;
			}
			current->next = NULL;
			current->cmd = NULL;
			current->regex = cmd_re;
			strncpy(current->text, txt_buff, BUFF_SIZE);
			txt_buff[0] = '\0';
			current->code = code;
			current->flags = cmd_flags;
			current->baddr = baddr;
			current->eaddr = eaddr;
			current->result = 0;
			if( code != '!' ){
				baddr = NULL;
				eaddr = NULL;
			}
			if( code == '{' ){
				if( stacki >= STACK_SIZE ){
					parse_error(*tok, CURPOS, linenum, ESOVERFLOW, "");
					return EPARSE;
				}
				grpstack[stacki++] = current;
			}
			if( code == '}' ){
				if( stacki == 0 ){
					parse_error(*tok, CURPOS, linenum, EILLEGALCHAR, "");
					return EPARSE;
				}
				stacki--;
				grpstack[stacki]->cmd = grpstack[stacki]->next;
				grpstack[stacki]->next = prev->next;
			}
		}

		switch(*tok){
			case NEWLINE:
				linenum++;
				goto next;
			case SPACE: case SEMICOLON: case '\0': goto next;
		}

		if( !is_cmd && *tok != NEWLINE ){
			parse_error(*tok, CURPOS, linenum, EINVALTOKEN, "");
			return EPARSE;
		}
		next:
		is_cmd = 0;
		if( !*tok ) break;
		tok++;
	}
	if( stacki != 0 ){
		if( code != '}' || stacki > 1 ){
			parse_error(*(tok-1), CURPOS, linenum, EGROUPNOEND, "");
			return EPARSE;
		}
		stacki--;
		grpstack[stacki]->cmd = grpstack[stacki]->next;
		grpstack[stacki]->next = current->next;
	}
	if( *cmd_list == NULL ){
		parse_error(*(tok-1), CURPOS, linenum, ENOCMD, "");
		return EPARSE;
	}

	int res = set_labels(*cmd_list, txt_buff);
	if( res != 0 ){
		parse_error(*(tok-1), CURPOS, linenum, res, txt_buff);
		return EPARSE;
	}
#ifdef DEBUG
	fmtprint(STDOUT, "[INFO]\n");
	fmtprint(STDOUT, "script:\t'%s'\n", script);
	scmd_t *citer = *cmd_list, *nextp = citer->next;
	for(; citer; citer = nextp){
		nextp = citer->next;
		fmtprint(STDOUT, "\ncommand:\t%c\n", citer->code);
		if( citer->baddr != NULL ){
			fmtprint(STDOUT, "baddr->line:\t%d\n", citer->baddr->line);
			fmtprint(STDOUT, "baddr->regex:\t%s\n", citer->baddr->regex);
		}

		if( citer->eaddr != NULL ){
			fmtprint(STDOUT, "eaddr->line:\t%d\n", citer->eaddr->line);
			fmtprint(STDOUT, "eaddr->regex:\t%s\n", citer->eaddr->regex);
		}

		if( citer->code == SUBST || citer->code == YANK ){
			fmtprint(STDOUT, "cmd->regex:\t%s\n", citer->regex);
			fmtprint(STDOUT, "cmd->replace:\t%s\n", citer->text);
		}
		fmtprint(STDOUT, "cmd->text:\t%s\n", citer->text);
		if( citer->cmd != NULL && citer->code == '{' ){
			nextp = citer->cmd;
			fmtprintout("Group started\n");
		}
		if( citer->code == ENDGROUP ){
			fmtprintout("Group ended\n");
		}
	}
#endif
	return SUCCESS;
}
