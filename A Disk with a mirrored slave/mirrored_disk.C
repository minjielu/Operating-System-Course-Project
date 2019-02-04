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
#include "mirrored_disk.H"
#include "scheduler.H"
#include "thread.H"
#include "machine.H"

/*--------------------------------------------------------------------------*/
/* CONSTRUCTOR */
/*--------------------------------------------------------------------------*/

Thread * MirroredDisk::start_master_bqueue = NULL;

Thread * MirroredDisk::end_master_bqueue = NULL;

Thread * MirroredDisk::start_slave_bqueue = NULL;

Thread * MirroredDisk::end_slave_bqueue = NULL;

MirroredDisk * MirroredDisk::last_disk = NULL;

MirroredDisk::MirroredDisk(DISK_ID _disk_id, unsigned int _size)
  : SimpleDisk(_disk_id, _size) {
    disk_id = _disk_id;
    disk_size = _size;
    last_disk = this;
}

/* issue_operation function is just copied from the simple disk since it's a private function there */
void MirroredDisk::issue_operation(DISK_OPERATION _op, unsigned long _block_no) {
  /* Issue operations to the master disk. */
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

  /* Issue operations to the slave disk. */
  Machine::outportb(0x3F1, 0x00); /* send NULL to port 0x1F1         */
  Machine::outportb(0x3F2, 0x01); /* send sector count to port 0X1F2 */
  Machine::outportb(0x3F3, (unsigned char)_block_no);
                         /* send low 8 bits of block number */
  Machine::outportb(0x3F4, (unsigned char)(_block_no >> 8));
                         /* send next 8 bits of block number */
  Machine::outportb(0x3F5, (unsigned char)(_block_no >> 16));
                         /* send next 8 bits of block number */
  Machine::outportb(0x3F6, ((unsigned char)(_block_no >> 24)&0x0F) | 0xE0 | (disk_id << 4));
                         /* send drive indicator, some bits,
                            highest 4 bits of block no */

  Machine::outportb(0x3F7, (_op == READ) ? 0x20 : 0x30);

}

/*--------------------------------------------------------------------------*/
/* SIMPLE_DISK FUNCTIONS */
/*--------------------------------------------------------------------------*/

void MirroredDisk::read(unsigned long _block_no, unsigned char * _buf) {
  // Issure read operation.
  issue_operation(READ, _block_no);
  // Add the current thread to the end of the master blocking queue.
  Machine::disable_interrupts();
  if(!start_master_bqueue) {
    start_master_bqueue = Thread::CurrentThread();
    end_master_bqueue = Thread::CurrentThread();
  }
  else {
    end_master_bqueue -> next_master_bqueue = Thread::CurrentThread();
    end_master_bqueue = Thread::CurrentThread();
  }
  if(!start_slave_bqueue) {
    start_slave_bqueue = Thread::CurrentThread();
    end_slave_bqueue = Thread::CurrentThread();
  }
  else {
    end_slave_bqueue -> next_slave_bqueue = Thread::CurrentThread();
    end_slave_bqueue = Thread::CurrentThread();
  }
  Thread::CurrentThread() -> waiting_for_diskIO = true;
  Scheduler::preempt_thread(Thread::CurrentThread());
  /* read data from port */
  Thread::CurrentThread() -> waiting_for_diskIO = false;
  int i;
  unsigned short tmpw;
  for (i = 0; i < 256; i++) {
    tmpw = Machine::inportw(0x1F0);
    _buf[i*2]   = (unsigned char)tmpw;
    _buf[i*2+1] = (unsigned char)(tmpw >> 8);
  }
  // Resume the thread.
  Scheduler::resume_from_blocking(Thread::CurrentThread());
  // Yield the cpu to the next thread in the ready queue of the scheduler.
  Scheduler::yield_after_diskIO();
}


void MirroredDisk::write(unsigned long _block_no, unsigned char * _buf) {
  // Issure write operation.
  issue_operation(WRITE, _block_no);
  Machine::disable_interrupts();
  // Add the current thread to the end of the blocking queue.
  if(!start_master_bqueue) {
    start_master_bqueue = Thread::CurrentThread();
    end_master_bqueue = Thread::CurrentThread();
  }
  else {
    end_master_bqueue -> next_master_bqueue = Thread::CurrentThread();
    end_master_bqueue = Thread::CurrentThread();
  }
  if(!start_slave_bqueue) {
    start_slave_bqueue = Thread::CurrentThread();
    end_slave_bqueue = Thread::CurrentThread();
  }
  else {
    end_slave_bqueue -> next_slave_bqueue = Thread::CurrentThread();
    end_slave_bqueue = Thread::CurrentThread();
  }
  Thread::CurrentThread() -> waiting_for_diskIO = true;
  Scheduler::preempt_thread(Thread::CurrentThread());
  // Allow write to be finished for two disks.
  Scheduler::preempt_thread(Thread::CurrentThread());
  Thread::CurrentThread() -> waiting_for_diskIO = false;
  /* write data to port */
  int i;
  unsigned short tmpw;
  for (i = 0; i < 256; i++) {
    tmpw = _buf[2*i] | (_buf[2*i+1] << 8);
    Machine::outportw(0x1F0, tmpw);
  }
  // Resume the thread.
  Scheduler::resume_from_blocking(Thread::CurrentThread());
  // Yield the cpu to the next thread in the ready queue of the scheduler.
  Scheduler::yield_after_diskIO();
}


void MirroredDisk::dispatch_to_master_blocked() {
  Thread * tmp = last_disk -> start_master_bqueue;
  // Remove the thread from the master blocking queue.
  if(last_disk -> start_master_bqueue == last_disk -> end_master_bqueue) {
    last_disk -> start_master_bqueue = NULL;
    last_disk -> end_master_bqueue = NULL;
  }
  else {
    Thread * tmp1 = last_disk -> start_master_bqueue -> next_master_bqueue;
    last_disk -> start_master_bqueue -> next_master_bqueue = NULL;
    last_disk -> start_master_bqueue = tmp1;
  }
  if(tmp -> waiting_for_diskIO) Thread::dispatch_to(tmp);
}

void MirroredDisk::dispatch_to_slave_blocked() {
  Thread * tmp = last_disk -> start_slave_bqueue;
  // Remove the thread from the master blocking queue.
  if(last_disk -> start_slave_bqueue == last_disk -> end_slave_bqueue) {
    last_disk -> start_slave_bqueue = NULL;
    last_disk -> end_slave_bqueue = NULL;
  }
  else {
    Thread * tmp1 = last_disk -> start_slave_bqueue -> next_slave_bqueue;
    last_disk -> start_slave_bqueue -> next_slave_bqueue = NULL;
    last_disk -> start_slave_bqueue = tmp1;
  }
  if(tmp -> waiting_for_diskIO) Thread::dispatch_to(tmp);
}

bool MirroredDisk::check_master_ready() {
   return ((Machine::inportb(0x1F7) & 0x08) != 0);
}

bool MirroredDisk::check_slave_ready() {
   return ((Machine::inportb(0x1F7) & 0x08) != 0);
}
