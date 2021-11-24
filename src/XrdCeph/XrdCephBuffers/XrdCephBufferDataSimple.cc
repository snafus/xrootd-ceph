//------------------------------------------------------------------------------
//! is a simple implementation of IXrdCephBufferData using std::vector<char> representation for the buffer
//------------------------------------------------------------------------------

#include "XrdCephBufferDataSimple.hh"
//#include "XrdCeph/XrdCephBuffers/IXrdCephBufferData.hh"
#include <errno.h>
#include <algorithm>
#include <iostream>

using  namespace XrdCephBuffer;


XrdCephBufferDataSimple::XrdCephBufferDataSimple(size_t bufCapacity):
 m_buffer(bufCapacity,0), m_externalOffset(0),m_bufLength(0) {
    m_valid = true;
}

XrdCephBufferDataSimple::~XrdCephBufferDataSimple() {
    m_valid = false;
    m_buffer.clear();
    m_buffer.reserve(0); // just to be paranoid and realse memory immediately
}


size_t XrdCephBufferDataSimple::capacity() const {
    return m_buffer.capacity();
}

size_t XrdCephBufferDataSimple::length() const   {
    return m_bufLength;
}
void   XrdCephBufferDataSimple::setLength(size_t len) {
    m_bufLength = len;
}
bool XrdCephBufferDataSimple::isValid() const {
    return m_valid;
}
void XrdCephBufferDataSimple::setValid(bool isValid) {
    m_valid = isValid;
}


off_t XrdCephBufferDataSimple::startingOffset() const  {
    return m_externalOffset;
}
off_t XrdCephBufferDataSimple::setStartingOffset(off_t offset) {
    m_externalOffset = offset;
    return m_externalOffset;
}

ssize_t XrdCephBufferDataSimple::invalidate() {
    m_externalOffset = 0;
    m_bufLength      = 0;
    m_valid = false;
    //m_buffer.clear();  // do we really need to clear the elements ?
    return 0;
}



ssize_t XrdCephBufferDataSimple::readBuffer(void* buf, off_t offset, size_t blen) const {
    // read from the internal buffer to buf (at pos 0), from offset for blen, or max length possible
    // returns -ve value on error, else the actual number of bytes read

    if (!m_valid) {
        return -1;
    }
    if (offset < 0) {
        return -1;
    }
    if (offset > (ssize_t) m_bufLength) {
        return 0;
    }
    ssize_t readlength = blen;
    if (offset + blen > m_bufLength) {
        readlength = m_bufLength - offset;
    }
    //std::cout << readlength << " " << blen << " " << m_bufLength << " " << offset << std::endl;
    if (readlength <0) {
        return -1;
    }

    if (readlength == 0) {
        return 0;
    }
    
    std::vector<char>::const_iterator itstart = m_buffer.cbegin();

    std::copy( itstart+offset, itstart+(offset+readlength), (char*)buf);
    return readlength;
}


ssize_t XrdCephBufferDataSimple::writeBuffer(const void* buf, off_t offset, size_t blen, off_t externalOffset) {
    // write data from buf (from pos 0), with length blen, into the buffer at position offset (local to the internal buffer)
    
    // #TODO Add test to see if it's in use
    //invalidate();

    if (offset < 0) {
        return -1;
    }

    ssize_t cap = capacity();
    if ((ssize_t)blen > cap) {
        return -1;
    }
    if ((ssize_t)offset > cap) {
        return -1;
    }
    if (ssize_t(offset + blen) > cap) {
        return -1;
    }

    std::vector<char>::iterator itstart = m_buffer.begin();
    size_t readBytes = blen;

    std::copy((char*)buf, (char*)buf +readBytes ,itstart + offset );

    m_externalOffset = externalOffset;
    // Decide to set the length of the maximum value that has be written
    // note; unless invalidate is called, then this value may not be correctly set ... 
    m_bufLength      = std::max(offset+blen, m_bufLength);
    m_valid          = true;


    return readBytes;
} 
