#ifndef OD_LOG_FILE_H
#define OD_LOG_FILE_H

/*
 * Odissey.
 *
 * Advanced PostgreSQL connection pooler.
*/

typedef struct od_logfile od_logfile_t;

struct od_logfile
{
	int fd;
};

void od_logfile_init(od_logfile_t*);
int  od_logfile_open(od_logfile_t*, char*);
void od_logfile_close(od_logfile_t*);
int  od_logfile_write(od_logfile_t*, char*, int);

#endif
