/*
 File: ContFramePool.C

 Author:
 Date  :

 */

/*--------------------------------------------------------------------------*/
/*
 POSSIBLE IMPLEMENTATION
 -----------------------

 The class SimpleFramePool in file "simple_frame_pool.H/C" describes an
 incomplete vanilla implementation of a frame pool that allocates
 *single* frames at a time. Because it does allocate one frame at a time,
 it does not guarantee that a sequence of frames is allocated contiguously.
 This can cause problems.

 The class ContFramePool has the ability to allocate either single frames,
 or sequences of contiguous frames. This affects how we manage the
 free frames. In SimpleFramePool it is sufficient to maintain the free
 frames.
 In ContFramePool we need to maintain free *sequences* of frames.

 This can be done in many ways, ranging from extensions to bitmaps to
 free-lists of frames etc.

 IMPLEMENTATION:

 One simple way to manage sequences of free frames is to add a minor
 extension to the bitmap idea of SimpleFramePool: Instead of maintaining
 whether a frame is FREE or ALLOCATED, which requires one bit per frame,
 we maintain whether the frame is FREE, or ALLOCATED, or HEAD-OF-SEQUENCE.
 The meaning of FREE is the same as in SimpleFramePool.
 If a frame is marked as HEAD-OF-SEQUENCE, this means that it is allocated
 and that it is the first such frame in a sequence of frames. Allocated
 frames that are not first in a sequence are marked as ALLOCATED.

 NOTE: If we use this scheme to allocate only single frames, then all
 frames are marked as either FREE or HEAD-OF-SEQUENCE.

 NOTE: In SimpleFramePool we needed only one bit to store the state of
 each frame. Now we need two bits. In a first implementation you can choose
 to use one char per frame. This will allow you to check for a given status
 without having to do bit manipulations. Once you get this to work,
 revisit the implementation and change it to using two bits. You will get
 an efficiency penalty if you use one char (i.e., 8 bits) per frame when
 two bits do the trick.

 DETAILED IMPLEMENTATION:

 How can we use the HEAD-OF-SEQUENCE state to implement a contiguous
 allocator? Let's look at the individual functions:

 Constructor: Initialize all frames to FREE, except for any frames that you
 need for the management of the frame pool, if any.

 get_frames(_n_frames): Traverse the "bitmap" of states and look for a
 sequence of at least _n_frames entries that are FREE. If you find one,
 mark the first one as HEAD-OF-SEQUENCE and the remaining _n_frames-1 as
 ALLOCATED.

 release_frames(_first_frame_no): Check whether the first frame is marked as
 HEAD-OF-SEQUENCE. If not, something went wrong. If it is, mark it as FREE.
 Traverse the subsequent frames until you reach one that is FREE or
 HEAD-OF-SEQUENCE. Until then, mark the frames that you traverse as FREE.

 mark_inaccessible(_base_frame_no, _n_frames): This is no different than
 get_frames, without having to search for the free sequence. You tell the
 allocator exactly which frame to mark as HEAD-OF-SEQUENCE and how many
 frames after that to mark as ALLOCATED.

 needed_info_frames(_n_frames): This depends on how many bits you need
 to store the state of each frame. If you use a char to represent the state
 of a frame, then you need one info frame for each FRAME_SIZE frames.

 A WORD ABOUT RELEASE_FRAMES():

 When we releae a frame, we only know its frame number. At the time
 of a frame's release, we don't know necessarily which pool it came
 from. Therefore, the function "release_frame" is static, i.e.,
 not associated with a particular frame pool.

 This problem is related to the lack of a so-called "placement delete" in
 C++. For a discussion of this see Stroustrup's FAQ:
 http://www.stroustrup.com/bs_faq2.html#placement-delete

 */
/*--------------------------------------------------------------------------*/


/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "cont_frame_pool.H"
#include "console.H"
#include "utils.H"
#include "assert.H"

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
/* METHODS FOR CLASS   C o n t F r a m e P o o l */
/*--------------------------------------------------------------------------*/
ContFramePool * ContFramePool::cur = NULL;

