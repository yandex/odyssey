#ifndef MM_H_
#define MM_H_

/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

void mm_attach(mm_machine_t*);
void mm_detach(mm_machine_t*);
int  mm_detach_by_id(int, mm_machine_t**);

#endif
