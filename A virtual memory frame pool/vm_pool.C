/*
 File: vm_pool.C

 Author:
 Date  :

 */

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "vm_pool.H"
#include "console.H"
#include "utils.H"
#include "assert.H"
#include "simple_keyboard.H"
#include "page_table.H"

#define _load_is_necessary

/*--------------------------------------------------------------------------*/
/* DATA STRUCTURES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* CONSTANTS */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* FORWARDS */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* METHODS FOR CLASS   V M P o o l */
/*--------------------------------------------------------------------------*/

VMPool::VMPool(unsigned long  _base_address,
               unsigned long  _size,
               ContFramePool *_frame_pool,
               PageTable     *_page_table) {
    base_address = _base_address;
    size = _size;
    frame_pool = _frame_pool;
    page_table = _page_table;
    page_table -> register_pool(this);
    // The first 4 Bytes of the vm pool states that the first page of the pool is
    // taken for storing a list of allocated frames in this pool.
    #ifdef _load_is_necessary
      page_table -> load();
    #endif
    // The list is bidirectional list and the next point of the last list element
    // points to itself.
    unsigned int * new_base_address = (unsigned int *)_base_address;
    (* new_base_address++) = base_address | 1; // The last bit indicates this free list entry is valid.
    (* new_base_address++) = base_address + 4096;
    for(int i = 0; i < 2; i++) (* new_base_address++) = base_address;

    Console::puts("Constructed VMPool object.\n");
}

unsigned long VMPool::allocate(unsigned long _size) {
    // Allocation applies a first fit policy.
    unsigned long page_no = _size / 4096 + ((_size % 4096) == 0? 0:1);
    #ifdef _load_is_necessary
      page_table -> load();
    #endif
    unsigned long * cur = (unsigned long *) base_address;
    while(*(cur+3) != (unsigned long)cur) {
      unsigned long * next = (unsigned long *) (*(cur+3));
      unsigned long available_size = (((* next) & 0xFFFFFFFE) - *(cur+1)) / 4096;
      // Allocate spaces by inserting an entry into the allocated list.
      if(available_size >= page_no) {
        // Find the first unused entry in the info frame.
        unsigned long * info_entry = (unsigned long *) base_address;
        while(*info_entry & 1) info_entry += 4;
        *(cur+3) = (unsigned long) info_entry;
        *(next+2) = (unsigned long) info_entry;
        *(info_entry++) = (*(cur+1) + 4096) | 1;
        *(info_entry++) = *(cur+1) + page_no * 4096;
        *(info_entry++) = (unsigned long) cur;
        *info_entry = (unsigned long) next;
        Console::puts("Allocated a region of memory.\n");
        return *(cur+1) + 4096;
      }
      cur = next;
    }
    // If enough space is only availabe at the end of the list.
    unsigned long available_size = (size - (*(cur+1) - base_address)) / 4096;
    if(available_size >= page_no) {
      unsigned long * info_entry = (unsigned long *) base_address;
      while(*info_entry & 1) info_entry += 4;
      *(cur+3) = (unsigned long) info_entry;
      *(info_entry++) = (*(cur+1) + 4096) | 1;
      *(info_entry++) = *(cur+1) + page_no * 4096;
      *(info_entry++) = (unsigned long)cur;
      *info_entry = *(cur+3);
      Console::puts("Allocated a region of memory.\n");
      return *(cur+1) + 4096;
    }
    Console::puts("Failed to locate enough space.\n");
}

void VMPool::release(unsigned long _start_address) {
    // Locate the info_entry to know the size of the segment.
    #ifdef _load_is_necessary
      page_table -> load();
    #endif
    unsigned long * cur = (unsigned long *) base_address;
    while(*(cur+3) != (unsigned long)cur) {
      if((*(cur) & 0xFFFFFFFE) == _start_address) {
        unsigned long end_address = *(cur+1);
        // Free all pages
	Console::puts("Cleaning a page");
        for(;_start_address <= end_address; _start_address += 4096) {
          page_table -> free_page(_start_address / 4096);
        }
        // Remove the entry in the allocation list.
        *cur = 0;
        unsigned long * pre = (unsigned long *) (*(cur+2));
        unsigned long * next = (unsigned long *) (*(cur+3));
        *(pre + 3) = *(cur + 3);
        *(next + 2) = *(cur + 2);
        Console::puts("Released region of memory.\n");
        return;
      }
      cur = (unsigned long *) (*(cur + 3));
    }
    // The segment to be removed is at the end of the allocated list.
    if((*(cur) & 0xFFFFFFFE) == _start_address) {
      unsigned long end_address = *(cur+1);
      // Free all pages
      for(;_start_address <= end_address; _start_address += 4096) {
        page_table -> free_page(_start_address / 4096);
      }
      // Remove the entry in the allocation list.
      *cur = 0;
      unsigned long * pre = (unsigned long *) (*(cur+2));
      *(pre + 3) = (unsigned long) pre;
      Console::puts("Released region of memory.\n");
      return;
    }
    Console::puts("Region to be released is not found.\n");
}

bool VMPool::is_legitimate(unsigned long _address) {
    Console::puts("Checked whether address is part of an allocated region.\n");
    #ifdef _load_is_necessary
      page_table -> load();
    #endif
    // When we are initializing the vm pool, the first sixteen bytes should be legal,
    // Since we need it to indicate the info page for the pool is allocated.
    if((_address >= base_address) && (_address < base_address + 16)) return true;
    // Traverse the allocated list to check legality.
    unsigned long * cur = (unsigned long *) base_address;
    while(*(cur+3) != (unsigned long) cur) {
      if((_address >= (*(cur) & 0xFFFFFFFE)) && (_address <= *(cur+1) + 4095)) return true;
      unsigned long * next = (unsigned long *) (*(cur + 3));
      if((_address >= (*(cur + 1) + 4096)) && (_address < *(next) & 0xFFFFFFFE)) return false;
      cur = next;
    }
    if((_address >= (*(cur) & 0xFFFFFFFE)) && (_address <= *(cur+1) + 4095)) return true;
    return false;
}
