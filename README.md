# High-Throughput Linux Kernel Character Device Driver with Circular Ring Buffer & Wait Queues

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://opensource.org/licenses/MIT)
[![Kernel: 5.x/6.x](https://img.shields.io/badge/Linux_Kernel-5.x%2F6.x_LTS-orange.svg)](https://kernel.org/)
[![Language: C99 / Kernel C](https://img.shields.io/badge/Language-C99%20%2F%20Kernel%20C-brightgreen.svg)]()
[![Avionics Telemetry](https://img.shields.io/badge/Avionics-High--Speed%20Telemetry-red.svg)]()

A production-grade **Linux Kernel Character Device Driver (`/dev/avionics_ringbuf`)** engineered for high-throughput, low-latency telemetry streaming between avionics data acquisition cards / radar sensors and real-time flight mission user-space applications.

---

## 🚀 Key Highlights & Architecture

- **True Linux Character Driver Subsystem (`cdev`)**: Implements dynamic major/minor allocation via `alloc_chrdev_region()`, dynamic sysfs device class creation (`class_create()`, `device_create()`), ensuring automatic `/dev/avionics_ringbuf` node creation by `udev`.
- **Circular Ring Buffer Engine**: Fixed-capacity ring buffer with zero dynamic reallocations in the critical datapath, preventing kernel memory fragmentation.
- **Sleep & Wakeup Concurrency Architecture**:
  - Implements Linux `wait_queue_head_t` for both write (`write_queue`) and read (`read_queue`).
  - Producers block interruptibly when the buffer is full (`wait_event_interruptible`).
  - Consumers block interruptibly when the buffer is empty.
  - Granular `mutex` locking (`mutex_lock_interruptible`) guarantees atomic buffer state transitions under multi-threaded concurrency.
- **Non-Blocking I/O Support**: Fully respects `O_NONBLOCK` file flags—immediately returns `-EAGAIN` if buffer limits are met without thread starvation.
- **`poll()` / `select()` / `epoll()` Integration**: Exposes `.poll` file operation using `poll_wait()`, enabling event-driven asynchronous user-space multiplexing (`POLLIN`, `POLLOUT`, `POLLRDNORM`, `POLLWRNORM`).
- **IOCTL Interface (`avionics_ringbuf.h`)**:
  - `AVIONICS_IOC_RESET`: Clears ring buffer pointers and drops stale data safely.
  - `AVIONICS_IOC_GET_STATS`: Returns telemetry metrics (`total_bytes_written`, `total_bytes_read`, `dropped_packets`, `buffer_utilization`).
- **Cross-Platform Verification Harness**: Includes a full standalone POSIX / Windows user simulator (`user_simulator.c`) validating producer-consumer concurrency, overflow rejection, and statistical telemetry.

---

## 📂 Repository Layout

```
linux-kernel-char-driver-ringbuffer/
├── driver/
│   ├── avionics_ringbuf.h    # Shared IOCTL definitions, magic numbers, telemetry structs
│   ├── avionics_ringbuf.c    # Linux Kernel C character device driver source
│   └── Makefile              # Kbuild Makefile for out-of-tree module compilation
├── user/
│   └── user_simulator.c      # Multi-producer/consumer verification test harness
├── build_simulator.bat       # 1-Click compiler for Windows/MinGW verification
├── PUSH_TO_GITHUB.bat        # Git deployment script
└── README.md                 # Complete technical documentation
```

---

## 🛠️ Linux Kernel Build & Deployment

To compile and load into a target Linux environment:

```bash
# 1. Compile kernel module using Kbuild against running kernel headers
cd driver/
make

# 2. Insert kernel module
sudo insmod avionics_ringbuf.ko

# 3. Check kernel dmesg log for dynamic major/minor assignment
dmesg | tail -n 10
# [avionics_ringbuf]: Module loaded. Device /dev/avionics_ringbuf created (Major: 240, Minor: 0)

# 4. Set user permissions (if not managed by udev rules)
sudo chmod 666 /dev/avionics_ringbuf

# 5. Check module unloading
sudo rmmod avionics_ringbuf
```

---

## 🧪 Simulation & Automated Verification

For cross-platform CI/CD and systems without root privileges, run the user-space simulator:

```bash
# Compile and run simulator
gcc -O2 -Wall user/user_simulator.c -o ringbuf_test.exe
./ringbuf_test.exe
```

### Verification Test Output:
```
==============================================================
   AVIONICS TELEMETRY KERNEL RING BUFFER SIMULATION
==============================================================
[INIT] Ring buffer initialized: capacity = 8192 bytes
[PRODUCER 1] Pushed 600 bytes (Utilization: 7.3%)
[PRODUCER 2] Pushed 800 bytes (Utilization: 17.0%)
[CONSUMER 1] Read 400 bytes (Utilization: 12.2%)
[OVERFLOW TEST] Attempting to push 9000 bytes into 8192 buffer...
[OVERFLOW REJECTED] Buffer full! Dropped packet count incremented correctly.
[IOCTL STATS] Written: 1400 B | Read: 400 B | Dropped: 1 | Current: 1000 B
[IOCTL RESET] Buffer flushed and pointers re-aligned to zero.
==============================================================
[PASS] All 6 Kernel Driver Logic Assertions Verified Successfully!
==============================================================
```

---

## 👨‍💻 Author

**Amit Bakale**  
Embedded Software & Avionics Engineer  
- Portfolio: [https://call-ammu.github.io/portfolio/](https://call-ammu.github.io/portfolio/)  
- LinkedIn: [https://www.linkedin.com/in/amitbakale/](https://www.linkedin.com/in/amitbakale/)  
- Email: bakaleamit28@gmail.com
