#ifndef COMMON_H
#define COMMON_H
#define NUM_BUFF 32
#define MIN_CHUNK 64
#define S_EOF -1
#define fmtprintout(str...) fmtprint(STDOUT, str)
void fmtprint(int fd, const char *str, ...);
int s_getline(sspace_t *space, int fd);
#endif
