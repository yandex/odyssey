#ifndef MACHINARIUM_PRIVATE_H_
#define MACHINARIUM_PRIVATE_H_

/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

#define _GNU_SOURCE 1

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <uv.h>
#include "tls.h"

#include "mm_macro.h"
#include "mm_list.h"
#include "mm_buf.h"
#include "mm_context.h"
#include "mm_call.h"
#include "mm_fiber.h"
#include "mm_scheduler.h"
#include "mm.h"
#include "mm_tls.h"
#include "mm_io.h"
#include "mm_read.h"

#endif