ContFramePool::ContFramePool(unsigned long _base_frame_no,
                             unsigned long _n_frames,
                             unsigned long _info_frame_no,
                             unsigned long _n_info_frames)
{
    base_frame_no = _base_frame_no;
    nframes = _n_frames;
    nFreeFrames = _n_frames;
    info_frame_no = _info_frame_no;
    if(_n_info_frames == 0) {
    	_n_info_frames = ContFramePool::needed_info_frames(_n_frames);
    }

    // If _info_frame_no is zero then we keep management info in the first
    //frame, else we use the provided frame to keep management info
    if(info_frame_no == 0) {
        bitmap = (unsigned char *) (base_frame_no * FRAME_SIZE);
	_info_frame_no = base_frame_no;
    } else {
        bitmap = (unsigned char *) (info_frame_no * FRAME_SIZE);
    }

    // Number of frames must be "fill" the bitmap!
    assert ((2*nframes % 8 ) == 0);

    // Two bits are used to store status of frames. 11 stands for free, 00
    //stands for ALLOCATED, 01 stands for HEAD-OF-SEQUENCES.
    // Everything ok. Proceed to mark all bits in the bitmap
    for(int i=0; i*8 < 2*_n_frames; i++) bitmap[i] = 0xFF;

    // Mark the first frame and sequent _n_info_frames as being used if it is
    //being used.

    // Mark the _info_frame_no frame as HEAD_OF_SQUENCE.
    unsigned int bitmap_index = (_info_frame_no-base_frame_no) / 4;
    unsigned char mask = 0x80 >> ((_info_frame_no-base_frame_no) % 4) * 2;

    bitmap[bitmap_index] = bitmap[bitmap_index]^mask;

    // Mark the remaining _n_info_frames-1 frames as ALLOCATED.
    mask = (mask >> 2) + (mask >> 3);
    _n_info_frames--;
    while((_n_info_frames > 0) && (mask != 0)) {
        bitmap[bitmap_index] = bitmap[bitmap_index]^mask;
        _n_info_frames--;
        mask = mask >> 2;
    }

    int i = bitmap_index+1;
    for(; i < bitmap_index+_n_info_frames / 4; i++) {
        bitmap[i] = 0x0;
    }

    mask = 0xc0;
    for(int j = 0; j < _n_info_frames % 4; j++) {
        bitmap[i] = bitmap[i]^mask;
        mask = mask >> 2;
    }

    pre = cur;
    cur = this;

    Console::puts("Frame Pool initialized\n");
}

unsigned long ContFramePool::get_frames(unsigned int _n_frames)
{
    // Any frames left to allocate?
    assert(nFreeFrames > 0);

    // Find a squence of at least _n_frames entries that are free. Mark the first
    //one as HEAD-OF-SQUENCES and mark the remaining ALLOCATED. Return the frame
    //index of the first one.
    unsigned int frame_no = 0;

    unsigned int i = 0; // i scans through bytes.
    unsigned char mask = 0x80; // mask scans in each byte.

    while (i < nframes/4) {
        // 01 and 00 are for ALLOCATED or HEAD_OF_SQUENCE frames. Therefore bitmap[i]
        //^oxaa == 0 meaning no 11 for these four frames, thus no free frames.
        while (((bitmap[i] & 0xaa) == 0) && (i < nframes/4)) {
            i++;
            frame_no += 4;
        }

        // Find the first frame that is free.
        while (((mask & bitmap[i]) == 0) && (mask != 0)) {
            mask = mask >> 2;
            frame_no++;
        }

        if (mask == 0) {
            mask = 0x80;
            i++;
            continue;
        }

        // Find the number of free frames in a sequence.
        unsigned int free_frame_no = 1;
        while (((mask & bitmap[i]) != 0) && i < nframes/4 && free_frame_no < _n_frames) {
            mask = mask >> 2;
            free_frame_no++;
            if (mask == 0) {
                i++;
                mask = 0x80;
            }
        }

        // Check whether number of sequent free frames are larger than _n_frames.
        if (free_frame_no == _n_frames) {
            nFreeFrames = nFreeFrames-_n_frames;

            i = frame_no/4;

            // Mark the first frame as HEAD-OF-SQUENCES.
            mask = 0x80 >> (frame_no % 4)*2;
            bitmap[i] = bitmap[i]^mask;
            _n_frames--;

            // Mark the remaining frames as ALLOCATED.
            mask = 0xc0 >> ((frame_no % 4) + 1) * 2;
            while ((mask != 0) && (_n_frames > 0)) {
                bitmap[i] = bitmap[i]^mask;
                mask = mask >> 2;
                _n_frames--;
            }
            i++;

            for(int j=0;j<_n_frames/4;j++) {
                bitmap[i++] = 0x0;
            }

            mask = 0xc0;
            _n_frames = _n_frames%4;
            for(int j=0;j<_n_frames;j++) {
                bitmap[i] = bitmap[i]^mask;
                mask = mask >> 2;
            }
            return frame_no+base_frame_no;
        }
	
	frame_no = frame_no+free_frame_no;
    }

    // Return 0 if there is no sequent _n_frames free frames.
    return 0;
}

