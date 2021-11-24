#ifndef __XRD_CEPH_BUFFER_DATA_SIMPLE_HH__
#define __XRD_CEPH_BUFFER_DATA_SIMPLE_HH__
//------------------------------------------------------------------------------
//! is a simple implementation of IXrdCephBufferData using std::vector<char> representation for the buffer
//------------------------------------------------------------------------------

#include <sys/types.h> 
#include "IXrdCephBufferData.hh"

#include <vector>

namespace XrdCephBuffer {

class XrdCephBufferDataSimple :  public virtual IXrdCephBufferData
 {
    public:
        XrdCephBufferDataSimple(size_t bufCapacity);
        virtual ~XrdCephBufferDataSimple();
        virtual size_t capacity() const override;//! total available space
        virtual size_t length() const  override;//! Currently occupied and valid space, which may be less than capacity
        virtual void   setLength(size_t len) override;//! Currently occupied and valid space, which may be less than capacity
        virtual bool isValid() const override;
        virtual void setValid(bool isValid) override;

        virtual  off_t startingOffset() const override;
        virtual  off_t setStartingOffset(off_t offset) override;


        virtual ssize_t readBuffer(void* buf, off_t offset, size_t blen) const override; //! copy data from the internal buffer to buf

        virtual ssize_t invalidate() override; //! set cache into an invalid state; do this before writes to be consistent
        virtual ssize_t writeBuffer(const void* buf, off_t offset, size_t blen, off_t externalOffset=0) override; //! write data into the buffer, store the external offset if provided

        virtual const void* raw() const override {return capacity() > 0 ? &(m_buffer[0]) : nullptr;}
        virtual void* raw() override {return capacity() > 0 ? &(m_buffer[0]) : nullptr;}


    protected:
        bool m_valid = false;
        std::vector<char> m_buffer; // actual physical buffer
        off_t m_externalOffset = 0; //! what does the first byte of the buffer map to for external offsets
        size_t m_bufLength = 0;  //! length of valid stored data; might be less than the capacity

}; // XrdCephBufferDataSimple

} // namespace 
#endif 
