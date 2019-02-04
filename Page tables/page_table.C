#include "assert.H"
#include "exceptions.H"
#include "console.H"
#include "paging_low.H"
#include "page_table.H"

PageTable * PageTable::current_page_table = NULL;
unsigned int PageTable::paging_enabled = 0;
ContFramePool * PageTable::kernel_mem_pool = NULL;
ContFramePool * PageTable::process_mem_pool = NULL;
unsigned long PageTable::shared_size = 0;



void PageTable::init_paging(ContFramePool * _kernel_mem_pool,
                            ContFramePool * _process_mem_pool,
                            const unsigned long _shared_size)
{
   // Set the global parameters for the paging subsystem.
   kernel_mem_pool = _kernel_mem_pool;
   process_mem_pool = _process_mem_pool;
   shared_size = _shared_size;
   Console::puts("Initialized Paging System\n");
}

PageTable::PageTable()
{
   // Get a frame from kernel pool for the page page_directory.
   page_directory = (unsigned long *) ((kernel_mem_pool -> get_frames(1)) * 4096);
   // One page table page is enough for the 4MB direct mapped memory.
   page_directory[0] = (kernel_mem_pool -> get_frames(1)) * 4096;
   unsigned long * page_table_add = (unsigned long *) page_directory[0];
   page_directory[0] = page_directory[0] | 1;
   for(int i = 1; i < 1024; i++) {
     page_directory[i] = page_directory[i] & 0xFFFFFFFE;
   }
   // Intialize page table entries for the 4MB direct mapped memory.
   // We need 1K entries to store page numbers from 0 to 0x300.
   for(int i = 0; i < 1024; i++) {
     unsigned long entry = (i << 12) | 1;
     page_table_add[i] = entry;
   }

   Console::puts("Constructed Page Table object\n");
}


void PageTable::load()
{
   // Load a page_directory by loading its address to the CR3 register.
   write_cr3((unsigned long) page_directory);
   // Set the current_page_table as the currently loaded page table object.
   current_page_table = this;
   Console::puts("Loaded page table\n");
}

void PageTable::enable_paging()
{
   // Enable_paging by setting the paging_enabled to 1 and write it to the CR0 register.
   paging_enabled = 1;
   unsigned long new_cr0 = read_cr0() | 0x80000000;
   write_cr0(new_cr0);
   Console::puts("Enabled paging\n");
}

void PageTable::handle_fault(REGS * _r)
{
  // In this machine problem, we only consider cases of page not present.
  unsigned long fault_add = read_cr2();
  // The case of a missing page table.
  unsigned long * current_page_directory = current_page_table -> page_directory;
  if(current_page_directory[fault_add >> 22] & 1) {
    // The case of simply missing a page table entry.
    unsigned long * current_page_table = (unsigned long *) (current_page_directory[fault_add >> 22] & 0xFFFFF000);
    // get a new page from the process pool.
    current_page_table[(fault_add >> 12) & 0x3FF] = (process_mem_pool -> get_frames(1)) * 4096;
    current_page_table[(fault_add >> 12) & 0x3FF] = (current_page_table[(fault_add >> 12) & 0x3FF] & 0xFFFFF000) | 1;
  }
  else
  {
    // get a new frame from the kernel pool for a new page table.
    current_page_directory[fault_add >> 22] = (kernel_mem_pool -> get_frames(1)) * 4096;
    unsigned long * current_page_table = (unsigned long *) current_page_directory[fault_add >> 22];
    current_page_directory[fault_add >> 22] = (current_page_directory[fault_add >> 22]) | 1;
    // get a new page from the process pool.
    current_page_table[(fault_add >> 12) & 0x3FF] = (process_mem_pool -> get_frames(1)) * 4096;
    current_page_table[(fault_add >> 12) & 0x3FF] = (current_page_table[(fault_add >> 12)  & 0x3FF] & 0xFFFFF000) | 1;
    Console::puts("New page table created\n");
  }
  Console::puts("handled page fault\n");
}
