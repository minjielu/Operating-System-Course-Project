/*
     File        : blocking_disk.c

     Author      :
     Modified    :

     Description :

*/

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

    /* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "assert.H"
#include "utils.H"
#include "console.H"
#include "blocking_disk.H"
#include "scheduler.H"
#include "thread.H"

/*--------------------------------------------------------------------------*/
/* CONSTRUCTOR */
/*--------------------------------------------------------------------------*/

Thread * BlockingDisk::start_bqueue = NULL;

Thread * BlockingDisk::end_bqueue = NULL;

BlockingDisk * BlockingDisk::last_disk = NULL;

BlockingDisk::BlockingDisk(DISK_ID _disk_id, unsigned int _size)
  : SimpleDisk(_disk_id, _size) {
    disk_id = _disk_id;
    disk_size = _size;
    last_disk = this;
}

/* issue_operation function is just copied from the simple disk since it's a private function there */
void BlockingDisk::issue_operation(DISK_OPERATION _op, unsigned long _block_no) {

  Machine::outportb(0x1F1, 0x00); /* send NULL to port 0x1F1         */
  Machine::outportb(0x1F2, 0x01); /* send sector count to port 0X1F2 */
  Machine::outportb(0x1F3, (unsigned char)_block_no);
                         /* send low 8 bits of block number */
  Machine::outportb(0x1F4, (unsigned char)(_block_no >> 8));
                         /* send next 8 bits of block number */
  Machine::outportb(0x1F5, (unsigned char)(_block_no >> 16));
                         /* send next 8 bits of block number */
  Machine::outportb(0x1F6, ((unsigned char)(_block_no >> 24)&0x0F) | 0xE0 | (disk_id << 4));
                         /* send drive indicator, some bits,
                            highest 4 bits of block no */

  Machine::outportb(0x1F7, (_op == READ) ? 0x20 : 0x30);

}

/*--------------------------------------------------------------------------*/
/* SIMPLE_DISK FUNCTIONS */
/*--------------------------------------------------------------------------*/

void BlockingDisk::read(unsigned long _block_no, unsigned char * _buf) {
  // Issure read operation.
  issue_operation(READ, _block_no);
  // Add the current thread to the end of the blocking queue.
  Machine::disable_interrupts();
  if(!start_bqueue) {
    start_bqueue = Thread::CurrentThread();
    end_bqueue = Thread::CurrentThread();
  }
  else {
    end_bqueue -> next_bqueue = Thread::CurrentThread();
    end_bqueue = Thread::CurrentThread();
  }
  Scheduler::preempt_thread(Thread::CurrentThread());
  /* read data from port */
  int i;
  unsigned short tmpw;
  for (i = 0; i < 256; i++) {
    tmpw = Machine::inportw(0x1F0);
    _buf[i*2]   = (unsigned char)tmpw;
    _buf[i*2+1] = (unsigned char)(tmpw >> 8);
  }
  // Resume the thread.
  Scheduler::resume_from_blocking(start_bqueue);
  // Remove the thread from the blocking queue.
  if(start_bqueue == end_bqueue) {
    start_bqueue = NULL;
    end_bqueue = NULL;
  }
  else {
    Thread * tmp = start_bqueue -> next_bqueue;
    start_bqueue -> next_bqueue = NULL;
    start_bqueue = tmp;
  }
  // Yield the cpu to the next thread in the ready queue of the scheduler.
  Scheduler::yield_after_diskIO();
}


void BlockingDisk::write(unsigned long _block_no, unsigned char * _buf) {
  // Issure write operation.
  issue_operation(WRITE, _block_no);
  Machine::disable_interrupts();
  // Add the current thread to the end of the blocking queue.
  if(!start_bqueue) {
    start_bqueue = Thread::CurrentThread();
    end_bqueue = Thread::CurrentThread();
  }
  else {
    end_bqueue -> next_bqueue = Thread::CurrentThread();
    end_bqueue = Thread::CurrentThread();
  }
  Scheduler::preempt_thread(Thread::CurrentThread());
  /* write data to port */
  int i;
  unsigned short tmpw;
  for (i = 0; i < 256; i++) {
    tmpw = _buf[2*i] | (_buf[2*i+1] << 8);
    Machine::outportw(0x1F0, tmpw);
  }
  // Resume the thread.
  Scheduler::resume_from_blocking(start_bqueue);
  // Remove the thread from the blocking queue.
  if(start_bqueue == end_bqueue) {
    start_bqueue = NULL;
    end_bqueue = NULL;
  }
  else {
    Thread * tmp = start_bqueue -> next_bqueue;
    start_bqueue -> next_bqueue = NULL;
    start_bqueue = tmp;
  }
  // Yield the cpu to the next thread in the ready queue of the scheduler.
  Scheduler::yield_after_diskIO();
}

void BlockingDisk::dispatch_to_blocked() {
  Thread::dispatch_to(start_bqueue);
}

bool BlockingDisk::check_ready() {
  return last_disk -> is_ready();
}
