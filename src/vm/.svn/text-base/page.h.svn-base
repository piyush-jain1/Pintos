#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <inttypes.h>
#include "filesys/off_t.h"
#include "lib/kernel/hash.h"
#include "threads/thread.h"

struct supp_page{

	struct file *file;
	off_t ofs;
	uint8_t *upage; // virtual address
	uint8_t *kpage; // physical address
	size_t page_read_bytes;
	size_t page_zero_bytes;
	bool writable;

	struct hash_elem elem; // used for hash table 
};

void init_supp_table(struct thread*);
void create_supp_page(struct supp_page*, struct file*, off_t, uint8_t*, size_t, size_t, bool);
void set_supp_page_kpage(struct supp_page*, uint8_t *);
unsigned page_hash (const struct hash_elem, void*);
bool page_less (const struct hash_elem, const struct hash_elem, void*);


#endif /* vm/page.h */

