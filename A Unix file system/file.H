/*
     File        : file.H

     Author      : Riccardo Bettati
     Modified    : 2017/05/01

     Description : Simple File class with sequential read/write operations.

*/

#ifndef _FILE_H_
#define _FILE_H_

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* DATA STRUCTURES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* class  F i l e   */
/*--------------------------------------------------------------------------*/

class FileSystem;

class File  {

friend class FileSystem;

private:
    /* -- your file data structures here ... */
    File* next_file; // To form a list of files for a filesystem.
    int file_id;
    unsigned int info_block;   // Block number of the info block.
    unsigned int direct_blocks[7]; // A list of block numbers of data blocks. 
    unsigned int cur_byte;   // The current position of read or write operation.
    int end_byte;   // The size of the file in bytes which indicates the end of the file. 
    char buffer[512]; // Buffer of a data block in the RAM.
    FileSystem* filesystem;
    /* -- maybe it would be good to have a reference to the file system? */

public:

    File(int _file_id, File * _next_file, unsigned int _info_block, FileSystem * _filesystem);
    /* Constructor for the file handle. Set the ’current
     position’ to be at the beginning of the file. */

    int Read(unsigned int _n, char * _buf);
    /* Read _n characters from the file starting at the current location and
     copy them in _buf.  Return the number of characters read.
     Do not read beyond the end of the file. */

    void Write(unsigned int _n, const char * _buf);
    /* Write _n characters to the file starting at the current location,
     if we run past the end of file,
     we increase the size of the file as needed. */

    void Reset();
    /* Set the ’current position’ at the beginning of the file. */

    void Rewrite();
    /* Erase the content of the file. Return any freed blocks.
     Note: This function does not delete the file! It just erases its content. */

    bool EoF();
    /* Is the current location for the file at the end of the file? */

};

#endif
