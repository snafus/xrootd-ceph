#ifndef __CEPH_BUFFER_UTILS_HH__
#define __CEPH_BUFFER_UTILS_HH__

// holder of various small utility classes for debugging, profiling, logging, and general stuff

#include <atomic>
#include <chrono>


namespace XrdCephBuffer {

class Timer_ns {
    public:
        explicit Timer_ns(long & output);
        ~Timer_ns();

    private:
        std::chrono::steady_clock::time_point m_start;
        long &m_output_val;

    // auto start = std::chrono::steady_clock::now();
    // std::copy( itstart+offset, itstart+(offset+readlength), (char*)buf);
    // auto end = std::chrono::steady_clock::now();
    // auto int_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end-start);

};






}

#endif
