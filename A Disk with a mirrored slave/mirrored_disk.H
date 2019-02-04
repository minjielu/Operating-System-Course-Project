/*
     File        : blocking_disk.H

     Author      :

     Date        :
     Description :

*/

#ifndef _BLOCKING_DISK_H_
#define _BLOCKING_DISK_H_

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "simple_disk.H"
#include "thread.H"

/*--------------------------------------------------------------------------*/
/* DATA STRUCTURES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* B l o c k i n g D i s k  */
/*--------------------------------------------------------------------------*/

class MirroredDisk : public SimpleDisk {
private:
  /* Copy private members from the SimpleDisk since they can't be invoked by the inheriting class */
  DISK_ID disk_id;  /* This disk is either MASTER or SLAVE */

  unsigned int disk_size;

  void issue_operation(DISK_OPERATION _op, unsigned long _block_no);
  /* Send a sequence of commands to the controller to initialize the READ/WRITE
     operation. This operation is called by read() and write(). */

public:


  static Thread * start_master_bqueue; // Start of the blocking queue;

  static Thread * end_master_bqueue;  // End of the blocking queue;

  static Thread * start_slave_bqueue;

  static Thread * end_slave_bqueue;

  static MirroredDisk * last_disk; // Pointer to the disk object;


   MirroredDisk(DISK_ID _disk_id, unsigned int _size);
   /* Creates a MirroredDisk device with the given size connected to the
      MASTER or SLAVE slot of the primary ATA controller.
      NOTE: We are passing the _size argument out of laziness.
      In a real system, we would infer this information from the
      disk controller. */

   /* DISK OPERATIONS */

   virtual void read(unsigned long _block_no, unsigned char * _buf);
   /* Reads 512 Bytes from the given block of the disk and copies them
      to the given buffer. No error check! */

   virtual void write(unsigned long _block_no, unsigned char * _buf);
   /* Writes 512 Bytes from the buffer to the given block on the disk. */

   static void dispatch_to_master_blocked();
   static void dispatch_to_slave_blocked();
   /* If a disk device is ready, dispatch to the blocked thread that is waiting
      for disk */

   static bool check_master_ready();

   static bool check_slave_ready();

   static void pop_master_queue();
   /* pop the front thread in the master queue */

   static void pop_slave_queue();
   /* pop the front thread in the slave queue */

};

#endif
