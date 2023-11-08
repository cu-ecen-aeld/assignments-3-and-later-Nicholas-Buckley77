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

// assignment 9
#include "aesdchar.h"
#include "aesd_ioctl.h"

int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Nicholas Buckley");
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;


// scull open but aesd instead of scull!
int aesd_open(struct inode *inode, struct file *filp)
{
    struct aesd_dev* dev;
    PDEBUG("open");
    dev = container_of(inode->i_cdev, struct aesd_dev , cdev);
    filp->private_data = dev;
    return 0;
}

/* Not required as it is handled elsewhere*/
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
    
    if(filp == NULL )
    {
        PDEBUG("Null pointer passed to read...");
        retval = -EFAULT; 
        goto exit_early_read;
    }

    local_aesd_dev_ptr = (struct aesd_dev*)(filp->private_data);

    if(mutex_lock_interruptible(&(local_aesd_dev_ptr->aesd_mutex)))
    {
        printk(KERN_ALERT "Mutex for local dev filp private data didn't lock!!!");
        retval = -ERESTARTSYS; 
        goto exit_early_read;
    }


   
    foundEntry= aesd_circular_buffer_find_entry_offset_for_fpos(&local_aesd_dev_ptr->circBuf, *f_pos, &offsetRtrn);

    if(foundEntry == NULL)
    {
        // entry not found
        printk(KERN_ALERT "Entry not found!!!");
        mutex_unlock(&local_aesd_dev_ptr->aesd_mutex);
        goto exit_early_read;
    }
    
    if(count > foundEntry->size - offsetRtrn) // if the read to more than is left to read from the entry
    {
        bytesAvailToRead = foundEntry->size - offsetRtrn; // can't read count bytes so read what we can
    }

    // Send entry found at the offset with remaining or max count
    bytesNotCopied = copy_to_user(buf, (foundEntry->buffptr + offsetRtrn), bytesAvailToRead);

    if(bytesNotCopied != 0)
    {
        printk(KERN_ALERT "bytes not coppied!!!");
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
    
    printk(KERN_ALERT "freeing an entry");

    kfree(buffer->entry[buffer->in_offs].buffptr);
    buffer->entry[buffer->in_offs].size = 0;
    buffer->entry[buffer->in_offs].buffptr = NULL;
    
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

    
    if(local_aesd_dev_ptr->entryToWrite.size == 0) // if nothing is being written
    {
        // kmalloc the buffer of count bytes
        local_aesd_dev_ptr->entryToWrite.buffptr = kmalloc(count * sizeof(char), GFP_KERNEL);
    }
    else
    {
        // realloc to the new size count bytes
        local_aesd_dev_ptr->entryToWrite.buffptr = krealloc(local_aesd_dev_ptr->entryToWrite.buffptr, (count + local_aesd_dev_ptr->entryToWrite.size)*sizeof(char), GFP_KERNEL);
    }

    if(local_aesd_dev_ptr->entryToWrite.buffptr == NULL)
    {
        retval = -ENOMEM;  // not enough memory
        mutex_unlock(&local_aesd_dev_ptr->aesd_mutex); // unlock mutex for next transaction
        goto exit_early_write;
    }

    // append to currently being written buffer
    uncopBytes = copy_from_user((void*)(local_aesd_dev_ptr->entryToWrite.buffptr + local_aesd_dev_ptr->entryToWrite.size), buf, count);
    local_aesd_dev_ptr->entryToWrite.size += count - uncopBytes; 
    retval = count - uncopBytes;

    *f_pos += retval;

    i = 0;
    newLineFound = 0;
    while (!newLineFound  && i < local_aesd_dev_ptr->entryToWrite.size)
    {
        
        if(local_aesd_dev_ptr->entryToWrite.buffptr[i] == '\n')
        {
            retval = local_aesd_dev_ptr->entryToWrite.size;
            if(local_aesd_dev_ptr->circBuf.full) // if Im overwritting an already existing entry
            { // free it before overwrite

                freeCurrentInEntry(&(local_aesd_dev_ptr->circBuf));
            }
            printk(KERN_ALERT "Adding entry %s %ld\r\n",local_aesd_dev_ptr->entryToWrite.buffptr, local_aesd_dev_ptr->entryToWrite.size);

            aesd_circular_buffer_add_entry(&local_aesd_dev_ptr->circBuf,&local_aesd_dev_ptr->entryToWrite);
            local_aesd_dev_ptr->entryToWrite.buffptr = NULL;
            local_aesd_dev_ptr->entryToWrite.size = 0;
            newLineFound ++;
        }
        i++;
    }
    if(newLineFound == 0)
    {
        printk(KERN_ALERT "didnt add entry %s %ld\r\n",local_aesd_dev_ptr->entryToWrite.buffptr, local_aesd_dev_ptr->entryToWrite.size);
    }

    mutex_unlock(&local_aesd_dev_ptr->aesd_mutex); // unlock mutex for next transaction

    

    exit_early_write:

    return retval;
}


