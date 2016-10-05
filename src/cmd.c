#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <regex.h>
#include <assert.h>

#include "cmd.h"
#include "common.h"

static char *sub_buff = NULL;
static size_t sub_buff_len = MIN_CHUNK;
static size_t sub_strlen = 0;

static char *outbuff = NULL;
static size_t outbufflen = MIN_CHUNK;

static int append_outbuff(char *str){
	if( str == NULL ) return 0;
	int len = strlen(str);
	if( outbufflen < len*2 ){
		outbufflen = len*2;
		TRY_REALLOC(outbuff, outbufflen);
	}
	strcat(outbuff, str);
	return 0;
}

static int match(const char *string, regex_t *pattern){
	assert(pattern != NULL);
	int status = regexec(pattern, string, (size_t) 0, NULL, 0);
    if( status != 0 ) {
		/* TODO: ЕГГОГ */
        return 0;
    }
	return 1;
}

static int substring(char *strp,
					 char *repl,
					 int repl_len,
					 int mstart,
					 int mend,
					 int n){
	int count = sub_strlen;
	for(; strp[n]; n++){
		if( sub_buff_len < count ){
			sub_buff_len = count*2;
			TRY_REALLOC(sub_buff, sub_buff_len);
		}
		if( n == mstart ){
			strncpy(&sub_buff[count], repl, repl_len);
			count += repl_len;
			continue;
		}
		if( n > mstart && n < mend ) continue;
		if( n == mend ) break;
		sub_buff[count++] = strp[n];
	}
	sub_strlen = count;
	return n;
}

#define GRP_MAX 9
static char *grp_replace(char *repl, int *repl_len,
						 char grp[GRP_MAX][BUFF_SIZE]){
	assert(repl != NULL);
	assert(repl_len != NULL);
	int n = 0, count = 0;
	char *buff = malloc(*repl_len*2);
	int cap = *repl_len*2;

	for(; repl[n]; n++){
		switch(repl[n]){
			case '\\':
				if( IS_NUM(repl[n+1]) ){
					int pos = repl[n+1] - '0';
					if( pos == 0 || pos > 9 ) return NULL;
					pos--;
					if( grp[pos][0] == '\0' ) continue;
					int len = strlen(grp[pos]);
					*repl_len += len;
					if( cap < *repl_len ){
						cap *= 2;
						TRY_REALLOC(buff, cap);
					}
					strncpy(&buff[count], grp[pos], len);
					count += len;
				}else{
					if( repl[n+1] == 'n' ){
						buff[count++] = '\n';
					}
				}
				n += 2;
			break;
		}
		if( repl[n] == '\\' ) {n--; continue; }
		if( repl[n] )
			buff[count++] = repl[n];
	}
	buff[count] = '\0';
	*repl_len = count;
	return buff;
}

static int replace(sspace_t *space, char *replacement, regex_t *pattern,
													unsigned int flags){
	assert(pattern != NULL);
	assert(space != NULL);
	int res = 0, is_global = flags & SFLAG_G;
	int offset = 0;
	int repl_len = strlen(replacement);
	char *repl = NULL;

	int space_len = strlen(space->space);
	int ngroups = pattern->re_nsub+1;
	regmatch_t *groups = malloc(ngroups*sizeof(regmatch_t));

	if( sub_buff_len < space->len ){
		sub_buff_len = space->len;
		TRY_REALLOC(sub_buff, sub_buff_len);
	}

	res = regexec(pattern, &space->space[0], ngroups, groups, 0);

	int shift = 0, times_matched = 0;
	char grp[GRP_MAX][BUFF_SIZE] = { {'\0'} };
	while( res == 0 ){
		times_matched++;
		size_t nmatched = 0;
		if( repl_len > 0 ){
			int imatch = 0;
			for(nmatched = 1; nmatched < ngroups; nmatched++){
				regmatch_t pm = groups[nmatched];
				if( pm.rm_so == (size_t)-1) break;

				int bmatch = offset+pm.rm_so;
				int len = pm.rm_eo-pm.rm_so;
				if( imatch < GRP_MAX ){
					strncpy(grp[imatch++], space->space+bmatch, len);
				}
			}
		}
		int gi = 0, has_groups = 0;
		for(gi = 0; gi < 9; gi++){
			if( grp[gi][0] != '\0'){
				has_groups = 1;
			}
		}
		if( has_groups ){
			repl = grp_replace(replacement, &repl_len, grp);
			if( repl == NULL ) return 0;
		}else{
			repl = replacement;
		}
		nmatched = 0;
		/*for(nmatched = 0; nmatched < ngroups; nmatched++){*/
			regmatch_t pm = groups[nmatched];
			if( pm.rm_so == (size_t)-1 ) break;
			/*printf("\nmatch at %.*s %d\n", (pm.rm_eo-pm.rm_so),*/
							/*&space->space[offset+pm.rm_so], offset+pm.rm_so);*/
			/* Do replace */
			int bmatch = offset+pm.rm_so;
			int ematch = offset+pm.rm_eo;
			int prev_shift = shift;
			shift = substring(space->space, repl, repl_len, bmatch,
															  ematch,
															  shift);
			if( shift < 0 ){
				return shift;
			}
			if( shift == prev_shift ) break;
			offset += pm.rm_eo;

		/*}*/
		if( offset >= space_len || !is_global ) break;
		res = regexec(pattern, &space->space[0] + offset, ngroups, groups,
																 0);
	}
	/* If chars left */
	if( space->space[shift] && times_matched > 0 ){
		shift = substring(space->space, repl, repl_len, -1, -1, shift);
	}
	sub_buff[sub_strlen] = '\0';
	if( space->len < sub_buff_len ){
		space->len = sub_buff_len;
		TRY_REALLOC(space->space, space->len);
	}
	if( sub_buff[0] != '\0' ){
		strcpy(space->space, sub_buff);
		if( flags & SFLAG_P ){
			append_outbuff(space->space);
		}
	}
	sub_buff[0] = '\0';
	sub_strlen = 0;
	return 1;
}

