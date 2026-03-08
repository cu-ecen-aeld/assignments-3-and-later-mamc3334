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
 * 
 * @author Mason McGaffin
 * @date 2026-03-03
 */

 /** AI USE DISCLAIMER
  * 
  * I used Claude AI to debug my code. 
  * I included build outputs and the source code I wrote to find and correct bugs.
  * 
  * Links to history: 
  * - https://claude.ai/share/097123e4-c175-498e-a0a1-74bc463e52a8
  * - https://claude.ai/share/662d7365-0c23-4bae-882d-7d750af5baba 
  */


#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include "aesdchar.h"
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Mason McGaffin");
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

//fun declarations
int aesd_open(struct inode *inode, struct file *filp);
int aesd_release(struct inode *inode, struct file *filp);
ssize_t aesd_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos);
ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos);
int aesd_init_module(void);
void aesd_cleanup_module(void);

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");

    struct aesd_dev *dev;
    dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
    filp->private_data = dev;
    
    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    /**
     * TODO: handle release
     */
    
    //nothing to do
    
    return 0;
}

/**
 * @return retval;  retval==count - Everything requested was transferred;
 *                  0<retval<count - Only some returned - partial read
 *                  retval==0 - End of file;
 *                  retval<0 - Error - -ERESTARTSYS, -EINTR, -EFAULT
 */
ssize_t aesd_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
    ssize_t retval = 0; // expect to change 
    PDEBUG("trying to read %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle read
     */

    struct aesd_dev* dev = (struct aesd_dev*) filp->private_data;

    // lock
    if(mutex_lock_interruptible(&dev->lock)) return -ERESTARTSYS;

    size_t ret_offset = 0;
    //handle offset
    struct aesd_buffer_entry *ret_entry = aesd_circular_buffer_find_entry_offset_for_fpos(&dev->circularBuffer, *f_pos, &ret_offset);
    if(!ret_entry)
    {
        PDEBUG("Byte not found at specified offset");
        mutex_unlock(&dev->lock);
        return 0;
    }

    // byte found - copy rest of entry
    retval = ret_entry->size - ret_offset;
    if(retval > count) retval = count; //more bytes in command than count - don't read full command

    int status = copy_to_user(buf, ret_entry->buffptr + ret_offset, retval);
    if(status != 0)
    {
        PDEBUG("Copy failed. Could not copy %u bytes", status);
        mutex_unlock(&dev->lock);
        return -EFAULT;
    }

    //update pointer
    *f_pos += retval;

    //Unlock
    mutex_unlock(&dev->lock);

    return retval;
}

/**
 * @param filp file pointer
 * @param buf  user buffer
 * @param count number of bytes to write
 * @param f_pos offset - simple
 * 
 * @return retval;  retval==count - successfully wrote command; 
 *                  0<return<count - only part written, retry;
 *                  retval==0 - Nothing written, retry;
 *                  retval<0 - Error code - -ENOMEM, -EFAULT
 */
ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
    ssize_t retval = -ENOMEM;
    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle write
     */
    
    // malloc buffer of specified size
    char* temp_buffer = kmalloc(count, GFP_KERNEL);
    if(!temp_buffer)
    {
        return -ENOMEM;
    }
    
    // copy user buffer to kernel space
    if (copy_from_user(temp_buffer, buf, count))
    {
        kfree(temp_buffer);
        return -EFAULT;
    }

    int index = 0;
    while(index < count)
    {
        if(temp_buffer[index] == '\n')
        {
            break;
        }
        index++;
    }
    
    bool completeCommand = false;
    if(index != count)
    {
        completeCommand = true;
    }

    size_t newCommandSize = (size_t)(completeCommand ? (index+1) : index);

    struct aesd_dev* dev = (struct aesd_dev*) filp->private_data;

    if (mutex_lock_interruptible(&dev->lock))
    {
        retval = -ERESTARTSYS;
        goto eofunc;
    }

    PDEBUG("write: count=%zu completeCommand=%d tempEntry.size before=%zu after=%zu",
       count, completeCommand, dev->tempEntry.size, 
       dev->tempEntry.size + newCommandSize);

    char* new_buffer;
    if(dev->tempEntry.buffptr == NULL) 
    {
        new_buffer = kmalloc(newCommandSize, GFP_KERNEL);
    } 
    else 
    {
        new_buffer = krealloc(dev->tempEntry.buffptr, dev->tempEntry.size + newCommandSize, GFP_KERNEL);
    }
    
    if(!new_buffer)
    {
        retval = -ENOMEM;
        goto eofunc;
    }
    dev->tempEntry.buffptr = new_buffer;

    PDEBUG("write: count=%zu completeCommand=%d tempEntry.size before=%zu after=%zu",
       count, completeCommand, dev->tempEntry.size, 
       dev->tempEntry.size + newCommandSize);

    //copy to allocated buffer
    memcpy((void *)dev->tempEntry.buffptr + dev->tempEntry.size, temp_buffer, newCommandSize);
    dev->tempEntry.size += newCommandSize;

    //if complete command, write to circular buffer
    if(completeCommand)
    {
        char *override = aesd_circular_buffer_add_entry(&dev->circularBuffer, &dev->tempEntry);
        
        if(override)
        {
            kfree(override);
        }

        //reset tempEntry
        dev->tempEntry.buffptr = NULL;
        dev->tempEntry.size = 0;
    }

    PDEBUG("write: count=%zu completeCommand=%d tempEntry.size before=%zu after=%zu",
       count, completeCommand, dev->tempEntry.size, 
       dev->tempEntry.size + newCommandSize);

    retval = newCommandSize;
    
    eofunc:
    //release lock
    mutex_unlock(&dev->lock);
    if(temp_buffer) kfree(temp_buffer);

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

    // init mutex
    mutex_init(&aesd_device.lock);
    
    // init circular buffer
    aesd_circular_buffer_init(&aesd_device.circularBuffer);

    //tempEntry exists but is empty - must cleanup

    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    return result;

}

void aesd_cleanup_module(void)
{
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    /**
     * TODO: cleanup AESD specific poritions here as necessary
     */

    //free tempEntry
    if(aesd_device.tempEntry.buffptr) kfree(aesd_device.tempEntry.buffptr);

    //free circular buffer
    uint8_t index;
    struct aesd_buffer_entry *entry;
    AESD_CIRCULAR_BUFFER_FOREACH(entry, &aesd_device.circularBuffer, index)
    {
        if (entry->buffptr) kfree((void *)entry->buffptr);
    }

    //free mutex
    mutex_destroy(&aesd_device.lock);

    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
