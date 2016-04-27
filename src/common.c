#define _GNU_SOURCE
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <assert.h>
#include "cmd.h"
#include "common.h"

static char *strrev(char *str) {
	char *p1, *p2;

	if (! str || ! *str)
		return str;
	for (p1 = str, p2 = str + strlen(str) - 1; p2 > p1; ++p1, --p2) {
		*p1 ^= *p2;
		*p2 ^= *p1;
		*p1 ^= *p2;
	}
	return str;
}

static void itoa(int n, char s[]){
	int i, sign;

	if ((sign = n) < 0){
		n = -n;
	}
	i = 0;
	do {
		s[i++] = n % 10 + '0';
	} while ((n /= 10) > 0);
	if (sign < 0){
		s[i++] = '-';
	}
	s[i] = '\0';
	strrev(s);
}

void fmtprint(int fd, const char *str, ...){
	int num;
	char buff[NUM_BUFF], c;
	const char *s;
	va_list argptr;
	va_start(argptr, str);
	for(; *str; str++){
		if (*str == '%'){
			str++;
			switch(*str){
				case 'd':
					num = va_arg(argptr, int);
					itoa(num, buff);
					write(fd, buff, strlen(buff));
				break;
				case 's':
					s = va_arg(argptr, const char*);
					if( s != NULL ){
						write(fd, s, strlen(s));
					}
				break;
				case 'c':
					c = va_arg(argptr, int);
					write(fd, &c, 1);
				break;
				case '%':
					write(fd, "%", 1);
				break;
				case '\0': return;
				default: break;
			}
		}
		else{
		write(fd, str, 1);
		}
	}
	va_end(argptr);
}

int s_getline(sspace_t **spaceptr, int fd){
	assert(spaceptr != NULL);
	char buff[BUFF_SIZE+1];
	char *it = NULL, *tmp = NULL;
	int has_line = 0;
	int cnt = 0, nbytes = 0, nread = 0, rread = 0, result = 0;
	if( *spaceptr == NULL ){
		*spaceptr = malloc(sizeof(sspace_t));
		(*spaceptr)->text = NULL;
		(*spaceptr)->len = BUFF_SIZE+1;
		(*spaceptr)->buff = NULL;
		(*spaceptr)->buff_len = 0;
		(*spaceptr)->offset = 0;
		(*spaceptr)->is_deleted = 0;
	}
	sspace_t *space = *spaceptr;
	char *rtok = NULL;
	buff[BUFF_SIZE] = '\0';
	if( space->buff_len > 0 ){
		space->text = malloc(space->buff_len+1);
		it = space->text;
		rtok = &space->buff[space->offset];
		cnt = space->buff_len - space->offset;
		while( *rtok ){
			rread++;
			cnt--;
			if( has_line ) break;
			*it++ = *rtok++;
			if( *rtok == NEWLINE ){
				has_line = 1;
				*it = '\0';
			}
		}
	}
	while( !has_line ){
		nread = 0;
		int curpos = lseek(fd, 0, SEEK_CUR);
		if( curpos < 0 && errno != EINVAL ){
			return -errno;
		}

		int offset = lseek(fd, curpos, SEEK_DATA);
		if( offset < 0 && errno != EINVAL ){
			if( errno == ENXIO ){
				result = 0;
				break;
			}else{
				return -errno;
			}
		}
		nbytes = read(fd, buff, BUFF_SIZE);
		if( nbytes < 0 ){
			return -errno;
		}

		if( nbytes == 0 ){
			result = 0;
			break;
		}
		space->len += nbytes;
		tmp = realloc(space->text, space->len);
		if( tmp == NULL ){
			return -errno;
		}
		space->text = tmp;
		tmp = NULL;

		if( it == NULL ){
			it = space->text;
		}

		char *tok = buff;
		cnt = nbytes;
		while( *tok ){
			cnt--;
			nread++;
			if( has_line ) break;
			*it++ = *tok++;
			if( *tok == NEWLINE ){
				has_line = 1;
				*it = '\0';
			}
		}
	}

	if( cnt > 0 ){
		result = 1;
		if( space->buff == NULL ){
			space->buff = malloc(cnt+1);
			space->buff_len = cnt;
			strncpy(space->buff, &buff[nread], cnt);
		}
		space->offset = rread;
	}else if( space->buff != NULL ){
		space->buff_len = 0;
		space->offset = 0;
		free(space->buff);
	}
	if( has_line ) result = 1;
	return result;
}
