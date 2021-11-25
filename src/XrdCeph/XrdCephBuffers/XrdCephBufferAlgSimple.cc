//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

#include <sys/types.h> 
#include "XrdCephBufferAlgSimple.hh"

#include "../XrdCephPosix.hh"
#include <XrdOuc/XrdOucEnv.hh>
#include <fcntl.h>
#include <sys/stat.h>
#include <iostream>

#include "XrdSfs/XrdSfsAio.hh"


using namespace XrdCephBuffer;


XrdCephBufferAlgSimple::XrdCephBufferAlgSimple(IXrdCephBufferData * buffer, ICephIOAdapter * cephio, int fd ):
m_bufferdata(buffer), m_cephio(cephio), m_fd(fd){

}

XrdCephBufferAlgSimple::~XrdCephBufferAlgSimple() {
        std::clog << "XrdCephBufferAlgSimple::Destructor fd:" << m_fd << std::endl;
    if (m_bufferdata) {
        delete m_bufferdata;
        m_bufferdata = nullptr;
    }
    if (m_cephio) {
        delete m_cephio;
        m_cephio = nullptr;
    }    
    m_fd = -1;
}


ssize_t XrdCephBufferAlgSimple::read_aio (XrdSfsAio *aoip) {
    return -ENOSYS;
    ssize_t rc(-ENOSYS);
    if (!aoip) {
        return -EINVAL; 
    }

    volatile void  * buf  = aoip->sfsAio.aio_buf;
    size_t blen  = aoip->sfsAio.aio_nbytes;
    off_t offset = aoip->sfsAio.aio_offset;

    // translate the aio read into a simple sync read.
    // hopefully don't get too many out of sequence reads to effect the caching
    rc = read(buf, offset, blen);

    aoip->Result = rc;
    aoip->doneRead();

    return rc;
}

ssize_t XrdCephBufferAlgSimple::write_aio(XrdSfsAio *aoip) {
    return -ENOSYS;
    ssize_t rc(-ENOSYS);
        if (!aoip) {
             return -EINVAL; 
         }

        // volatile void  * buf  = aoip->sfsAio.aio_buf;
        // size_t blen  = aoip->sfsAio.aio_nbytes;
        // off_t offset = aoip->sfsAio.aio_offset;
    size_t blen  = aoip->sfsAio.aio_nbytes;
    off_t offset = aoip->sfsAio.aio_offset;

    rc = write(const_cast<const void*>(aoip->sfsAio.aio_buf), offset, blen);
    aoip->Result = rc;
    aoip->doneWrite();
    return rc;
}


ssize_t XrdCephBufferAlgSimple::read(volatile void *buf,   off_t offset, size_t blen)  {
    const std::lock_guard<std::recursive_mutex> lock(m_data_mutex); // 

    std::clog << "XrdCephBufferAlgSimple::read: " << offset << " " << blen << std::endl;
    if (blen == 0) return 0;

    if (blen >= m_bufferdata->capacity()) {
        std::clog << "XrdCephBufferAlgSimple::read: Readthrough cache: fd: " << m_fd 
                  << " " << offset << " " << blen << std::endl;
        // larger than cache, so read through, and invalidate the cache anyway
        m_bufferdata->invalidate();
        // #FIXME JW: const_cast is probably a bit dangerous here!
        return ceph_posix_pread(m_fd, const_cast<void*>(buf), blen, offset);
    }

    ssize_t rc(-1);
    size_t bytesRemaining = blen;
    off_t offsetDelta = 0;
    size_t bytesRead = 0;
    while (bytesRemaining > 0) { // in principle, only should ever have the first loop
        bool loadCache = false;
        if (m_bufferLength == 0) {
            // no data in buffer
            loadCache = true;
        } else if (offset < m_bufferStartingOffset) {
            // offset before any cache data 
            loadCache = true;
        } else if (offset >=  (off_t) (m_bufferStartingOffset + m_bufferLength) ) {
            // offset is beyond the stored data
            loadCache = true;
        }

        // do we need to read data into the cache?
        if (loadCache) {
            m_bufferdata->invalidate();
            rc = m_cephio->read(offset + offsetDelta, m_bufferdata->capacity()); // fill the cache
            std::clog << "LoadCache ReadToCache: " << rc << " " << offset + offsetDelta << " " << m_bufferdata->capacity() << std::endl;
            if (rc < 0) {
                std::clog << "LoadCache Error: " << rc << std::endl;
                return rc;// TODO return correct errors
            }
            m_bufferStartingOffset = offset + offsetDelta;
            m_bufferLength = rc;
            if (rc == 0) {
                // perhaps at end of file and nothing to read?
                break;
            }
        }

        //now read as much data as possible
        off_t bufPosition = offset - m_bufferStartingOffset + offsetDelta; 
        rc =  m_bufferdata->readBuffer( (void*) &(((char*)buf)[offsetDelta]) , bufPosition  + offsetDelta , bytesRemaining);
        if (rc < 0 ) {
            std::clog << "Reading from Cache Failed: " << rc << "  " << offsetDelta << "  " << bytesRemaining << std::endl;
            return rc; // TODO return correct errors
        }
        if (rc == 0) {
            // no bytes returned; much be at end of file
            std::clog << "No bytes returned: " << rc << "  " << offset << " + " << offsetDelta << "; " << blen << " : " << bytesRemaining << std::endl;
            break; // leave the loop even though bytesremaing is probably >=0.
            //i.e. requested a full buffers worth, but only a fraction of the file is here.
        }

        std::clog << "End of loop: " << rc << "  " << offset << " + " << offsetDelta << "; " << blen << " : " << bytesRemaining << std::endl;
        offsetDelta    += rc; 
        bytesRemaining -= rc;
        bytesRead      += rc;

    } // while bytesremaing

    return bytesRead;
}

