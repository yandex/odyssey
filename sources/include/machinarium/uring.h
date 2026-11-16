#pragma once

/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

/* Check required kernel capabilities using a temporary ring. No I/O is queued.
 * Return 0 on support, otherwise -1 with errno. ENOTSUP means missing build or
 * kernel support; permission and resource errors preserve their errno.
 */
int mm_uring_probe(void);
