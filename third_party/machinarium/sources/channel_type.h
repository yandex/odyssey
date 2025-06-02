#pragma once

/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

typedef struct mm_channeltype mm_channeltype_t;

struct mm_channeltype {
	int is_shared;
} __attribute__((packed));