ssize_t XrdCephBufferAlgSimple::write (const void *buf, off_t offset, size_t blen) {
    const std::lock_guard<std::recursive_mutex> lock(m_data_mutex); // 

    // take the data in buf and put it into the cache; when the cache is full, write to underlying storage
    // remember to flush the cache at the end of operations ... 
    ssize_t rc(-1);
    ssize_t bytesWrittenToStorage(0);

    if (blen == 0) {
        return 0; // nothing to write; are we done?
    }

    off_t expected_offset = (off_t)(m_bufferStartingOffset + m_bufferLength);

    if ((offset != expected_offset) && (m_bufferLength > 0) ) {
        // TODO, might be dangerous to flush the cache on non-aligned writes ... 
        std::clog << "Non expected offset: " << rc << "  " << offset << "  " << expected_offset << std::endl;
        // rc = flushWriteCache();
        // if (rc < 0) {
        //     return rc; // TODO return correct errors
        // }
    } // mismatched offset

    if ( (m_bufferStartingOffset % m_bufferdata->capacity()) != 0 ) {
        std::clog << " Non aligned offset?" << m_bufferStartingOffset << " " <<  m_bufferdata->capacity() << " " <<  m_bufferStartingOffset % m_bufferdata->capacity() << std::endl;
    }

    // if (blen >= m_bufferdata->capacity()) {
    //     // TODO, might be dangerous to flush the cache on non-aligned writes ... 
    //     // flush the cache now, if needed
    //     rc = flushWriteCache();
    //     if (rc < 0) {
    //         return rc; // TODO return correct errors
    //     }
    //     bytesWrittenToStorage += rc;

    //     // Size is larger than the buffer; send the write straight through
    //     std::clog << "XrdCephBufferAlgSimple::write: Readthrough cache: fd: " << m_fd 
    //               << " " << offset << " " << blen << std::endl;
    //     // larger than cache, so read through, and invalidate the cache anyway
    //     m_bufferdata->invalidate();
    //     m_bufferLength=0;
    //     m_bufferStartingOffset=0;
    //     rc =  ceph_posix_pwrite(m_fd, buf, blen, offset);
    //     if (rc < 0) {
    //         return rc; // TODO return correct errors
    //     }
    //     bytesWrittenToStorage += rc;
    //     return rc;
    // }

    // if needed, we flushed the cache, but maybe the cache is already partly full    
    size_t bytesRemaining = blen;
    off_t  offsetDelta = 0;
    size_t bytesWritten = 0;
    off_t  bufferOffset = m_bufferLength; // position to append data in the buffer

    while (bytesRemaining > 0) { // expect that only one loop is perfomed, else likely to have written directly
        if (m_bufferLength == 0) {
            // empty cache, so set the external offset now
            m_bufferStartingOffset = offset + offsetDelta;
        }
        //add data to the cache
        rc = m_bufferdata->writeBuffer(buf, bufferOffset, bytesRemaining, 0);
        if (rc < 0) {
            std::clog << "WriteBuffer step failed: " << rc << "  " << bufferOffset << "  " << blen << "  " << offset << std::endl;
            return rc; // TODO return correct errors
        }

        m_bufferLength += rc;
        bufferOffset   += rc;
        bytesWritten   += rc;
        offsetDelta    += rc;
        bytesRemaining -= rc;

        if (m_bufferLength == m_bufferdata->capacity()) {
            rc = flushWriteCache();
            if (rc < 0) {
               return rc; // TODO return correct errors
            }
            bytesWrittenToStorage += rc;
        } // at capacity; 
        
    } // while byteRemaining
    std::clog << "WriteBuffer " << bytesWritten << " " << bytesWrittenToStorage << "  " << offset << "  " << blen << " " << std::endl;


    return bytesWritten;
}



ssize_t XrdCephBufferAlgSimple::flushWriteCache()  {
    const std::lock_guard<std::recursive_mutex> lock(m_data_mutex); // 
    std::clog << "flushWriteCache: " << m_bufferStartingOffset << " " << m_bufferLength << std::endl; 
    ssize_t rc(-1);
    if (m_bufferLength == 0 ) {
        return 0; // nothing to be written
    }
    // #TODO; the actual write
    rc = m_cephio->write(m_bufferStartingOffset, m_bufferLength);
    if (rc < 0) {
        std::clog << "WriteBuffer write step failed: " << rc << std::endl;
        return rc;  // TODO return correct errors
    }
    m_bufferLength=0;
    m_bufferStartingOffset=0;
    m_bufferdata->invalidate();
    return rc;
}



ssize_t XrdCephBufferAlgSimple::rawRead (void *buf,       off_t offset, size_t blen) {
    return -ENOSYS;
}

ssize_t XrdCephBufferAlgSimple::rawWrite(void *buf,       off_t offset, size_t blen) {
    return -ENOSYS;
}