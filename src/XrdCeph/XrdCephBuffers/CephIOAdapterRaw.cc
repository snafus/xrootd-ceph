#include "CephIOAdapterRaw.hh"
#include "../XrdCephPosix.hh"
#include "XrdOuc/XrdOucEnv.hh"

#include <iostream>

using namespace XrdCephBuffer;


CephIOAdapterRaw::CephIOAdapterRaw(IXrdCephBufferData * bufferdata, int fd) : 
  m_bufferdata(bufferdata),m_fd(fd) {

}

CephIOAdapterRaw::~CephIOAdapterRaw() {

}

ssize_t CephIOAdapterRaw::write(off64_t offset,size_t count) {
    const void* buf = m_bufferdata->raw();
    if (!buf) return -EINVAL;
    ssize_t rc = ceph_posix_pwrite(m_fd,buf,count,offset);
    std::clog << "CephIOAdapterRaw::write " << offset << " " << count << " " << rc << std::endl;
    return rc;
}
ssize_t CephIOAdapterRaw::read(off64_t offset, size_t count) {
    void* buf = m_bufferdata->raw();
    if (!buf) {
      return -EINVAL;
    }

    ssize_t rc = ceph_posix_pread(m_fd,buf,count,offset);
    std::clog << "CephIOAdapterRaw::read " << offset << " " << count << " " << rc << std::endl;
    if (rc>=0) {
      m_bufferdata->setLength(rc);
      m_bufferdata->setStartingOffset(offset);
      m_bufferdata->setValid(true);
    }
    return rc;
}

