#include "CephIOAdapterRaw.hh"
#include "../XrdCephPosix.hh"
#include "XrdOuc/XrdOucEnv.hh"

#include <iostream>
#include <chrono>
#include <ratio>

using namespace XrdCephBuffer;

using myclock = std::chrono::steady_clock;
//using myseconds = std::chrono::duration<float,

CephIOAdapterRaw::CephIOAdapterRaw(IXrdCephBufferData * bufferdata, int fd) : 
  m_bufferdata(bufferdata),m_fd(fd) {

}

CephIOAdapterRaw::~CephIOAdapterRaw() {
    std::clog << "CephIOAdapterRaw::Summary fd:" << m_fd 
              << " " << m_stats_write_req << " " << m_stats_write_bytes << " "
              << m_stats_write_timer*1e-3 << " " << m_stats_write_longest*1e-3
              << " " << m_stats_read_req << " " << m_stats_read_bytes << " "
               << m_stats_read_timer*1e-3  << "  " << m_stats_read_longest*1e-3
              << std::endl;

}

ssize_t CephIOAdapterRaw::write(off64_t offset,size_t count) {
    const void* buf = m_bufferdata->raw();
    if (!buf) return -EINVAL;

    auto start = std::chrono::steady_clock::now();
    ssize_t rc = ceph_posix_pwrite(m_fd,buf,count,offset);
    auto end = std::chrono::steady_clock::now();
    //std::chrono::duration<double> elapsed_seconds = end-start;
    //auto elapsed = end-start;
    //std::chrono::duration<float> elapsed{end-start};
    // integral duration: requires duration_cast
    auto int_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end-start);

    std::clog << "CephIOAdapterRaw::write fd:" << m_fd << " " 
              <<  offset << " " << count << " " << rc << " " << int_ms.count() << std::endl;

    if (rc < 0) return rc;
    m_stats_write_longest = std::max(m_stats_write_longest,int_ms.count()); 
    m_stats_write_timer.fetch_add(int_ms.count());
    m_stats_write_bytes.fetch_add(rc);
    ++m_stats_write_req;
    return rc;
}


ssize_t CephIOAdapterRaw::read(off64_t offset, size_t count) {
    void* buf = m_bufferdata->raw();
    if (!buf) {
      return -EINVAL;
    }

    auto start = std::chrono::steady_clock::now();
    ssize_t rc = ceph_posix_pread(m_fd,buf,count,offset);
    auto end = std::chrono::steady_clock::now();
    //auto elapsed = end-start;
    auto int_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end-start);

    if (rc < 0) return rc;

    m_stats_read_longest = std::max(m_stats_read_longest,int_ms.count()); 
    m_stats_read_timer.fetch_add(int_ms.count());
    m_stats_read_bytes.fetch_add(rc);
    ++m_stats_read_req;

    std::clog << "CephIOAdapterRaw::read fd:" << m_fd << " " << offset
             << " " << count << " " << rc << " " << int_ms.count()  << std::endl;

    if (rc>=0) {
      m_bufferdata->setLength(rc);
      m_bufferdata->setStartingOffset(offset);
      m_bufferdata->setValid(true);
    }
    return rc;
}