// assignment 9
/* wrapper for fixed_size_llseek*/
loff_t aesd_llseek(struct file *filp, loff_t offset, int whence)
{
    loff_t retval = 0;
    loff_t bytesTotal = 0;
    int buffIndex = 0;

    struct aesd_dev* local_aesd_dev_ptr;

    struct aesd_buffer_entry* countEntry;

    if(filp == NULL )
    {
        PDEBUG("Null pointer passed to llseek...");
        retval = -EFAULT; 
        goto exit_early_llseek;
    }
    local_aesd_dev_ptr = (struct aesd_dev*)(filp->private_data);

    if(mutex_lock_interruptible(&(local_aesd_dev_ptr->aesd_mutex)))
    {
        printk(KERN_ALERT "Mutex for local dev filp private data didn't lock!!!");
        retval = -ERESTARTSYS; 
        goto exit_early_llseek;
    }

    AESD_CIRCULAR_BUFFER_FOREACH(countEntry, &local_aesd_dev_ptr->circBuf, buffIndex)
    {
        // get the total size of the content of the buffer for fixed size llseek
        bytesTotal += countEntry->size;
    }


    retval = fixed_size_llseek(filp,offset,whence,bytesTotal);
    mutex_unlock(&local_aesd_dev_ptr->aesd_mutex); // unlock mutex for next transaction



    exit_early_llseek:

    return retval;

}
// from overview suggestions

static long aesd_adjust_file_offset(struct file *filp, unsigned int write_cmd, unsigned int write_cmd_offset)
{
    long retVal = 0;
    int buffCount = 0;
    loff_t newFpo;

    struct aesd_dev* local_aesd_dev_ptr;

    struct aesd_buffer_entry* countEntry;

    if(filp == NULL )
    {
        PDEBUG("Null pointer passed to adjust...");
        retVal = -EFAULT; 
        goto exit_early_adjust;
    }
    local_aesd_dev_ptr = (struct aesd_dev*)(filp->private_data);

    
    newFpo = filp->f_pos;

    if(mutex_lock_interruptible(&(local_aesd_dev_ptr->aesd_mutex)))
    {
        printk(KERN_ALERT "Mutex for local dev filp private data didn't lock!!!");
        retVal = -ERESTARTSYS; 
        goto exit_early_adjust;
    }

    // count entries and sum size to cmd
    AESD_CIRCULAR_BUFFER_FOREACH(countEntry, &local_aesd_dev_ptr->circBuf, buffCount)
    {
        if (buffCount < write_cmd)
        {
            newFpo += countEntry->size;
        }
    }


    // haven't written the command yet
    if (write_cmd > buffCount  || write_cmd < 0 ||
    // command out of range
    write_cmd > AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED || write_cmd > buffCount ||
    // command offset >= size of cmd
    write_cmd_offset < 0 || write_cmd_offset >= local_aesd_dev_ptr->circBuf.entry[write_cmd].size)
    {
        PDEBUG("copy_from_user returned zero in aesd_ioctl...");
        retVal = -EINVAL;
        mutex_unlock(&local_aesd_dev_ptr->aesd_mutex);
        goto exit_early_adjust;
    }

    // size to command + offset = newFpointer
    newFpo += write_cmd_offset;

    filp->f_pos = newFpo;
    mutex_unlock(&local_aesd_dev_ptr->aesd_mutex); // unlock mutex for next transaction

    exit_early_adjust:

    return retVal;

}

// based on unlocked ioctl and overview example!
static long aesd_ioctl(struct file *filp, unsigned int write_cmd, unsigned long write_arg)
{
    long retVal = 0;

    struct aesd_seekto seek;

    struct aesd_dev* local_aesd_dev_ptr;


    if(_IOC_TYPE(write_cmd) != AESD_IOC_MAGIC)
    {
        PDEBUG("Non-Magic cmd type...");
        retVal = -ENOTTY;
        goto exit_early_ioctl;

    }

    if(_IOC_NR(write_cmd) > AESDCHAR_IOC_MAXNR)
    {
        PDEBUG("NR greater than max NR...");
        retVal = -ENOTTY;
        goto exit_early_ioctl;

    }

    if(filp == NULL )
    {
        PDEBUG("Null pointer passed to ioctl...");
        retVal = -EFAULT; 
        goto exit_early_ioctl;
    }

    local_aesd_dev_ptr = (struct aesd_dev*)(filp->private_data);

    
    // lecture implementation
    
    switch (write_cmd)
    {
        case AESDCHAR_IOCSEEKTO:

        if (copy_from_user(&seek, (struct aesd_seekto *)write_arg, sizeof(struct aesd_seekto)) != 0)
        {
            PDEBUG("copy_from_user did not get all values in aesd_ioctl...");
            retVal = -EFAULT;
            goto exit_early_ioctl;
        }
        else
        {
            retVal = aesd_adjust_file_offset(filp, seek.write_cmd, seek.write_cmd_offset);
        }
        break;

        default:
            retVal = -ENOTTY;
        break;
    }

    exit_early_ioctl:

    return retVal;
}


struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
    .llseek = aesd_llseek,
    .unlocked_ioctl = aesd_ioctl,
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
    memset(&aesd_device.entryToWrite,0,sizeof(struct aesd_buffer_entry));
    
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

    kfree(aesd_device.entryToWrite.buffptr );
    aesd_device.entryToWrite.buffptr = NULL;
    aesd_device.entryToWrite.size = 0;
    mutex_destroy(&aesd_device.aesd_mutex);


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