void ContFramePool::mark_inaccessible(unsigned long _base_frame_no,
                                      unsigned long _n_frames)
{
    instance_mark_inaccessible(_base_frame_no,true);
    int i = 0;
    for(i = _base_frame_no + 1; i < _base_frame_no + _n_frames; i++) {
      instance_mark_inaccessible(i,false);
    }
    nFreeFrames -= _n_frames;
}

void ContFramePool::instance_mark_inaccessible(unsigned long _frame_no, bool _head)
{
    // Let's first do a range check.
    assert ((_frame_no >= base_frame_no) && (_frame_no < base_frame_no + nframes));

    unsigned int bitmap_index = (_frame_no-base_frame_no)/4;
    unsigned char mask = 0x80 >> ((_frame_no-base_frame_no)%4)*2;

    // Is the frame being used already?
    assert((bitmap[bitmap_index] & mask) != 0);

    if (!_head) mask = mask + (mask >> 1);

    // Update bitmap
    bitmap[bitmap_index] = bitmap[bitmap_index]^mask;
    nFreeFrames--;
}

void ContFramePool::release_frames(unsigned long _first_frame_no)
{
    // Iterate through all pools to locate frames needed to be released.
    ContFramePool * pool_iter = ContFramePool::cur;

    while(pool_iter) {
        if(_first_frame_no >= pool_iter -> base_frame_no && _first_frame_no < ((pool_iter -> base_frame_no) + (pool_iter -> nframes))) {
            pool_iter -> instance_release_frames(_first_frame_no);
            return;
        }
        pool_iter = pool_iter->pre;
    }

    // Throw an error if frames needed to be released don't belong to any pool.
    Console::puts("Error, corresponding pool is not found.\n");
    assert(false);
}

void ContFramePool::instance_release_frames(unsigned long _first_frame_no)
{
    unsigned int bitmap_index = (_first_frame_no - base_frame_no) / 4;
    unsigned char mask = 0x80 >> ((_first_frame_no - base_frame_no) % 4) * 2;

    // Make sure the first frame to be released is a HEAD_OF_SQUENCE.
    if (((bitmap[bitmap_index] & mask) != 0) || ((bitmap[bitmap_index] & (mask >> 1)) == 0)) {
        Console::puts("Error, the first frame being released is not a HEAD_OF_SQUENCE.\n");
        assert(false);
    }

    // Mark the first frame as FREE.
    bitmap[bitmap_index] = bitmap[bitmap_index] ^ mask;
    nFreeFrames++;

    // Mark the remaining ALLOCATED frames as FREE.
    mask = (mask >> 2) + (mask >> 3);
    while(mask != 0 && ((bitmap[bitmap_index] & mask) == 0)) {
        bitmap[bitmap_index] = bitmap[bitmap_index]^mask;
        mask = mask >> 2;
	nFreeFrames++;
    }
    
    if(mask == 0) {
        bitmap_index++;
        while(bitmap[bitmap_index] == 0) {
	    bitmap[bitmap_index++] = 0xFF;
	    nFreeFrames += 4;
        }

        mask = 0xc0;
        while(mask != 0 && ((bitmap[bitmap_index] & mask) == 0)) {
	    bitmap[bitmap_index] = bitmap[bitmap_index]^mask;
	    mask = mask >> 2;
	    nFreeFrames++;
        }
    }
}

unsigned long ContFramePool::needed_info_frames(unsigned long _n_frames)
{
    // We need 2*_n_frames bits for info frames.
    return _n_frames / (4 * FRAME_SIZE) + (_n_frames % (4 * FRAME_SIZE) > 0? 1 : 0);
}
