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
	char buff[BUFF_SIZE], rbuff[BUFF_SIZE];
	char *it = NULL, *tmp = NULL;
	int has_line = 0;
	int cnt = 0, nbytes = 0;
	if( *spaceptr == NULL ){
		*spaceptr = malloc(sizeof(sspace_t));
		(*spaceptr)->text = NULL;
		(*spaceptr)->len = 0;
		(*spaceptr)->buff = NULL;
		(*spaceptr)->buff_len = 0;
		(*spaceptr)->is_deleted = 0;
	}
	sspace_t *space = *spaceptr;
	while( !has_line ){
		int curpos = lseek(fd, 0, SEEK_CUR);
		if( curpos < 0 && errno != EINVAL ){
			return -errno;
		}

		int offset = lseek(fd, curpos, SEEK_DATA);
		if( offset < 0 && errno != EINVAL ){
			if( errno == ENXIO ){
				break;
			}else{
				return -errno;
			}
		}
		nbytes = read(fd, buff, BUFF_SIZE);
		if( nbytes < 0 ){
			return -errno;
		}

		if( nbytes == 0 ) break;

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

		int cnt = nbytes;
		char *tok = buff;
		while( *tok != NEWLINE && *tok ){
			*it = *tok;
			it++;
			tok++;
			cnt--;
		}

		if( *tok == NEWLINE ){
			has_line = 1;
		}
	}


	return ENXIO;
}

