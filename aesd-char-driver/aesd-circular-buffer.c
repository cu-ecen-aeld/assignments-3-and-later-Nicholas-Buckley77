/**
 * @file aesd-circular-buffer.c
 * @brief Functions and data related to a circular buffer imlementation
 *
 * @author Dan Walkes
 * @date 2020-03-01
 * @copyright Copyright (c) 2020
 *
 */

#ifdef __KERNEL__
#include <linux/string.h>
#else
#include <string.h>
#endif

#include "aesd-circular-buffer.h"

/**
 * @param buffer the buffer to search for corresponding offset.  Any necessary locking must be performed by caller.
 * @param char_offset the position to search for in the buffer list, describing the zero referenced
 *      character index if all buffer strings were concatenated end to end
 * @param entry_offset_byte_rtn is a pointer specifying a location to store the byte of the returned aesd_buffer_entry
 *      buffptr member corresponding to char_offset.  This value is only set when a matching char_offset is found
 *      in aesd_buffer.
 * @return the struct aesd_buffer_entry structure representing the position described by char_offset, or
 * NULL if this position is not available in the buffer (not enough data is written).
 */
struct aesd_buffer_entry *aesd_circular_buffer_find_entry_offset_for_fpos(struct aesd_circular_buffer *buffer,
            size_t char_offset, size_t *entry_offset_byte_rtn )
{
    //check if buffer is not null
     if(buffer == NULL)
     {
      return NULL;
    }

    //save the initial index 
    uint8_t entryIndex = buffer->out_offs;

    int index = char_offset ;

    // while there are still entries to check...
    while (buffer->entry[entryIndex].buffptr !=NULL && entryIndex < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED + buffer->out_offs)
    {
        // check if the char index is located in the buffer entry

        // if it is not and the index is past the char this entries char count...
        if(index >= buffer->entry[entryIndex%AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED].size )
        {
            // update the index by subtracting the char count of the entry and go to the next entry
            index-=buffer->entry[entryIndex%AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED].size;
            entryIndex++;
        }
        else
        {
            // if it is return the entry and index!
            *entry_offset_byte_rtn = index;
            return &buffer->entry[entryIndex%AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED];
        }
    }
    return NULL;

}

/**
* Adds entry @param add_entry to @param buffer in the location specified in buffer->in_offs.
* If the buffer was already full, overwrites the oldest entry and advances buffer->out_offs to the
* new start location.
* Any necessary locking must be handled by the caller
* Any memory referenced in @param add_entry must be allocated by and/or must have a lifetime managed by the caller.
*/
void aesd_circular_buffer_add_entry(struct aesd_circular_buffer *buffer, const struct aesd_buffer_entry *add_entry)
{
    /**
    * chat gpt assisted implementation "add circular buffer entry for wrapping circular buffer c" (it mostly reminded me of the structure and order of implementation)
    */
    
    // add or override entry and size to the current new in_offs
    buffer->entry[buffer->in_offs].buffptr = add_entry->buffptr;
    buffer->entry[buffer->in_offs].size = add_entry->size;

    // check if the buffer was full...
    if(buffer->full)
    {
        // if it was then update the out as it just got overriden
        buffer->out_offs = (buffer->out_offs + 1)%AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    }

    // update buffer in_off
    buffer->in_offs = (buffer->in_offs + 1)%AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;

    // if in and out are the same
    if(buffer->in_offs ==  buffer->out_offs )
    {
        // the buffer is full
        buffer->full = true;
    }
    

}

/**
* Initializes the circular buffer described by @param buffer to an empty struct
*/
void aesd_circular_buffer_init(struct aesd_circular_buffer *buffer)
{
    memset(buffer,0,sizeof(struct aesd_circular_buffer));
}