int run_line(scmd_t *cmd_list, sspace_t *pspace, sspace_t *hspace,
												 const int line,
												 unsigned int flags){
	TRY_REALLOC(pspace->space, pspace->len);
	strcpy(pspace->space, pspace->text);
	scmd_t *cmd = cmd_list;
	scmd_t *nextp = cmd->next;
	int prev_res = 0, prev_code = 0;
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
		int exec = 1;
		if( bline > 0 && bline > line ) exec = 0;
		if( eline > 0 && eline < line ) exec = 0;
		if( eline == 0 && bline > 0 && bline != line ) exec = 0;
		if( bre != NULL && !match(pspace->space, bre) ) exec = 0;
		if( ere != NULL && !match(pspace->space, ere) ) exec = 0;
		if( prev_code == '!' ) exec = !exec;
		prev_code = cmd->code;
		if( !exec ) continue;
		switch(cmd->code){
			case 'q':
				return RQUIT;
			break;

			case 'd':
				pspace->is_deleted = 1;
			break;

			case 'p': {
				int res = append_outbuff(pspace->space);
				if( res < 0 ){
					return res;
				}
				break;
			}

			case 's':
				cmd->result = replace(pspace, cmd->text,
								  cmd->regex, cmd->flags | flags);
			break;

			case '{': case 'b':
				nextp = cmd->cmd;
			break;

			case 't':
				if( prev_res ){
					nextp = cmd->cmd;
				}
			break;

			case 'g':
				if( hspace->space != NULL && hspace->len > 0 ){
					TRY_REALLOC(pspace->space, hspace->len);
					memcpy(pspace->space, hspace->space, hspace->len);
				}
			break;

			case 'h':
				if( pspace->len > 0 ){
					TRY_REALLOC(hspace->space, pspace->len);
					memcpy(hspace->space, pspace->space, pspace->len);
				}
			break;

			case 'x':
				if( hspace->space != NULL ){
					int len_tmp = pspace->len;
					char *tmp = pspace->space;
					pspace->space = hspace->space;
					pspace->len = hspace->len;
					hspace->space = tmp;
					hspace->len = len_tmp;
				}else{
					pspace->space[0] = '\0';
				}
			break;

			case 'i': case 'a':
				if( cmd->code == 'i' ) outbuff[0] = '\0';
				append_outbuff(cmd->text);
			break;

			default:
			break;
		}
		prev_res = cmd->result;
	}
	return RCONT;
}

static int print_space(sspace_t *space){
	if( !space->is_deleted ){
		int res = append_outbuff(space->space);
		if( res < 0 ){
			return res;
		}
	}
	fmtprintout("%s", outbuff);
	outbuff[0] = '\0';
	return 0;
}

static void scmd_list_free(scmd_t *cmd_list){
	assert(cmd_list != NULL);
	scmd_t *node;
	node = cmd_list;
	while( node != NULL ){
		scmd_t *next = node->next;
		if( node->baddr != NULL ){
			free(node->baddr);
		}
		if( node->eaddr != NULL ){
			free(node->eaddr);
		}
		if( node->regex != NULL ){
			regfree(node->regex);
		}
		free(node);
		node = next;
	}
}

int run_script(const char script[], int fd, unsigned int flags){
	scmd_t *cmd_list = NULL;
	int result = parse_script(script, &cmd_list, flags);
	if( result != 0 ){
		return result;
	}
	sspace_t pspace, hspace;
	memset(&pspace, 0, sizeof(sspace_t));
	memset(&hspace, 0, sizeof(sspace_t));
	enum cmd_result code = RNONE;
	int line = 1, nflag = flags & NFLAG;
	TRY_REALLOC(outbuff, MIN_CHUNK);
	TRY_REALLOC(sub_buff, MIN_CHUNK);
	outbuff[0] = '\0';
	sub_buff[0] = '\0';
	while( (result = s_getline(&pspace, fd) > 0 ) ){
		if( code != RNONE ){
			result = print_space(&pspace);
			if( result < 0 ){
				goto exit;
			}
			switch(code){
				case RQUIT:
					return SUCCESS;
				default:
					if( code < 0 ){
						result = code;
						goto exit;
					}
				break;
			}
		}
		pspace.is_deleted = nflag;
		code = run_line(cmd_list, &pspace, &hspace, line, flags);
		line++;
	}
	result = print_space(&pspace);
exit:
	/* TODO: clean */
	scmd_list_free(cmd_list);
	free(outbuff);
	free(sub_buff);
	if( pspace.space != NULL ){
		free(pspace.space);
	}
	if( pspace.text != NULL ){
		free(pspace.text);
	}
	return result;
}
