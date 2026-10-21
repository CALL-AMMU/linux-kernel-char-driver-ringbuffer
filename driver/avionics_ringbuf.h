#ifndef AVIONICS_RINGBUF_H
#define AVIONICS_RINGBUF_H

#ifdef __KERNEL__
#include <linux/ioctl.h>
#include <linux/types.h>
#else
#include <sys/ioctl.h>
#include <stdint.h>
#endif

#define AVIONICS_IOC_MAGIC 'A'

struct avionics_buffer_stats {
    uint32_t capacity;
    uint32_t bytes_used;
    uint32_t bytes_free;
    uint32_t total_writes;
    uint32_t total_reads;
    uint32_t overrun_count;
};

// IOCTL Commands
#define AVIONICS_IOC_RESET       _IO(AVIONICS_IOC_MAGIC, 0)
#define AVIONICS_IOC_GET_STATS   _IOR(AVIONICS_IOC_MAGIC, 1, struct avionics_buffer_stats)
#define AVIONICS_IOC_SET_BLOCK   _IOW(AVIONICS_IOC_MAGIC, 2, int)

#endif // AVIONICS_RINGBUF_H
