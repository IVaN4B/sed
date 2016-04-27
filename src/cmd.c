#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include "cmd.h"
#include "common.h"

static const char *err_msgs[ERR_MAX] = {
	"Success\n",
	"Unable to read given file\n",
	"Invalid token\n",
	"Illegal character\n",
	"Regex must not be empty\n",
	"Wrong mark given for substr or yank command\n",
};

const char *err_msg(int err_code){
	if( err_code < 0 || err_code > ERR_MAX ) return err_msgs[0];
	return err_msgs[err_code];
}

int parse_script(const char script[], scmd_t **cmd_list){
	char num_buff[NUM_BUFF_SIZE], re_buff[BUFF_SIZE],
		 *num_buff_iter = num_buff, *re_buff_iter = re_buff, prev_tok;
	const char *tok = script;
	int is_cmd = 0, is_num = 0, is_re = 0, is_comment = 0,
		is_replace = 0;
	int code = -1, num = 0;
	int re_pending = 0, num_pending = 0;
	saddr_t *baddr = NULL, *eaddr = NULL;
	scmd_t *current;

	for(; *tok; tok++){
		/* Start or end RE block */
		if( *tok == RE_CHAR && !is_comment ){
			if( is_num ){
				return EILLEGALCHAR;
			}
			if( is_re && prev_tok != ESCAPE_CHAR ){
				*re_buff_iter = '\0';
				re_buff_iter = re_buff;
				is_re = 0;
				re_pending = 1;
				/* If substr or yank we grab patterns here */
				if( is_replace ){
					if( strcmp(current->regex, "") == 0 ){
						if( strcmp(re_buff, "") == 0){
							return ENOREGEX;
						}
						strncpy(current->regex, re_buff, BUFF_SIZE);
						is_re = 1;
					}else{
						strncpy(current->text, re_buff, BUFF_SIZE);
					}
					re_pending = 0;
				}
				continue;
			}
			is_re = 1;
			continue;
		}
		if( is_re ){
			*re_buff_iter = *tok;
			re_buff_iter++;
			continue;
		}

		switch(*tok){
			case COMMENT:
				is_comment = 1;
			break;

			case NEWLINE:
				is_comment = 0;
				is_replace = 0;
			break;
		}

		if( is_comment ){
			switch(prev_tok){
				case COMMA: case RE_CHAR:
				return EILLEGALCHAR;
			}
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
			if( strcmp(re_buff, "") == 0 ){
				return ENOREGEX;
			}
			saddr_t *p;
			if( baddr == NULL || eaddr == NULL ){
				p = malloc(sizeof(saddr_t));
				p->type = REGEX_ADDR;
				strncpy(p->regex, re_buff, BUFF_SIZE);
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
			}
		}

		prev_tok = *tok;
		if( is_cmd ){
			if( *cmd_list == NULL ){
				current = malloc(sizeof(scmd_t));
				*cmd_list = current;
			}else{
				current->next = malloc(sizeof(scmd_t));
				current = current->next;
			}
			current->next = NULL;
			current->code = code;
			current->baddr = baddr;
			current->eaddr = eaddr;
			baddr = NULL;
			eaddr = NULL;
			is_cmd = 0;
		}else{
			switch(*tok){
				case COMMA: case NEWLINE:
				continue;
			}
			return EINVALTOKEN;
		}
	}
	/* DEBUG */
	fmtprint(STDOUT, "[INFO]\n");
	fmtprint(STDOUT, "script:\t'%s'\n", script);
	scmd_t *iter = *cmd_list;
	for(; iter; iter = iter->next){
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
	}
	return SUCCESS;
}

int run_line(scmd_t *cmd_list, sspace_t *pspace, sspace_t *hspace,
												 const int line){
	pspace->space = pspace->text;
	pspace->text = NULL;
	scmd_t *cmd = cmd_list;
	pspace->is_deleted = 0;
	for(; cmd; cmd = cmd->next){
		int bline = 0, eline = 0;
		if( cmd->baddr != NULL && cmd->baddr->type == LINE_ADDR ){
			bline = cmd->baddr->line;
		}

		if( cmd->eaddr != NULL && cmd->eaddr->type == LINE_ADDR ){
			eline = cmd->eaddr->line;
		}
		if( bline > 0 && bline > line ) continue;
		if( eline > 0 && eline < line ) continue;
		if( eline == 0 && bline > 0 && bline != line ) continue;
		switch(cmd->code){
			case QUIT:
				return RQUIT;
			break;
			case DELETE:
				pspace->is_deleted = 1;
			break;

			case PRINT:
				fmtprint(STDOUT, "%s\n", pspace->space);
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

int run_script(const char script[], int fd, int flags){
	scmd_t *cmd_list = NULL;
	int result = parse_script(script, &cmd_list);
	if( result > 0 ){
		return result;
	}
	sspace_t *pspace = NULL, hspace;
	enum cmd_result code = RNONE;
	int line = 1;
	while( (result = s_getline(&pspace, fd) > 0) ){
		if( code != RNONE ){
			if( !pspace->is_deleted ){
				fmtprint(STDOUT, "%s\n", pspace->space);
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
	if( !pspace->is_deleted ){
		fmtprint(STDOUT, "%s\n", pspace->space);
	}
	if ( result < 0 ){
		return result;
	}
	return SUCCESS;
}
