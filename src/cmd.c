#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <regex.h>

#include "cmd.h"
#include "common.h"

static const char *err_msgs[ERR_MAX] = {
	"Success\n",
	"Unable to read given file\n",
	"Invalid token\n",
	"Illegal character\n",
	"Regex must not be empty\n",
	"Wrong mark given for substr or yank command\n",
	"Unable to compile regex",
	"Malloc failed",
};

const char *err_msg(int err_code){
	if( err_code < 0 || err_code > ERR_MAX ) return err_msgs[0];
	return err_msgs[err_code];
}

int parse_script(const char script[], scmd_t **cmd_list, unsigned int eflags){
	char num_buff[NUM_BUFF_SIZE], txt_buff[BUFF_SIZE],
		 *num_buff_iter = num_buff, *txt_buff_iter = txt_buff, prev_tok;
	const char *tok = script;
	int is_cmd = 0, is_num = 0, is_re = 0, is_comment = 0,
		is_replace = 0, need_text = 0, text_newline = 0;
	int code = -1, num = 0, stacki = 0;
	int re_pending = 0, num_pending = 0;
	saddr_t *baddr = NULL, *eaddr = NULL;
	scmd_t *current, *grpstack[STACK_SIZE];

	for(; *tok; tok++){
		if( !is_re ){
			switch(*tok){
				case SPACE:
					continue;
				break;

				case COMMENT:
					is_comment = 1;
				break;

				case ESCAPE_CHAR:
					fmtprintout("ntok: '%c' '%c'\n", *tok, *(tok+1));
				break;

				case NEWLINE: case SEMICOLON:
					is_comment = !(*tok == NEWLINE);
					is_replace = 0;
					if( need_text && prev_tok != ESCAPE_CHAR ){
			fmtprintout("tt: '%c' '%c'\n", *tok, *(tok-1));
						*txt_buff_iter = '\0';
						txt_buff_iter = txt_buff;
						strncpy(current->text, txt_buff, BUFF_SIZE);
						need_text = 0;
					}
				break;
			}
		}

		if( is_comment ){
			switch(prev_tok){
				case COMMA: case RE_CHAR:
				return EILLEGALCHAR;
			}
			continue;
		}

		if( need_text ){
			*txt_buff_iter++ = *tok;
			prev_tok = *tok;
			continue;
		}

		/* Start or end RE block */
		if( *tok == RE_CHAR ){
			if( is_num ){
				return EILLEGALCHAR;
			}
			if( is_re && prev_tok != ESCAPE_CHAR ){
				*txt_buff_iter = '\0';
				txt_buff_iter = txt_buff;
				is_re = 0;
				re_pending = 1;
				/* If substr or yank we grab patterns here */
				if( is_replace ){
					if( current->regex == NULL ){
						if( strcmp(txt_buff, "") == 0){
							return ENOREGEX;
						}
						current->regex = malloc(sizeof(current->regex));
						if( current->regex == NULL ){
							return EMALLOC;
						}
						int rc = regcomp(current->regex, txt_buff, eflags);
						if( rc != 0 ){
							return EWRONGREGEX;
						}
						is_re = 1;
						fmtprintout("re: %s\n", txt_buff);
					}else{
						fmtprintout("tx: %s\n", txt_buff);
						strncpy(current->text, txt_buff, BUFF_SIZE);
					}
					re_pending = 0;
				}
				continue;
			}
			is_re = 1;
			continue;
		}

		if( is_re ){
			*txt_buff_iter++ = *tok;
			continue;
		}

		/* Grab numbers from input */
		if( *tok >= '0' && *tok <= '9' ){
			*num_buff_iter = *tok;
			num_buff_iter++;
			is_num = 1;
			continue;
		}else if( is_num ){
			*num_buff_iter = '\0';
			num_buff_iter = num_buff;
			is_num = 0;
			num = atoi(num_buff);
			num_pending = 1;
		}

		if( num_pending ){
			saddr_t *p;
			if( baddr == NULL || eaddr == NULL ){
				p = malloc(sizeof(saddr_t));
				p->type = LINE_ADDR;
				p->line = num;
				if( baddr == NULL )      baddr = p;
				else if( eaddr == NULL ) eaddr = p;
			}
			num_pending = 0;
		}

		if( re_pending ){
			if( strcmp(txt_buff, "") == 0 ){
				return ENOREGEX;
			}
			saddr_t *p;
			if( baddr == NULL || eaddr == NULL ){
				p = malloc(sizeof(saddr_t));
				p->type = REGEX_ADDR;
				p->regex = malloc(sizeof(regex_t));
				if( p->regex == NULL ){
					return EMALLOC;
				}
				int rc = regcomp(p->regex, txt_buff, eflags);
				if( rc != 0 ){
					fmtprint(STDOUT, "reg: %s\n", txt_buff);
					return EWRONGREGEX;
				}
				if( baddr == NULL )      baddr = p;
				else if( eaddr == NULL ) eaddr = p;
			}
			re_pending = 0;
		}
		/* Do cmd stuff */
		if( is_replace ){
			int has_mark = 0;
			switch(*tok){
				case 'g':
					fmtprint(STDOUT, "global\n");
					has_mark = 1;
				break;
			}
			is_replace = 0;
			if( !has_mark ){
				return EWRONGMARK;
			}
			continue;
		}

		/* Find cmd */
		int i;
		for(i = 0; i < SCMD_NUM; i++){
			if( tokens[i] == *tok ){
				is_cmd = 1;
				code = i;
				is_replace = ( code == SUBST || code == YANK );
				text_newline = ( code == INSERT || code == APPEND );
				need_text = ( code == LABEL || code == BRANCH || text_newline );
				break;
			}
		}

		prev_tok = *tok;
		if( is_cmd ){
			if( *cmd_list == NULL ){
				current = malloc(sizeof(scmd_t));
				*cmd_list = current;
				current->next = NULL;
			}else{
				current->next = malloc(sizeof(scmd_t));
			}
			if( code == GROUP ){
				/* Add group to stack */
				if( stacki >= STACK_SIZE ) return ESOVERFLOW;
				grpstack[stacki++] = current;
			}
			if( code == ENDGROUP ){
				/* Remove group to stack */
				if( stacki == 0 ) return EILLEGALCHAR;
				stacki--;
				grpstack[stacki]->cmd = grpstack[stacki]->next;
				grpstack[stacki]->next = current->next;
			}
			if( current->next != NULL ){
				current = current->next;
			}
			current->next = NULL;
			current->cmd = NULL;
			current->regex = NULL;
			current->code = code;
			current->baddr = baddr;
			current->eaddr = eaddr;
			baddr = NULL;
			eaddr = NULL;
			is_cmd = 0;
		}else{
			switch(*tok){
					case COMMA: case NEWLINE: case SPACE: case '\0':
					continue;
			}
			if( *tok <= SPACE ){
				continue;
			}
					/*fmtprintout("'%c' '%c'\n", *(tok), *(tok-1));*/
			return EINVALTOKEN;
		}
	}
	/* DEBUG */
	fmtprint(STDOUT, "[INFO]\n");
	fmtprint(STDOUT, "script:\t'%s'\n", script);
	scmd_t *iter = *cmd_list;
	for(; iter; iter = (iter->cmd ? iter->cmd : iter->next)){
		fmtprint(STDOUT, "\ncommand:\t%d\n", iter->code);
		if( iter->baddr != NULL ){
			fmtprint(STDOUT, "baddr->line:\t%d\n", iter->baddr->line);
			fmtprint(STDOUT, "baddr->regex:\t%s\n", iter->baddr->regex);
		}

		if( iter->eaddr != NULL ){
			fmtprint(STDOUT, "eaddr->line:\t%d\n", iter->eaddr->line);
			fmtprint(STDOUT, "eaddr->regex:\t%s\n", iter->eaddr->regex);
		}

		if( iter->code == SUBST || iter->code == YANK ){
			fmtprint(STDOUT, "cmd->regex:\t%s\n", iter->regex);
			fmtprint(STDOUT, "cmd->replace:\t%s\n", iter->text);
		}
		fmtprint(STDOUT, "cmd->text:\t%s\n", iter->text);
		if( iter->cmd != NULL ){
			fmtprintout("Group started\n");
		}
		if( iter->code == ENDGROUP ){
			fmtprintout("Group ended\n");
		}
	}
	return SUCCESS;
}

