/*
     File        : file.C

     Author      : Riccardo Bettati
     Modified    : 2017/05/01

     Description : Implementation of simple File class, with support for
                   sequential read/write operations.
*/

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

    /* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "assert.H"
#include "console.H"
#include "file.H"
#include "file_system.H"
#include "simple_disk.H"

/*--------------------------------------------------------------------------*/
/* CONSTRUCTOR */
/*--------------------------------------------------------------------------*/

File::File(int _file_id, File * _next_file, unsigned int _info_block, FileSystem * _filesystem) {
    /* We will need some arguments for the constructor, maybe pointer to disk
     block with file management and allocation data. */
    Console::puts("In file constructor.\n");
    file_id = _file_id;
    next_file = _next_file;
    info_block = _info_block;
    cur_byte = 0;
    end_byte = -1;
    filesystem = _filesystem;
}

/*--------------------------------------------------------------------------*/
/* FILE FUNCTIONS */
/*--------------------------------------------------------------------------*/

int File::Read(unsigned int _n, char * _buf) {
    Console::puts("reading from file\n");
    int read_ptr = 0;
    /* Read multiples of data blocks*/
    while(((((cur_byte / 512) + 1) * 512) <= cur_byte + _n) && ((((cur_byte / 512) + 1) * 512) <= end_byte + 1)) {
      /* Read in the next block from the disk if necessary. */
      if(cur_byte % 512 == 0) {
        filesystem -> disk -> read((unsigned long)(direct_blocks[cur_byte / 512]), (unsigned char *) buffer); // Blocks for files start from the second block.
      }
      _buf[read_ptr++] = buffer[(cur_byte++ % 512)];
      _n--;
      while((cur_byte % 512) != 0) {
        _buf[read_ptr++] = buffer[(cur_byte++ % 512)];
        _n--;
      }
    }
    /* Read remaining bytes */
    if((cur_byte % 512 == 0) && (_n > 0) && (cur_byte <= end_byte)) {
      filesystem -> disk -> read((unsigned long)(direct_blocks[cur_byte / 512]), (unsigned char *) buffer); // Blocks for files start from the second block.
    }
    while((_n > 0) && (cur_byte <= end_byte)) {
      _buf[read_ptr++] = buffer[(cur_byte++ % 512)];
      _n--;
    }
    return read_ptr;
}


void File::Write(unsigned int _n, const char * _buf) {
    Console::puts("writing to file\n");
    int write_ptr = 0;
    /* Write multiples of entire blocks */
    while((((cur_byte / 512) + 1) * 512) <= cur_byte + _n) {
      buffer[(cur_byte++ % 512)] = _buf[write_ptr++];
      _n--;
      while((cur_byte % 512) != 0) {
        buffer[(cur_byte++ % 512)] = _buf[write_ptr++];
        _n--;
      }
      if(cur_byte > ((end_byte == -1)? 0:(((end_byte / 512) + 1) * 512))) {
        unsigned int new_block = filesystem -> free_block();
        direct_blocks[cur_byte / 512 - 1] = new_block;
	// Write the linked list of data blocks to disk.
	filesystem -> disk -> write((unsigned long)(info_block), (unsigned char *) direct_blocks);
        filesystem -> disk -> write((unsigned long)(new_block), (unsigned char *) buffer);
      }
      else {
        filesystem -> disk -> write((unsigned long)(direct_blocks[cur_byte / 512 - 1]), (unsigned char *) buffer);
      }
    }
    /* Write remaining bytes */
    if(_n > 0)
    {
	    while(_n > 0) {
	      buffer[(cur_byte++ % 512)] = _buf[write_ptr++];
	      _n--;
	    }
	    /* Padding the unwritted byte with 0 */
	    for(int i = cur_byte % 512; i < 512; i++) buffer[i] = (char)0;
	    if(cur_byte > ((end_byte == -1)? 0:(((end_byte / 512) + 1) * 512))) {
	      unsigned int new_block = filesystem -> free_block();
	      direct_blocks[cur_byte / 512] = new_block;
	      // Write the linked list of data blocks to disk.
	      filesystem -> disk -> write((unsigned long)(info_block), (unsigned char *) direct_blocks);
	      filesystem -> disk -> write((unsigned long)(new_block), (unsigned char *) buffer);
	    }
	    else {
	      filesystem -> disk -> write((unsigned long)(direct_blocks[cur_byte / 512]), (unsigned char *) buffer);
	    }
    }
    end_byte = cur_byte - 1;
}

void File::Reset() {
    Console::puts("reset current position in file\n");
    cur_byte = 0;
}

void File::Rewrite() {
    Console::puts("erase content of file\n");
    filesystem -> free_blocks(this); // Free used data blocks by this file.
    cur_byte = 0;
    end_byte = -1;
}


bool File::EoF() {
    Console::puts("testing end-of-file condition\n");
    if(cur_byte >= end_byte) return true;
    return false;
}
