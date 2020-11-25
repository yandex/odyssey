#ifndef MACHINARIUM_PRIVATE_H_
#define MACHINARIUM_PRIVATE_H_

/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#define _GNU_SOURCE 1

#ifndef IOV_MAX
#	define IOV_MAX __IOV_MAX
#endif

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/mman.h>
#include <sys/signalfd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#include <openssl/bio.h>
#include <openssl/conf.h>
#include <openssl/crypto.h>
#include <openssl/engine.h>
#include <openssl/err.h>
#include <openssl/opensslv.h>
#include <openssl/ssl.h>

#include "buf.h"
#include "build.h"
#include "list.h"
#include "macro.h"
#include "sleep_lock.h"
#include "util.h"

#include "bind.h"
#include "clock.h"
#include "epoll.h"
#include "fd.h"
#include "idle.h"
#include "loop.h"
#include "poll.h"
#include "socket.h"
#include "timer.h"

#include "call.h"
#include "context.h"
#include "context_stack.h"
#include "coroutine.h"
#include "coroutine_cache.h"
#include "scheduler.h"

#include "signal_mgr.h"
#include "thread.h"

#include "cond.h"
#include "event.h"
#include "event_mgr.h"

#include "channel.h"
#include "channel_fast.h"
#include "channel_type.h"
#include "msg.h"
#include "msg_cache.h"

#include "task.h"
#include "task_mgr.h"

#include "machine.h"
#include "machine_mgr.h"
#include "mm.h"

#include "compression.h"
#include "io.h"
#include "iov.h"
#include "tls.h"
#include "zpq_stream.h"

#include "lrand48.h"

#endif
