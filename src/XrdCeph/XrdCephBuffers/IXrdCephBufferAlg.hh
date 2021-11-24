#ifndef __IXRD_CEPH_BUFFER_ALG_HH__
#define __IXRD_CEPH_BUFFER_ALG_HH__
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

class XrdSfsAio;

namespace XrdCephBuffer {


class IXrdCephBufferAlg {
    public:
        virtual ~IXrdCephBufferAlg() {}

        virtual ssize_t read_aio (XrdSfsAio *aoip)  = 0;
        virtual ssize_t write_aio(XrdSfsAio *aoip) = 0;

        virtual ssize_t read (volatile void *buff, off_t offset, size_t blen)  = 0;
        virtual ssize_t write(const void *buff, off_t offset, size_t blen) = 0;
        virtual ssize_t flushWriteCache() = 0; 


    protected:
        

    private:

};

}

#endif 
