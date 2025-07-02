// perf_collector.cpp
#include <linux/perf_event.h>  // For perf_event_attr and perf event definitions
#include <sys/ioctl.h>         // For ioctl() system call
#include <asm/unistd.h>        // For syscall numbers (__NR_perf_event_open)
#include <unistd.h>            // For syscall(), read(), close()
#include <iostream>            // For console output
#include <vector>              // For potential data storage
#include <cstring>            // For memset()
#include <thread>             // For sleep functionality
#include <chrono>             // For timestamps
#include <fstream>            // For file operations
#include <sstream>            // For string operations


//perf_event_open syscall wrapper, Wraps the Linux system call to access performance monitoring
static long perf_event_open(struct perf_event_attr *hw_event, pid_t pid,
                           int cpu, int group_fd, unsigned long flags) {
    return syscall(__NR_perf_event_open, hw_event, pid, cpu, group_fd, flags);
}


//Perf_counter class to manage performance counters

class PerfCounter {
public:
    PerfCounter(uint64_t type, uint64_t config) 
    {
        memset(&pe, 0, sizeof(pe));     // Zero out the structure
        pe.type = type;                 // Set event type (HARDWARE, SOFTWARE, etc.)
        pe.size = sizeof(pe);           // Set structure size
        pe.config = config;             // Set specific event (CPU_CYCLES, etc.)
        pe.disabled = 1;                // Start disabled
        pe.exclude_kernel = 1;          // Don't count kernel events, of no use in monitoring the application performance
        pe.exclude_hv = 1;              // Don't count hypervisor events
    }

   
   //The open() method is responsible for initializing the performance counter by making the perf_event_open() system call. 
    bool open(pid_t pid = 0, int cpu = -1, int group_fd = -1) {
        fd = perf_event_open(&pe, pid, cpu, group_fd, 0);
        return fd != -1;
    }

    
    //The start() function is part of the PerfCounter class and is responsible for preparing and enabling the performance counter for data collection. Here's a breakdown of what it does:
    void start() {
        ioctl(fd, PERF_EVENT_IOC_RESET, 0); //instruct the kernel to reset the counter , start fresh do not include previous measurements

        ioctl(fd, PERF_EVENT_IOC_ENABLE, 0); //enable the counter, start counting events
    }


    
    //The stop() function is part of the PerfCounter class and is responsible for disabling the performance counter, stopping it from tracking events.
    void stop() {
        ioctl(fd, PERF_EVENT_IOC_DISABLE, 0);
    }

    
    //The read() function is part of the PerfCounter class and is responsible for retrieving the current value of the performance counter. 
    uint64_t read() {

        uint64_t count; // This variable will hold the count of events recorded by the performance counter

        ssize_t n = ::read(fd, &count, sizeof(count)); //system call to read the current count from the file descriptor

        return (n == sizeof(count)) ? count : 0;
    }

    
    //The close() function is part of the PerfCounter class and is responsible for releasing the resources associated with the performance counter.
    void close() {
        if (fd != -1) {
            ::close(fd);
            fd = -1; // Reset the file descriptor to -1 to indicate it is closed
        }
    }

    ~PerfCounter() {
        close();
    }

private:
    struct perf_event_attr pe;
    int fd = -1;
};

void collect_data(const std::string& output_file) {
  
    // Create counters for different metrics instance of the class, each configured to monitor a specific hardware event.  
    PerfCounter cycles(PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES);
    PerfCounter instructions(PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS);
    PerfCounter cache_misses(PERF_TYPE_HARDWARE, PERF_COUNT_HW_CACHE_MISSES);
    PerfCounter branch_misses(PERF_TYPE_HARDWARE, PERF_COUNT_HW_BRANCH_MISSES);

    if (!cycles.open() || !instructions.open() || !cache_misses.open() || !branch_misses.open()) {
        std::cerr << "Error opening perf events" << std::endl;
        return;
    }

    std::ofstream outfile(output_file);
    if (!outfile.is_open()) {
        std::cerr << "Error opening output file" << std::endl;
        return;
    }

    // Write CSV header
    outfile << "timestamp,cycles,instructions,cache_misses,branch_misses,cpi\n";

    cycles.start();
    instructions.start();
    cache_misses.start();
    branch_misses.start();

    while (true) {
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();

        uint64_t cycles_count = cycles.read();
        uint64_t instr_count = instructions.read();
        uint64_t cache_miss_count = cache_misses.read();
        uint64_t branch_miss_count = branch_misses.read();

        // Calculate CPI (cycles per instruction)
        double cpi = (instr_count > 0) ? static_cast<double>(cycles_count) / instr_count : 0.0;

        // Write to CSV
        outfile << timestamp << ","
                << cycles_count << ","
                << instr_count << ","
                << cache_miss_count << ","
                << branch_miss_count << ","
                << cpi << "\n";
        outfile.flush();

        // Sleep for sampling interval (e.g., 100ms)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

int main() {
    std::string output_file = "/tmp/perf_data.csv";
    std::cout << "Starting perf data collection to " << output_file << std::endl;
    collect_data(output_file);
    return 0;
}
