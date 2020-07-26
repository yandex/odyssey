#ifndef OD_SIGNAL_HANDLER
#define OD_SIGNAL_HANDLER

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <errno.h>

#include <machinarium.h>
#include <kiwi.h>
#include <odyssey.h>

void
od_system_signal_handler(void *arg);

#endif /* OD_SIGNAL_HANDLER */
