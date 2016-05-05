#ifndef COMMON_H
#define COMMON_H
#define NUM_BUFF 32
#define CHUNK 64
#define fmtprintout(str...) fmtprint(STDOUT, str)
void fmtprint(int fd, const char *str, ...);
int s_getline(sspace_t **spaceptr, int fd);
#endif