static int match(const char *string, regex_t *pattern){
	int status = regexec(pattern, string, (size_t) 0, NULL, 0);
    if( status != 0 ) {
		/* TODO: ЕГГОГ */
        return 0;
    }
	return 1;
}

static int replace(sspace_t *space, char *replacement, regex_t *pattern,
													unsigned int flags){
	int a = 0, cnt = 0;
	int offset = 0;
	regmatch_t pm;
	a = regexec(pattern, &space->space[0], 1, &pm, REG_EXTENDED);

	while( a == 0 ){
		printf("\nmatch at %.*s %d\n", (pm.rm_eo-pm.rm_so),
						&space->space[offset+pm.rm_so], offset+pm.rm_so);
		int bmatch = offset+pm.rm_so;
		int ematch = offset+pm.rm_eo;
		int len = pm.rm_eo-pm.rm_so;
		int rlen = strlen(replacement);
		int diff = rlen - len;
		/* Check avail */
		space->len += BUFF_SIZE;
		char *tmp = realloc(space->space, space->len);
		if( tmp == NULL ){
			return -errno;
		}
		space->text = tmp;
		printf("%s: %s\n", space->space+bmatch, replacement);
		memmove(space->space+ematch+diff, space->space+ematch,
						space->len-ematch);
		strncpy(space->space+bmatch, replacement, rlen);
		/* memmove */
		offset += pm.rm_eo+diff;
		cnt++;
		a = regexec(pattern, &space->space[0] + offset, 1, &pm, 0);
	}
	return 1;
}

