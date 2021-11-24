//------------------------------------------------------------------------------
// Copyright (c) 2014-2015 by European Organization for Nuclear Research (CERN)
// Author: Sebastien Ponce <sebastien.ponce@cern.ch>
//------------------------------------------------------------------------------
// This file is part of the XRootD software suite.
//
// XRootD is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// XRootD is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with XRootD.  If not, see <http://www.gnu.org/licenses/>.
//
// In applying this licence, CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.
//------------------------------------------------------------------------------

#include <sys/types.h>
#include <unistd.h>
#include <sstream>
#include <iostream>
#include <fcntl.h>

#include "XrdCeph/XrdCephPosix.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdOuc/XrdOucTrace.hh"
#include "XrdSfs/XrdSfsAio.hh"
#include "XrdCeph/XrdCephOssFile.hh"

#include "XrdCeph/XrdCephOssBufferedFile.hh"
#include "XrdCeph/XrdCephBuffers/XrdCephBufferAlgSimple.hh"
#include "XrdCeph/XrdCephBuffers/XrdCephBufferDataSimple.hh"
#include "XrdCeph/XrdCephBuffers/CephIOAdapterRaw.hh"

using namespace XrdCephBuffer;

extern XrdSysError XrdCephEroute;

XrdCephOssBufferedFile::XrdCephOssBufferedFile(XrdCephOssFile *cephossDF, size_t buffersize):
m_xrdOssDF(cephossDF), m_bufsize(buffersize) {
}

XrdCephOssBufferedFile::~XrdCephOssBufferedFile() {
  // call the destructor of wrapped and delegated classes:
  if (m_bufferAlg) {
    delete m_bufferAlg;
    m_bufferAlg = nullptr;
  }

  if (m_xrdOssDF) {
    delete m_xrdOssDF;
    m_xrdOssDF = nullptr;
  }

}


int XrdCephOssBufferedFile::Open(const char *path, int flags, mode_t mode, XrdOucEnv &env) {
  int rc = m_xrdOssDF->Open(path, flags, mode, env);
  if (rc < 0) {
    return rc;
  }
  m_flags = flags; // e.g. for write/read knowledge

  // opened a file, so create the buffer here; note - this might be better delegated elswhere ...
  // need the file descriptor, so do it after we know the file is opened 
  IXrdCephBufferData * cephbuffer = new XrdCephBufferDataSimple(m_bufsize);
  ICephIOAdapter * cephio =  new CephIOAdapterRaw(cephbuffer,m_xrdOssDF->getFileDescriptor()  );
  // TODO; check for deletion memory leaks
  m_bufferAlg = new XrdCephBufferAlgSimple(cephbuffer,cephio,m_xrdOssDF->getFileDescriptor());
  std::stringstream msg; 
  msg << "XrdCephOssBufferedFile::Open: Buffer created: " << cephbuffer->capacity();
  XrdCephEroute.Say(msg.str().c_str());


  return rc;
}

int XrdCephOssBufferedFile::Close(long long *retsz) {
  if ((m_flags & (O_WRONLY|O_RDWR)) != 0) {
    ssize_t rc = m_bufferAlg->flushWriteCache();
    if (rc < 0) {
        std::stringstream msg; 
        msg << "XrdCephOssBufferedFile::Close: flush ";
        XrdCephEroute.Say(msg.str().c_str());
        return rc;
    }
  } // check for write
  // TODO delete cephio, etc
  
  if (m_bufferAlg) {
    delete m_bufferAlg;
    m_bufferAlg = nullptr;
  }
  

  return m_xrdOssDF->Close(retsz);
}

ssize_t XrdCephOssBufferedFile::Read(off_t offset, size_t blen) {
  return m_xrdOssDF->Read(offset,blen);
}

ssize_t XrdCephOssBufferedFile::Read(void *buff, off_t offset, size_t blen) {
  std::stringstream msg; 
  msg << "XrdCephOssBufferedFile::Read: " <<  offset << "  " << blen;
  XrdCephEroute.Say(msg.str().c_str());
  //  return m_xrdOssDF->Read(buff,offset,blen);
  return m_bufferAlg->read(buff, offset, blen);
}

int XrdCephOssBufferedFile::Read(XrdSfsAio *aiop) {

  std::stringstream msg; msg << "XrdCephOssBufferedFile::AIOREAD: "  << time(nullptr) << " : " 
          << aiop->sfsAio.aio_offset << " " 
          << aiop->sfsAio.aio_nbytes << " " << aiop->sfsAio.aio_reqprio << " "
          << aiop->sfsAio.aio_fildes << " " ;
  XrdCephEroute.Say(msg.str().c_str());
  
  //return m_xrdOssDF->Read(aiop);
  return m_bufferAlg->read_aio(aiop);

}

ssize_t XrdCephOssBufferedFile::ReadRaw(void *buff, off_t offset, size_t blen) {
  return m_xrdOssDF->ReadRaw(buff, offset, blen);
}

int XrdCephOssBufferedFile::Fstat(struct stat *buff) {
  return m_xrdOssDF->Fstat(buff);
}

ssize_t XrdCephOssBufferedFile::Write(const void *buff, off_t offset, size_t blen) {
  //return m_xrdOssDF->Write(buff, offset,blen );
  std::stringstream msg; msg << "XrdCephOssBufferedFile::Write: " <<  offset << "  " << blen;
  XrdCephEroute.Say(msg.str().c_str());
  return m_bufferAlg->write(buff, offset, blen);
}

int XrdCephOssBufferedFile::Write(XrdSfsAio *aiop) {
  std::stringstream msg; msg << "XrdCephOssBufferedFile::AIOWRITE: "  << time(nullptr) << " : " 
          << aiop->sfsAio.aio_offset << " " 
          << aiop->sfsAio.aio_nbytes << " " << aiop->sfsAio.aio_reqprio << " "
          << aiop->sfsAio.aio_fildes << " " ;
  XrdCephEroute.Say(msg.str().c_str());
  //return m_xrdOssDF->Write(aiop); // pass through (slow) for now
  return m_bufferAlg->write_aio(aiop);
}

int XrdCephOssBufferedFile::Fsync() {
  return m_xrdOssDF->Fsync();
}

int XrdCephOssBufferedFile::Ftruncate(unsigned long long len) {
  return m_xrdOssDF->Ftruncate(len);
}
