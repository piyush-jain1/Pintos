#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <stdint.h>

struct frame{

	//store the physical address in the appropriate data type
	uint8_t *kpage; // attained via palloc_get_page(PAL_USER)
	//we might want to store the virtual address as well
	uint32_t *upage;
	struct thread * owner_thread;
	//----Needs a page table entry this frame corresponds to----
};

void init_frame_table(void);
struct frame * get_frame(void);
void evict_frame(void);
void clean_frame(void);
void init_frame(struct frame f);

#endif /* vm/frame.h */

