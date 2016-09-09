#define _GNU_SOURCE
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <assert.h>
#include <regex.h>
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
static void printc(int fd, char c, char buff[], int *nwrite){
	assert(buff != NULL);
	assert(nwrite != NULL);
	int flush = (*nwrite >= BUFF_SIZE-1 || c == '\n' || c == '\0');
	if( *nwrite > BUFF_SIZE ) *nwrite = BUFF_SIZE-1;
	if( flush ){
		write(STDOUT, buff, *nwrite);
		*nwrite = 0;
	}
	buff[(*nwrite)++] = c;
}

void fmtprint(int fd, const char *fmt, ...){
	assert(fmt != NULL);
	char buff[BUFF_SIZE];
	char chunk[MIN_CHUNK];
	memset(buff, 0, BUFF_SIZE);
	memset(chunk, 0, MIN_CHUNK);
	va_list argptr;
	va_start(argptr, fmt);
	int nwrite = 0;
	for(; *fmt; fmt++){
		if( *fmt == '%' ){
			fmt++;
			switch(*fmt){
				case 'd': {
					int num = va_arg(argptr, int);
					itoa(num, chunk);
					for(int i = 0; chunk[i]; i++){
						printc(fd, chunk[i], buff, &nwrite);
					}
				}
				break;

				case 's': {
					const char *s = va_arg(argptr, const char*);
					for(; *s; s++){
						printc(fd, *s, buff, &nwrite);
					}
				}
				break;

				case 'c': {
					char c = va_arg(argptr, int);
					printc(fd, c, buff, &nwrite);
				}
				break;

				case '%':
					printc(fd, '%', buff, &nwrite);
				break;

				case '\0':
					printc(fd, '\0', buff, &nwrite);
					return;
			}
			continue;
		}
		printc(fd, *fmt, buff, &nwrite);
	}
	printc(fd, '\0', buff, &nwrite);
	va_end(argptr);
}

#define OUT_PUSH(str, len) { \
	int left = len, str_pos = 0; \
	while( left > 0 ){ \
		while( left > 0 && buff_pos < BUFF_SIZE ){ \
			out_buff[buff_pos++] = str[str_pos++]; \
			left--; \
		} \
		out_buff[BUFF_SIZE] = '\0'; \
		write(fd, out_buff, BUFF_SIZE); \
		buff_pos = 0; \
	} \
}

/*void fmtprint(int fd, const char *str, ...){
	int num;
	char buff[NUM_BUFF], c;
	char out_buff[BUFF_SIZE];
	memset(&out_buff, 0, BUFF_SIZE);
	int buff_pos = 0;
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
					OUT_PUSH(buff, strlen(buff));
				break;
				case 's':
					s = va_arg(argptr, const char*);
					if( s != NULL ){
						OUT_PUSH(s, strlen(s));
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
}*/

static int s_getc(sspace_t *space, int fd){
	assert(space != NULL);

	if( space->offset > 0 && space->offset < space->nread ){
		return space->buff[space->offset++];
	}
	if( space->offset == space->nread ) space->offset = 0;

	int can_seek = 0, nbytes = 0;
	int curpos = lseek(fd, 0, SEEK_CUR);
	if( curpos < 0 ){
		can_seek = 0;
	}

	if( can_seek ){
		int offset = lseek(fd, curpos, SEEK_DATA);
		if( offset < 0 && errno != EINVAL ){
			if( errno == ENXIO ){
				space->buff[space->offset] = S_EOF;
			}else{
				return -errno;
			}
		}
	}
	nbytes = read(fd, space->buff, BUFF_SIZE);
	space->nread = nbytes;
	if( nbytes < 0 ){
		return -errno;
	}

	if( nbytes == 0 ){
		space->buff[space->offset] = S_EOF;
	}
	return space->buff[space->offset++];
}

int s_getline(sspace_t *space, int fd){
	assert(space != NULL);
	char **lineptr = &space->text;
	size_t *n = &space->len;

	int nchars_avail;	/* Allocated but unused chars in *LINEPTR.	*/
	char *read_pos;	/* Where we're reading into *LINEPTR. */
	int ret;

	if( !*lineptr ){
		*n = MIN_CHUNK;
		*lineptr = malloc(*n);
		if( !*lineptr ){
			errno = ENOMEM;
			return -1;
		}
	}

	nchars_avail = *n;
	read_pos = *lineptr;

	for(;;){
		int c = s_getc(space, fd);
		if( c < S_EOF ){
			return c;
		}

		assert((*lineptr + *n) == (read_pos + nchars_avail));
		if (nchars_avail < 2) {
			if (*n > MIN_CHUNK)
				*n *= 2;
			else
				*n += MIN_CHUNK;

			nchars_avail = *n + *lineptr - read_pos;
			*lineptr = realloc (*lineptr, *n);
			if (!*lineptr) {
				errno = ENOMEM;
				return -1;
			}
			read_pos = *n - nchars_avail + *lineptr;
			assert((*lineptr + *n) == (read_pos + nchars_avail));
		}

		if (c == S_EOF) {
			/* Return partial line, if any.	*/
			if (read_pos == *lineptr)
				return -1;
			else
				break;
		}

		*read_pos++ = c;
		nchars_avail--;

		if( c == NEWLINE )
			/* Return the line.	*/
			break;
	}

	/* Done - NUL terminate and return the number of chars read. */
	*read_pos = '\0';

	ret = read_pos - *lineptr;
	return ret;
}
