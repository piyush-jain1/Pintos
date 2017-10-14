#include "vm/frame.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vm/swap.h"
#include "vm/page.h"
#include "userprog/pagedir.h"
#include "bitmap.h"
#include "threads/palloc.h"
#include "threads/loader.h"

/* We need to initialize our frame table when the first thread is created?! (kernel)?
	We want to use FIFO for page replacement algorithm!!!!!
   "To simplify your design, you may store these data structures in non-pageable memory.
   Non-pageable memory should not be evicted from the frame table. That means that you 
   can be sure that pointers among them will remain valid." -via documentation 4.1.3
   We think we need to malloc() at PHYS_BASE to store the frame table, page table, and swap table...
*/

//Needed global variables
struct bitmap * frame_bitmap;
struct frame * frame_table;
int frame_table_index;
int num_pages = 383;
bool frame_table_initialized = false;

//Included from loader.h
uint32_t init_ram_pages;


//Prototype definitions
void init_frame(struct frame f);

void 
init_frame_table(void){
	int i;
	//Initializes the frame table by allocating [num_pages] chunks of data of size [frame *].
	//This gives us, effectively, an array.

	if (!frame_table_initialized)
	{
		frame_table = (struct frame *) malloc( sizeof(struct frame) * num_pages );
		i = 0;
		int counter = 0;
		for(;i<num_pages;++i)
		{
			// frame_table[i] = (struct frame *) malloc(sizeof(struct frame));
			init_frame(frame_table[i]);
			frame_table[i].kpage = palloc_get_page(PAL_USER | PAL_ZERO);
		}

		//Initializes the bitmap of used frames, sets them all to 0 initially.
		frame_bitmap = bitmap_create(num_pages);
		bitmap_set_all(frame_bitmap, 0);
		frame_table_index = 0;
		frame_table_initialized = true;
	}
}

/*Get an available frame from the user pool. If no frames are available,
	it then evicts according to the eviction policy (FIFO), then returns 
	the newly open frame. */
struct frame *
get_frame(void)
{
	int index;
	//this if statement checks for there to be no open frame
	if (!bitmap_contains(frame_bitmap, 0, num_pages, 0))
	{
		//Now evicts a frame
		evict_frame();
		return &frame_table[frame_table_index - 1];
	}

	//Returns the open adress
	index = 0;
	for (;index<num_pages;++index)
	{
		if (!bitmap_test(frame_bitmap, index))
		{
			bitmap_mark(frame_bitmap, index);
			return &frame_table[index];
		}
	}

	//Should never get here.mak
}

/* Evicts a frame according to our frame eviction algorithm.
	We're using FIFO. */
void 
evict_frame(void)
{
	//erase the frame_table[fifo_evict_count]
	clean_frame();
	bitmap_set (frame_bitmap, frame_table_index, 0); // sets the bitmap to 0
	frame_table_index = (frame_table_index + 1) % num_pages;
}

/* Cleans a frame's info, setting it all equal to NULL (used for eviction) */
void 
clean_frame(void)
{
	struct frame f = frame_table[frame_table_index];
	f.upage = NULL;
	f.owner_thread = NULL;
}

/* Sets all values of a new frame to NULL*/
void
init_frame(struct frame f)
{
	f.kpage = NULL;
	f.upage = NULL;
	f.owner_thread = NULL;
}

