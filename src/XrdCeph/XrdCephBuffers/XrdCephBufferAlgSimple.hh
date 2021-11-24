#ifndef __XRD_CEPH_BUFFER_ALG_SIMPLE_HH__
#define __XRD_CEPH_BUFFER_ALG_SIMPLE_HH__
//------------------------------------------------------------------------------
// Implementation of the logic section of buffer code
//------------------------------------------------------------------------------

#include <sys/types.h> 
#include <mutex>

#include "IXrdCephBufferAlg.hh"
#include "ICephIOAdapter.hh"



namespace XrdCephBuffer {

class XrdCephBufferAlgSimple : public virtual  IXrdCephBufferAlg {
    public:
        XrdCephBufferAlgSimple(IXrdCephBufferData * buffer, ICephIOAdapter * cephio, int fd ); 
        virtual ~XrdCephBufferAlgSimple();

        virtual ssize_t read_aio (XrdSfsAio *aoip) override;
        virtual ssize_t write_aio(XrdSfsAio *aoip) override;


        virtual ssize_t read (volatile void *buff, off_t offset, size_t blen) override;
        virtual ssize_t write(const void *buff, off_t offset, size_t blen) override;
        virtual ssize_t flushWriteCache() override; 

        virtual const IXrdCephBufferData *buffer() const {return m_bufferdata;}
        virtual IXrdCephBufferData *buffer() {return m_bufferdata;}

    protected:
        virtual ssize_t rawRead (void *buff,       off_t offset, size_t blen) ; // read from the storage, at its offset
        virtual ssize_t rawWrite(void *buff,       off_t offset, size_t blen) ; // write to the storage, to its offset posiiton

    private:
        IXrdCephBufferData * m_bufferdata = nullptr; //! this algorithm takes ownership of the buffer, and will delete it on destruction
        ICephIOAdapter * m_cephio = nullptr; // no ownership is taken here
        int m_fd = -1;

        off_t m_bufferStartingOffset = 0;
        size_t m_bufferLength = 0;

        std::recursive_mutex m_data_mutex; // any data access method on the buffer will use this
};  

}

#endif 
