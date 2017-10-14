#include "vm/page.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vm/frame.h"
#include "vm/swap.h"
#include "userprog/pagedir.h"
#include "lib/kernel/hash.h"
#include <inttypes.h>
#include "filesys/file.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "filesys/off_t.h"

/* Wrapper functions to  access the supplementary page table */ 
void  
init_supp_table(struct thread * t)
{	
	hash_init(&t->supp_table, page_hash, page_less, NULL);
}

void 
create_supp_page(struct supp_page * spage, struct file *file, off_t ofs, uint8_t *upage, size_t page_read_bytes,
						 size_t page_zero_bytes, bool writable)
{
	spage->file = file;
	spage->ofs = ofs;
	spage->upage = upage;
	spage->page_read_bytes = page_read_bytes;
	spage->page_zero_bytes = page_zero_bytes;
	spage->writable = writable;

	struct thread * t = thread_current();
	struct hash * hash_table = &t->supp_table;

	//Insert into hash table 
	hash_insert (hash_table, &spage->elem);
}

void 
set_supp_page_kpage(struct supp_page * spage, uint8_t *kpage){
	spage->kpage = kpage;
}

unsigned
page_hash (const struct hash_elem p_, void *aux UNUSED)
{
  const struct supp_page *spage = hash_entry(&p_, struct supp_page, elem);
  return hash_bytes (&spage->upage, sizeof spage->upage);
}

bool
page_less (const struct hash_elem a_, const struct hash_elem b_, void *aux UNUSED)
{
  const struct supp_page *a = hash_entry (&a_, struct supp_page, elem);
  const struct supp_page *b = hash_entry (&b_, struct supp_page, elem);
  return a->upage < b->upage;
}

