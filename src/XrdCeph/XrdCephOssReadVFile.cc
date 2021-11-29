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
#include <memory>
#include <algorithm>
#include <fcntl.h>

#include "XrdCeph/XrdCephPosix.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdOuc/XrdOucTrace.hh"
#include "XrdSfs/XrdSfsAio.hh"
#include "XrdCeph/XrdCephOssFile.hh"

#include "XrdCeph/XrdCephOssReadVFile.hh"
#include "XrdCeph/XrdCephBuffers/BufferUtils.hh"

using namespace XrdCephBuffer;

extern XrdSysError XrdCephEroute;

XrdCephOssReadVFile::XrdCephOssReadVFile(XrdCephOss *cephoss,XrdCephOssFile *cephossDF):
XrdCephOssFile(cephoss), m_cephoss(cephoss), m_xrdOssDF(cephossDF)
{
    if (!m_xrdOssDF) XrdCephEroute.Say("XrdCephOssReadVFile::Null m_xrdOssDF");
}

XrdCephOssReadVFile::~XrdCephOssReadVFile() {
    XrdCephEroute.Say("XrdCephOssReadVFile::Destructor");

  if (m_xrdOssDF) {
    delete m_xrdOssDF;
    m_xrdOssDF = nullptr;
  }

}

int XrdCephOssReadVFile::Open(const char *path, int flags, mode_t mode, XrdOucEnv &env) {
  std::stringstream msg; 
  msg << "XrdCephOssReadVFile::Opening" ; 
  XrdCephEroute.Say(msg.str().c_str());
  msg.flush();

  int rc = m_xrdOssDF->Open(path, flags, mode, env);
  if (rc < 0) {
    return rc;
  }
  m_fd = m_xrdOssDF->getFileDescriptor();
  std::stringstream msg2; 
  msg2 << "XrdCephOssReadVFile::Open: fd: " << m_xrdOssDF->getFileDescriptor(); 
  XrdCephEroute.Say(msg2.str().c_str());

  return rc;
}

int XrdCephOssReadVFile::Close(long long *retsz) {
  return m_xrdOssDF->Close(retsz);
}


ssize_t XrdCephOssReadVFile::ReadV(XrdOucIOVec *readV, int rnum) {
  int fd = m_xrdOssDF->getFileDescriptor();
  std::stringstream msg; 
  msg << "XrdCephOssReadVFile::ReadV: fd: " << fd  << " " << rnum << "\n";
  XrdCephEroute.Say(msg.str().c_str()); msg.clear();

  ExtentHolder extents(rnum);
  for (int i = 0; i < rnum; i++) {
    extents.push_back(Extent(readV[i].offset, readV[i].size));
  }
  msg.clear(); 
  msg << "Extents: fd: "<< fd << " "  << extents.size() << " " << extents.len() << " " 
      << extents.begin() << " " << extents.end() << " " << extents.bytesContained() 
      << " " << extents.bytesMissing();
  XrdCephEroute.Say(msg.str().c_str());

  //std::unique_ptr<XrdCephBuffer::IXrdCephReadVAdapter> readVAdapter(new XrdCephBuffer::XrdCephReadVNoOp());
  std::unique_ptr<XrdCephBuffer::IXrdCephReadVAdapter> readVAdapter(new XrdCephBuffer::XrdCephReadVBasic());

  std::vector<ExtentHolder> mappedExtents = readVAdapter->convert(extents);

  size_t totalBytesRead(0), totalBytesUseful(0);

  int nbytes = 0, curCount = 0, counter(0);

  std:: clog << "mappedExtents: " << mappedExtents.size() << std::endl;
  for (std::vector<ExtentHolder>::const_iterator ehit = mappedExtents.cbegin(); ehit!= mappedExtents.cend(); ++ehit ) {
    off_t off = ehit->begin();
    size_t len = ehit->len();

    std:: clog << "outerloop: " << off << " " << len << " " << ehit->end() << " " << " " << ehit->size() << std::endl;

    // read the full extent into the buffer
    // #fixme
    std::vector<char> buffer;
    buffer.reserve(len);
    curCount = m_xrdOssDF->Read(buffer.data(), off, len);
    std:: clog << "buf Read " << curCount << std::endl;
    if (curCount != (ssize_t)len) {
      return (curCount < 0 ? curCount : -ESPIPE);
    }
    totalBytesRead += curCount;
    totalBytesUseful += ehit->bytesContained(); 

    // now read out into the original readV requests
    const char* data = buffer.data();
    const ExtentContainer& innerExtents = ehit->extents();
    for (ExtentContainer::const_iterator it = innerExtents.cbegin(); it != innerExtents.cend(); ++it) {
      off_t innerBegin = it->begin() - off;
      off_t innerEnd   = it->end()   - off; 
      std:: clog << "innerloop: " << innerBegin << " " << innerEnd << " " << off << " " << it->begin() << " " << it-> end() << " " << readV[counter].offset << " " << readV[counter].size <<  std::endl;
      std::copy(data+innerBegin, data+innerEnd, readV[counter].data);
      nbytes += it->len();
      ++counter; // next element
    } // inner extents

  } // outer extents
  std::clog << "readV returning " << nbytes << " bytes: " << "Read:  " <<totalBytesRead << " Useful: " << totalBytesUseful  << endl;
  return nbytes;

    // int nbytes = 0, curCount = 0;

    // for (int i = 0; i < rnum; i++)
    // {
    //   curCount = m_xrdOssDF->Read(readV[i].data, readV[i].offset, readV[i].size);
    //   if (curCount != readV[i].size)
    //     return (curCount < 0 ? curCount : -ESPIPE);
    //   nbytes += curCount;
    // }
    // return nbytes;
}

ssize_t XrdCephOssReadVFile::Read(off_t offset, size_t blen) {
  return m_xrdOssDF->Read(offset,blen);
}

ssize_t XrdCephOssReadVFile::Read(void *buff, off_t offset, size_t blen) {
  return m_xrdOssDF->Read(buff,offset,blen);
}

int XrdCephOssReadVFile::Read(XrdSfsAio *aiop) {
  return m_xrdOssDF->Read(aiop);
}

ssize_t XrdCephOssReadVFile::ReadRaw(void *buff, off_t offset, size_t blen) {
  return m_xrdOssDF->ReadRaw(buff, offset, blen);
}

int XrdCephOssReadVFile::Fstat(struct stat *buff) {
  return m_xrdOssDF->Fstat(buff);
}

ssize_t XrdCephOssReadVFile::Write(const void *buff, off_t offset, size_t blen) {
  return m_xrdOssDF->Write(buff,offset,blen);
}

int XrdCephOssReadVFile::Write(XrdSfsAio *aiop) {
  return m_xrdOssDF->Write(aiop);
}

int XrdCephOssReadVFile::Fsync() {
  return m_xrdOssDF->Fsync();
}

int XrdCephOssReadVFile::Ftruncate(unsigned long long len) {
  return m_xrdOssDF->Ftruncate(len);
}
