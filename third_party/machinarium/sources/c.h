
#pragma once

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include <stdatomic.h>

#include <limits.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>

#include <netinet/in.h>
#include <netinet/tcp.h>

#include <netdb.h>
#include <errno.h>

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
#define OD_THREAD_LOCAL thread_local
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define OD_THREAD_LOCAL _Thread_local
#elif defined(_MSC_VER)
#define OD_THREAD_LOCAL __declspec(thread)
#else
#define OD_THREAD_LOCAL __thread
#endif
