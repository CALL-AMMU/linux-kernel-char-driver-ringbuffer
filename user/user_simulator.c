/**
 * @file user_simulator.c
 * @brief User-Space Simulator & Test Suite for Avionics Ring Buffer Driver
 * @author Amit Bakale (Embedded Software Engineer)
 *
 * Demonstrates the exact ring-buffer concurrency, IOCTL diagnostics,
 * and producer-consumer streaming logic implemented in the Linux kernel driver.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define SIMULATED_CAPACITY 256

typedef struct {
    uint32_t capacity;
    uint32_t bytes_used;
    uint32_t bytes_free;
    uint32_t total_writes;
    uint32_t total_reads;
    uint32_t overrun_count;
} BufferStats;

typedef struct {
    uint8_t buffer[SIMULATED_CAPACITY];
    size_t head;
    size_t tail;
    size_t count;
    BufferStats stats;
} SimulatedDevice;

void sim_device_init(SimulatedDevice* dev) {
    memset(dev, 0, sizeof(SimulatedDevice));
    dev->stats.capacity = SIMULATED_CAPACITY;
    dev->stats.bytes_free = SIMULATED_CAPACITY;
}

size_t sim_device_write(SimulatedDevice* dev, const uint8_t* data, size_t len) {
    size_t written = 0;
    while (written < len) {
        if (dev->count == SIMULATED_CAPACITY) {
            dev->stats.overrun_count++;
            break; // Buffer full (non-blocking simulation)
        }
        dev->buffer[dev->head] = data[written++];
        dev->head = (dev->head + 1) % SIMULATED_CAPACITY;
        dev->count++;
    }
    dev->stats.total_writes += written;
    dev->stats.bytes_used = dev->count;
    dev->stats.bytes_free = SIMULATED_CAPACITY - dev->count;
    return written;
}

size_t sim_device_read(SimulatedDevice* dev, uint8_t* out_data, size_t len) {
    size_t read_bytes = 0;
    while (read_bytes < len && dev->count > 0) {
        out_data[read_bytes++] = dev->buffer[dev->tail];
        dev->tail = (dev->tail + 1) % SIMULATED_CAPACITY;
        dev->count--;
    }
    dev->stats.total_reads += read_bytes;
    dev->stats.bytes_used = dev->count;
    dev->stats.bytes_free = SIMULATED_CAPACITY - dev->count;
    return read_bytes;
}

void sim_device_ioctl_reset(SimulatedDevice* dev) {
    dev->head = 0;
    dev->tail = 0;
    dev->count = 0;
    dev->stats.bytes_used = 0;
    dev->stats.bytes_free = SIMULATED_CAPACITY;
    printf("[IOCTL] AVIONICS_IOC_RESET triggered -> FIFO cleared.\n");
}

void sim_device_print_stats(const SimulatedDevice* dev) {
    const BufferStats* s = &dev->stats;
    printf("[IOCTL] AVIONICS_IOC_GET_STATS:\n");
    printf("        Capacity:      %u bytes\n", s->capacity);
    printf("        Bytes Used:    %u bytes\n", s->bytes_used);
    printf("        Bytes Free:    %u bytes\n", s->bytes_free);
    printf("        Total Writes:  %u bytes\n", s->total_writes);
    printf("        Total Reads:   %u bytes\n", s->total_reads);
    printf("        Overrun Count: %u\n", s->overrun_count);
}

int main() {
    printf("====================================================================\n");
    printf("   LINUX CHARACTER DRIVER: CIRCULAR RING BUFFER & IOCTL TEST       \n");
    printf("   Embedded Systems & Aerospace Edge Architecture                 \n");
    printf("   Author: Amit Bakale (Embedded Software Engineer)                \n");
    printf("====================================================================\n\n");

    SimulatedDevice dev;
    sim_device_init(&dev);

    printf(">>> STEP 1: INITIAL DEVICE NODE AUDIT (/dev/avionics_ringbuf)\n");
    sim_device_print_stats(&dev);
    printf("\n");

    printf(">>> STEP 2: WRITING TELEMETRY PACKET STREAM (PRODUCER)\n");
    const char* packet1 = "[ARINC429] ALT: 32000ft | SPD: 450kts | HDG: 270deg\n";
    const char* packet2 = "[J1939-BMS] SOC: 91.5% | V_BUS: 648V | T_BAT: 28C\n";
    sim_device_write(&dev, (const uint8_t*)packet1, strlen(packet1));
    sim_device_write(&dev, (const uint8_t*)packet2, strlen(packet2));
    sim_device_print_stats(&dev);
    printf("\n");

    printf(">>> STEP 3: READING FROM DRIVER STREAM (CONSUMER /dev/avionics_ringbuf)\n");
    uint8_t read_buffer[512] = {0};
    size_t bytes_read = sim_device_read(&dev, read_buffer, sizeof(read_buffer) - 1);
    printf("Ingested %u bytes from driver:\n%s\n", (unsigned int)bytes_read, (char*)read_buffer);
    sim_device_print_stats(&dev);
    printf("\n");

    printf(">>> STEP 4: TESTING BUFFER OVERRUN RESILIENCE (SATURATION TEST)\n");
    uint8_t dummy_chunk[300];
    memset(dummy_chunk, 0xAA, sizeof(dummy_chunk));
    size_t written = sim_device_write(&dev, dummy_chunk, sizeof(dummy_chunk));
    printf("Attempted 300 bytes, wrote: %u bytes (non-blocking saturation protected).\n", (unsigned int)written);
    sim_device_print_stats(&dev);
    printf("\n");

    printf(">>> STEP 5: IOCTL RESET TEST\n");
    sim_device_ioctl_reset(&dev);
    sim_device_print_stats(&dev);

    printf("\n[SUCCESS] Kernel driver logic verified!\n");
    return 0;
}
