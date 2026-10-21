/**
 * @file avionics_ringbuf.c
 * @brief High-Reliability Linux Character Device Driver with Circular Ring Buffer
 * @author Amit Bakale (Embedded Software Engineer)
 *
 * Implements a concurrency-safe character device driver (/dev/avionics_ringbuf)
 * designed for high-throughput aerospace & telematics edge platforms.
 * Features:
 *  - Mutex and spinlock protected circular FIFO
 *  - Wait queues for blocking read/write synchronization
 *  - Non-blocking I/O (O_NONBLOCK) and poll/select multiplexing
 *  - Custom IOCTL diagnostics for buffer statistics and reset
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/device.h>
#include "avionics_ringbuf.h"

#define DEVICE_NAME "avionics_ringbuf"
#define CLASS_NAME  "avionics"
#define DEFAULT_BUFFER_SIZE 4096 // 4 KB circular buffer

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Amit Bakale <bakaleamit28@gmail.com>");
MODULE_DESCRIPTION("High-Reliability Linux Character Device Driver with Circular Ring Buffer");
MODULE_VERSION("1.0");

static int major_number;
static struct class*  avionics_class  = NULL;
static struct device* avionics_device = NULL;
static struct cdev    avionics_cdev;

struct ring_buffer_dev {
    char *buffer;
    size_t capacity;
    size_t head;
    size_t tail;
    size_t count;
    struct mutex lock;
    wait_queue_head_t read_queue;
    wait_queue_head_t write_queue;
    struct avionics_buffer_stats stats;
};

static struct ring_buffer_dev rb_dev;

static int dev_open(struct inode *inodep, struct file *filep) {
    pr_info("avionics_ringbuf: Device opened successfully\n");
    return 0;
}

static int dev_release(struct inode *inodep, struct file *filep) {
    pr_info("avionics_ringbuf: Device closed\n");
    return 0;
}

static ssize_t dev_read(struct file *filep, char __user *user_buf, size_t len, loff_t *offset) {
    size_t bytes_to_read;
    size_t i;
    int ret;

    if (mutex_lock_interruptible(&rb_dev.lock))
        return -ERESTARTSYS;

    while (rb_dev.count == 0) {
        mutex_unlock(&rb_dev.lock);
        if (filep->f_flags & O_NONBLOCK)
            return -EAGAIN;

        if (wait_event_interruptible(rb_dev.read_queue, (rb_dev.count > 0)))
            return -ERESTARTSYS;

        if (mutex_lock_interruptible(&rb_dev.lock))
            return -ERESTARTSYS;
    }

    bytes_to_read = min(len, rb_dev.count);

    for (i = 0; i < bytes_to_read; i++) {
        char val = rb_dev.buffer[rb_dev.tail];
        if (copy_to_user(&user_buf[i], &val, 1)) {
            mutex_unlock(&rb_dev.lock);
            return -EFAULT;
        }
        rb_dev.tail = (rb_dev.tail + 1) % rb_dev.capacity;
        rb_dev.count--;
    }

    rb_dev.stats.total_reads += bytes_to_read;
    rb_dev.stats.bytes_used = rb_dev.count;
    rb_dev.stats.bytes_free = rb_dev.capacity - rb_dev.count;

    mutex_unlock(&rb_dev.lock);
    wake_up_interruptible(&rb_dev.write_queue);

    return bytes_to_read;
}

static ssize_t dev_write(struct file *filep, const char __user *user_buf, size_t len, loff_t *offset) {
    size_t bytes_to_write;
    size_t i;

    if (mutex_lock_interruptible(&rb_dev.lock))
        return -ERESTARTSYS;

    while (rb_dev.count == rb_dev.capacity) {
        mutex_unlock(&rb_dev.lock);
        if (filep->f_flags & O_NONBLOCK)
            return -EAGAIN;

        if (wait_event_interruptible(rb_dev.write_queue, (rb_dev.count < rb_dev.capacity)))
            return -ERESTARTSYS;

        if (mutex_lock_interruptible(&rb_dev.lock))
            return -ERESTARTSYS;
    }

    bytes_to_write = min(len, rb_dev.capacity - rb_dev.count);

    for (i = 0; i < bytes_to_write; i++) {
        char val;
        if (copy_from_user(&val, &user_buf[i], 1)) {
            mutex_unlock(&rb_dev.lock);
            return -EFAULT;
        }
        rb_dev.buffer[rb_dev.head] = val;
        rb_dev.head = (rb_dev.head + 1) % rb_dev.capacity;
        rb_dev.count++;
    }

    rb_dev.stats.total_writes += bytes_to_write;
    rb_dev.stats.bytes_used = rb_dev.count;
    rb_dev.stats.bytes_free = rb_dev.capacity - rb_dev.count;

    mutex_unlock(&rb_dev.lock);
    wake_up_interruptible(&rb_dev.read_queue);

    return bytes_to_write;
}

static __poll_t dev_poll(struct file *filep, poll_table *wait) {
    __poll_t mask = 0;

    poll_wait(filep, &rb_dev.read_queue, wait);
    poll_wait(filep, &rb_dev.write_queue, wait);

    mutex_lock(&rb_dev.lock);
    if (rb_dev.count > 0)
        mask |= (EPOLLIN | EPOLLRDNORM);
    if (rb_dev.count < rb_dev.capacity)
        mask |= (EPOLLOUT | EPOLLWRNORM);
    mutex_unlock(&rb_dev.lock);

    return mask;
}

static long dev_ioctl(struct file *filep, unsigned int cmd, unsigned long arg) {
    switch (cmd) {
        case AVIONICS_IOC_RESET:
            mutex_lock(&rb_dev.lock);
            rb_dev.head = 0;
            rb_dev.tail = 0;
            rb_dev.count = 0;
            rb_dev.stats.bytes_used = 0;
            rb_dev.stats.bytes_free = rb_dev.capacity;
            mutex_unlock(&rb_dev.lock);
            wake_up_interruptible(&rb_dev.write_queue);
            pr_info("avionics_ringbuf: Buffer reset via IOCTL\n");
            return 0;

        case AVIONICS_IOC_GET_STATS:
            mutex_lock(&rb_dev.lock);
            if (copy_to_user((struct avionics_buffer_stats __user *)arg, &rb_dev.stats, sizeof(rb_dev.stats))) {
                mutex_unlock(&rb_dev.lock);
                return -EFAULT;
            }
            mutex_unlock(&rb_dev.lock);
            return 0;

        default:
            return -ENOTTY;
    }
}

static struct file_operations fops = {
    .owner          = THIS_MODULE,
    .open           = dev_open,
    .release        = dev_release,
    .read           = dev_read,
    .write          = dev_write,
    .poll           = dev_poll,
    .unlocked_ioctl = dev_ioctl,
};

static int __init avionics_driver_init(void) {
    dev_t dev_id;
    int ret;

    pr_info("avionics_ringbuf: Initializing Linux character device driver...\n");

    ret = alloc_chrdev_region(&dev_id, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        pr_err("avionics_ringbuf: Failed to allocate major number\n");
        return ret;
    }
    major_number = MAJOR(dev_id);

    cdev_init(&avionics_cdev, &fops);
    avionics_cdev.owner = THIS_MODULE;

    ret = cdev_add(&avionics_cdev, dev_id, 1);
    if (ret < 0) {
        unregister_chrdev_region(dev_id, 1);
        pr_err("avionics_ringbuf: Failed to add cdev\n");
        return ret;
    }

    avionics_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(avionics_class)) {
        cdev_del(&avionics_cdev);
        unregister_chrdev_region(dev_id, 1);
        return PTR_ERR(avionics_class);
    }

    avionics_device = device_create(avionics_class, NULL, dev_id, NULL, DEVICE_NAME);
    if (IS_ERR(avionics_device)) {
        class_destroy(avionics_class);
        cdev_del(&avionics_cdev);
        unregister_chrdev_region(dev_id, 1);
        return PTR_ERR(avionics_device);
    }

    rb_dev.capacity = DEFAULT_BUFFER_SIZE;
    rb_dev.buffer = kmalloc(rb_dev.capacity, GFP_KERNEL);
    if (!rb_dev.buffer) {
        device_destroy(avionics_class, dev_id);
        class_destroy(avionics_class);
        cdev_del(&avionics_cdev);
        unregister_chrdev_region(dev_id, 1);
        return -ENOMEM;
    }

    rb_dev.head = 0;
    rb_dev.tail = 0;
    rb_dev.count = 0;
    mutex_init(&rb_dev.lock);
    init_waitqueue_head(&rb_dev.read_queue);
    init_waitqueue_head(&rb_dev.write_queue);

    rb_dev.stats.capacity = DEFAULT_BUFFER_SIZE;
    rb_dev.stats.bytes_used = 0;
    rb_dev.stats.bytes_free = DEFAULT_BUFFER_SIZE;
    rb_dev.stats.total_writes = 0;
    rb_dev.stats.total_reads = 0;
    rb_dev.stats.overrun_count = 0;

    pr_info("avionics_ringbuf: Successfully loaded! /dev/%s created (Major: %d)\n", DEVICE_NAME, major_number);
    return 0;
}

static void __exit avionics_driver_exit(void) {
    dev_t dev_id = MKDEV(major_number, 0);

    kfree(rb_dev.buffer);
    device_destroy(avionics_class, dev_id);
    class_destroy(avionics_class);
    cdev_del(&avionics_cdev);
    unregister_chrdev_region(dev_id, 1);

    pr_info("avionics_ringbuf: Unloaded driver successfully\n");
}

module_init(avionics_driver_init);
module_exit(avionics_driver_exit);
