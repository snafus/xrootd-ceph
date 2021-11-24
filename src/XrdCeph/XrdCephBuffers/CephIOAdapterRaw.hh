#ifndef __CEPH_IO_ADAPTER_RAW_HH__
#define __CEPH_IO_ADAPTER_RAW_HH__
//------------------------------------------------------------------------------
// Interface of the logic part of the buffering
// Intention to be able to abstract the underlying implementation and code against the inteface
// e.g. for different complexities of control.
// Couples loosely to IXrdCepgBufferData and anticipated to be called by XrdCephOssBufferedFile. 
// Should managage all of the IO and logic to give XrdCephOssBufferedFile only simple commands to call.
// implementations are likely to use (via callbacks?) CephPosix library code for actual reads and writes. 
//------------------------------------------------------------------------------

#include <sys/types.h> 
#include "IXrdCephBufferData.hh"
#include "ICephIOAdapter.hh"

namespace XrdCephBuffer {


class CephIOAdapterRaw: public  virtual ICephIOAdapter {
    public:
        CephIOAdapterRaw(IXrdCephBufferData * bufferdata, int fd);
        virtual ~CephIOAdapterRaw();

        virtual ssize_t write(off64_t offset,size_t count) override;
        virtual ssize_t read(off64_t offset,size_t count) override;

    private:
        IXrdCephBufferData * m_bufferdata; // no ownership of pointer (consider shared ptrs, etc)
        int m_fd;

};

}

#endif 
