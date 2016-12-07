#ifndef MM_PRIVATE_H_
#define MM_PRIVATE_H_

/*
 * machinarium.
 *
 * Cooperative multitasking engine.
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
#include <ucontext.h>

#include <uv.h>

#include "mm_macro.h"
#include "mm_list.h"
#include "mm_buf.h"
#include "mm_context.h"
#include "mm_fiber.h"
#include "mm_scheduler.h"
#include "mm.h"
#include "mm_io.h"
#include "mm_read.h"

#endif