int run_line(scmd_t *cmd_list, sspace_t *pspace, sspace_t *hspace,
												 const int line){
	pspace->space = pspace->text;
	pspace->text = NULL;
	scmd_t *cmd = cmd_list;
	pspace->is_deleted = 0;
	scmd_t *nextp = cmd->next;
	for(; cmd; cmd = nextp){
		nextp = cmd->next;
		int bline = 0, eline = 0;
		regex_t *bre = NULL, *ere = NULL;
		if( cmd->baddr != NULL && cmd->baddr->type == LINE_ADDR ){
			bline = cmd->baddr->line;
		}

		if( cmd->eaddr != NULL && cmd->eaddr->type == LINE_ADDR ){
			eline = cmd->eaddr->line;
		}

		if( cmd->baddr != NULL && cmd->baddr->type == REGEX_ADDR ){
			bre = cmd->baddr->regex;
		}

		if( cmd->eaddr != NULL && cmd->eaddr->type == REGEX_ADDR ){
			ere = cmd->eaddr->regex;
		}

		if( bline > 0 && bline > line ) continue;
		if( eline > 0 && eline < line ) continue;
		if( eline == 0 && bline > 0 && bline != line ) continue;
		if( bre != NULL && !match(pspace->space, bre) ) continue;
		if( ere != NULL && !match(pspace->space, ere) ) continue;

		switch(cmd->code){
			case QUIT:
				return RQUIT;
			break;

			case DELETE:
				pspace->is_deleted = 1;
			break;

			case PRINT:
				return RPRINT;
			break;

			case SUBST:
				replace(pspace, cmd->text, cmd->regex, 0);
			break;

			case GROUP:
				nextp = cmd->cmd;
			break;
			default:
			break;
		}
	}
	return RCONT;
}

static void reset_space(sspace_t *pspace){
	free(pspace->space);
	pspace->is_deleted = 0;
	pspace->len = 0;
}

int run_script(const char script[], int fd, unsigned int flags){
	scmd_t *cmd_list = NULL;
	int result = parse_script(script, &cmd_list, REG_EXTENDED);
	if( result != 0 ){
		return result;
	}
	sspace_t *pspace = NULL, hspace;
	enum cmd_result code = RNONE;
	int line = 1, nflag = flags & NFLAG;

	while( (result = s_getline(&pspace, fd) > 0) ){
		/*fmtprint(STDOUT, "\n\n[LINE] '%s'\n\n", pspace->text);*/
		if( code != RNONE ){
			if( code == RPRINT ){
				fmtprint(STDOUT, "%s", pspace->space);
			}
			if( !pspace->is_deleted && !nflag ){
				fmtprint(STDOUT, "%s", pspace->space);
			}
			reset_space(pspace);
			switch(code){
				case RQUIT:
					return SUCCESS;
				default:
				break;
			}
		}
		code = run_line(cmd_list, pspace, &hspace, line);
		line++;
	}
	if( code == RPRINT ){
		fmtprint(STDOUT, "%s", pspace->space);
	}
	if( !pspace->is_deleted && !nflag ){
		fmtprint(STDOUT, "%s", pspace->space);
	}
	return result;
}
