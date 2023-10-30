/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include "aesdchar.h"
#include <linux/slab.h>

int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Nicholas Buckley"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

struct aesd_buffer_entry entryToWrite;

// scull open but aesd instead of scull!
int aesd_open(struct inode *inode, struct file *filp)
{
    struct aesd_dev* dev;
    PDEBUG("open");
    /**
     * TODO: handle open container of with inode->i_cdev 
     */
    dev = container_of(inode->i_cdev, struct aesd_dev , cdev);
    filp->private_data = dev;
    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    /**
     * handled by module_exit since filp data struct is allocated in init
     */
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = 0;
    ssize_t offsetRtrn = 0;
    ssize_t bytesAvailToRead = count;
    uint32_t bytesNotCopied;

    struct aesd_dev* local_aesd_dev_ptr;

    struct aesd_buffer_entry* foundEntry;

    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle read copy_to_user
     */
    if(filp == NULL || buf == NULL || f_pos == NULL)
    {
        PDEBUG("Null pointer passed to read...");
        retval = -EFAULT; 
        goto exit_early_read;
    }

    local_aesd_dev_ptr = (struct aesd_dev*)(filp->private_data);

    if(mutex_lock_interruptible(&local_aesd_dev_ptr->aesd_mutex))
    {
        printk(KERN_ALERT "Mutex for local dev filp private data didn't lock!!!");
        retval = -ERESTARTSYS; 
        goto exit_early_read;
    }


   
    foundEntry= aesd_circular_buffer_find_entry_offset_for_fpos(&local_aesd_dev_ptr->circBuf, *f_pos, &offsetRtrn);

    if(foundEntry == NULL)
    {
        // entry not found
        mutex_unlock(&local_aesd_dev_ptr->aesd_mutex);
        goto exit_early_read;
    }
    
    if(count > foundEntry->size - offsetRtrn) // if the read to more than is left to read from the entry
    {
        bytesAvailToRead = foundEntry->size - offsetRtrn; // can't read count bytes so read what we can
    }

    // Send entry found at the offset with remaining or max count
    bytesNotCopied = copy_to_user(buf, (foundEntry->buffptr + offsetRtrn), bytesAvailToRead);

    if(!bytesNotCopied)
    {
        printk(KERN_ALERT "Mutex for local dev filp private data didn't lock!!!");
        retval = -ERESTARTSYS;
        mutex_unlock(&local_aesd_dev_ptr->aesd_mutex);
        goto exit_early_read;
    }

    retval = bytesAvailToRead; // we read the available bytes
    *f_pos += bytesAvailToRead; // update file position
    mutex_unlock(&local_aesd_dev_ptr->aesd_mutex); // unlock mutex for next transaction

    exit_early_read:
    return retval;
}

void freeCurrentInEntry(struct aesd_circular_buffer *buffer)
{
    kfree(buffer->entry[buffer->in_offs].buffptr);
    buffer->entry[buffer->in_offs].size = 0;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = -ENOMEM;
    struct aesd_dev* local_aesd_dev_ptr;
    ssize_t uncopBytes;
    int i;
    uint8_t newLineFound;


    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle write
     */
    if(filp == NULL || buf == NULL || f_pos == NULL)
    {
        PDEBUG("Null pointer passed to write...");
        retval = -EFAULT; 
        goto exit_early_write;
    }

    local_aesd_dev_ptr = (struct aesd_dev*)(filp->private_data);


    if(mutex_lock_interruptible(&(local_aesd_dev_ptr->aesd_mutex)))
    {
        printk(KERN_ALERT "Mutex for local dev filp private data didn't lock!!!");
        retval = -ERESTARTSYS; 
        goto exit_early_write;
    }

    
    if(entryToWrite.size == 0) // if nothing is being written
    {
        // kmalloc the buffer of count bytes
        entryToWrite.buffptr = kmalloc(count * sizeof(char), GFP_KERNEL);
    }
    else
    {
        // realloc to the new size count bytes
        entryToWrite.buffptr = krealloc(entryToWrite.buffptr, (count + entryToWrite.size)*sizeof(char), GFP_KERNEL);
    }

    if(entryToWrite.buffptr == NULL)
    {
        retval = -ENOMEM;  // not enough memory
        mutex_unlock(&local_aesd_dev_ptr->aesd_mutex); // unlock mutex for next transaction
        goto exit_early_write;
    }

    // append to currently being written buffer
    uncopBytes = copy_from_user((void*)(entryToWrite.buffptr + entryToWrite.size), buf, count);
    entryToWrite.size += count - uncopBytes; 
    i = 0;
    newLineFound = 0;
    while (!newLineFound  && i < entryToWrite.size)
    {
        
        if(entryToWrite.buffptr[i] == '\n')
        {
            retval = entryToWrite.size;
            if(local_aesd_dev_ptr->circBuf.full) // if Im overwritting an already existing entry
            { // free it before overwrite
                freeCurrentInEntry(&local_aesd_dev_ptr->circBuf);
            }

            aesd_circular_buffer_add_entry(&local_aesd_dev_ptr->circBuf,&entryToWrite);
            kfree(entryToWrite.buffptr);
            entryToWrite.buffptr = NULL;
            entryToWrite.size = 0;
            newLineFound ++;
        }
        i++;
    }

    mutex_unlock(&local_aesd_dev_ptr->aesd_mutex); // unlock mutex for next transaction

    

    exit_early_write:

    return retval;
}
struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add (&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}



int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;
    result = alloc_chrdev_region(&dev, aesd_minor, 1,
            "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device,0,sizeof(struct aesd_dev));

    /**
     * TODO: initialize the AESD specific portion of the device
     */
    memset(&entryToWrite,0,sizeof(struct aesd_buffer_entry));
    
    mutex_init(&aesd_device.aesd_mutex);


    aesd_circular_buffer_init(&aesd_device.circBuf);
    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    return result;

}

void aesd_cleanup_module(void)
{
    struct aesd_buffer_entry *curEntry;
    int i;
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    /**
     * TODO: cleanup AESD specific poritions here as necessary
     */
    kfree(entryToWrite.buffptr );
    entryToWrite.buffptr = NULL;
    entryToWrite.size = 0;



    AESD_CIRCULAR_BUFFER_FOREACH(curEntry, &aesd_device.circBuf, i)
    {
        if(curEntry != NULL)
        {
            kfree(curEntry->buffptr);
            curEntry->buffptr = NULL;
            curEntry->size = 0;
        }
    }

    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
