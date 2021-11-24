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

#ifndef __XRD_CEPH_OSS_BUFFERED_FILE_HH__
#define __XRD_CEPH_OSS_BUFFERED_FILE_HH__

#include "XrdOss/XrdOss.hh"
#include "XrdCeph/XrdCephOss.hh"

#include "XrdCeph/XrdCephBuffers/IXrdCephBufferData.hh"
#include "XrdCeph/XrdCephBuffers/IXrdCephBufferAlg.hh"
#include "XrdCeph/XrdCephBuffers/IXrdCephReadVAdapter.hh"


//------------------------------------------------------------------------------
//! Decorator class XrdCephOssBufferedFile designed to wrap XrdCephOssFile
//! Functionality for buffered access to/from data in Ceph to avoid inefficient
//! small reads / writes from the client side
//------------------------------------------------------------------------------

class XrdCephOssBufferedFile : public XrdOssDF {

public:

  XrdCephOssBufferedFile(XrdCephOssFile *cephossDF, size_t buffersize); 
  virtual ~XrdCephOssBufferedFile();
  virtual int Open(const char *path, int flags, mode_t mode, XrdOucEnv &env);
  virtual int Close(long long *retsz=0);
  virtual ssize_t Read(off_t offset, size_t blen);
  virtual ssize_t Read(void *buff, off_t offset, size_t blen);
  virtual int     Read(XrdSfsAio *aoip);
  virtual ssize_t ReadRaw(void *, off_t, size_t);
  virtual int Fstat(struct stat *buff);
  virtual ssize_t Write(const void *buff, off_t offset, size_t blen);
  virtual int Write(XrdSfsAio *aiop);
  virtual int Fsync(void);
  virtual int Ftruncate(unsigned long long);

protected:
  XrdCephOssFile * m_xrdOssDF = nullptr; // holder of the XrdCephOssFile instance
  XrdCephBuffer::IXrdCephBufferAlg * m_bufferAlg = nullptr;
  int m_flags = 0;
  size_t m_bufsize = 16*1024*1024L;
};

#endif /* __XRD_CEPH_OSS_BUFFERED_FILE_HH__ */
