/*
     File        : file_system.C

     Author      : Riccardo Bettati
     Modified    : 2017/05/01

     Description : Implementation of simple File System class.
                   Has support for numerical file identifiers.
 */

/*--------------------------------------------------------------------------*/
/* DEFINES */
#define KB * (0x1 << 10)
/*--------------------------------------------------------------------------*/

    /* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "assert.H"
#include "console.H"
#include "file_system.H"
#include "simple_disk.H"


/*--------------------------------------------------------------------------*/
/* CONSTRUCTOR */
/*--------------------------------------------------------------------------*/

FileSystem::FileSystem() {
    Console::puts("In file system constructor.\n");
    last_file = NULL;
}

/*--------------------------------------------------------------------------*/
/* FILE SYSTEM FUNCTIONS */
/*--------------------------------------------------------------------------*/

bool FileSystem::Mount(SimpleDisk * _disk) {
    Console::puts("mounting file system form disk\n");
    /* Initialize the bitmap according to the file system size */
    unsigned int no_block = _disk -> fs_size * 2 / (1 KB) / 32;
    bitmap = new unsigned int [no_block]; // One block is 0.5 KB in the SIMPLE_DISK
    bitmap[0] = 1; // The first block is used for bitmap.
    for(int i = 1; i < no_block; i++) bitmap[i] = 0;
    size = _disk -> fs_size;
    disk = _disk;
    last_file = NULL;
    _disk -> filesystem = this;
    return true;
}

bool FileSystem::Format(SimpleDisk * _disk, unsigned int _size) {
    Console::puts("formatting disk\n");
    FileSystem * old_fs = _disk -> filesystem;
    /* Delete all files belongs to the old file system */
    if(old_fs != NULL) {
      File * cur = old_fs -> last_file;
      while(cur) {
        File * tmp = cur;
        cur = cur -> next_file;
        delete tmp;
      }
      _disk -> filesystem = NULL;
      delete old_fs;
    }
    _disk -> fs_size = _size;
    return true;
}

File * FileSystem::LookupFile(int _file_id) {
    Console::puts("looking up file\n");
    File * cur = last_file;
    while(cur != NULL) {
      if(cur -> file_id == _file_id) return cur;
      cur = cur -> next_file;
    }
    return NULL;
}

bool FileSystem::CreateFile(int _file_id) {
    Console::puts("creating file\n");
    /* Check whether the file already exist */
    File * cur = last_file;
    while(cur != NULL) {
      if(cur -> file_id == _file_id) return false;
      cur = cur -> next_file;
    }
    cur = last_file;
    unsigned int info_block = free_block(); // Locate an info block for the file.
    File * new_file = new File (_file_id, last_file, info_block, this);
    last_file = new_file;
    return true;
}

bool FileSystem::DeleteFile(int _file_id) {
    Console::puts("deleting file\n");
    File * cur = last_file;
    while(cur && cur -> file_id != _file_id) cur = cur -> next_file;
    if(!cur) return false; // Return false if the file doesn't exist.
    else {
      free_blocks(cur);
    }
    /* Free the info block*/
    unsigned int block_no = cur -> info_block;
    bitmap[block_no / 32] = bitmap[block_no / 32] & (~ (1 << block_no % 32));    
    /* Remove the file from the list of the filesystem */
    File * pre = last_file;
    if(cur == pre) {
      last_file = cur -> next_file;
      delete cur;
    }
    else {
      while(pre -> next_file != cur) pre = pre -> next_file;
      pre -> next_file = cur -> next_file;
      delete cur;
    }
    return true;
}

unsigned int FileSystem::free_block() {
  int i = 0;
  while(!(~bitmap[i])) i++;
  int j = 0;	
  while((bitmap[i] & (1 << j))) {
    j++;
  }
  bitmap[i] |= (1 << j); // Mark the block as used.
  disk -> write(0, (unsigned char *) bitmap); // Write the bitmap to the disk.
  return j + i * 32;
}

void FileSystem::free_blocks(File * cur) {
  /* Release all data blocks located to the file and update the bitmap */
  unsigned int * block_list = cur -> direct_blocks;
  if(cur -> end_byte == -1) return; 
  for(int i = 0; i <= cur -> end_byte / 512; i++) {
    unsigned int block_no = *block_list;
    block_list++;
    bitmap[block_no / 32] = bitmap[block_no / 32] & (~ (1 << block_no % 32));
  }
  disk -> write(0, (unsigned char *) bitmap); // Write the bitmap to the disk.
}
