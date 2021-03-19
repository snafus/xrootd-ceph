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

#include <string>
#include <vector>
#include <stdio.h>

#include "XrdCeph/XrdCephPosix.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdOuc/XrdOucTrace.hh"
#include "XrdSfs/XrdSfsAio.hh"

#include "XrdCeph/XrdCephOssFile.hh"
#include "XrdCeph/XrdCephOss.hh"


extern XrdSysError XrdCephEroute;

XrdCephOssFile::XrdCephOssFile(XrdCephOss *cephOss) : m_fd(-1), m_cephOss(cephOss) {}

int XrdCephOssFile::Open(const char *path, int flags, mode_t mode, XrdOucEnv &env) {
  try {
    int rc = ceph_posix_open(&env, path, flags, mode);
    if (rc < 0) return rc;
    m_fd = rc;
    return XrdOssOK;
  } catch (std::exception &e) {
    XrdCephEroute.Say("open : invalid syntax in file parameters");
    return -EINVAL;
  }
}

int XrdCephOssFile::Close(long long *retsz) {
  return ceph_posix_close(m_fd);
}

ssize_t XrdCephOssFile::Read(off_t offset, size_t blen) {
  return XrdOssOK;
}

ssize_t XrdCephOssFile::Read(void *buff, off_t offset, size_t blen) {
  return ceph_posix_pread(m_fd, buff, blen, offset);
}

static void aioReadCallback(XrdSfsAio *aiop, size_t rc) {
  aiop->Result = rc;
  aiop->doneRead();
}

int XrdCephOssFile::Read(XrdSfsAio *aiop) {
  return ceph_aio_read(m_fd, aiop, aioReadCallback);
}

ssize_t XrdCephOssFile::ReadRaw(void *buff, off_t offset, size_t blen) {
  return Read(buff, offset, blen);
}

ssize_t XrdCephOssFile::ReadV(XrdOucIOVec *readV, int n)
{
  // Attempt to separate readV requests into a minimum set of read requests.
  // Separate each readV into corresponding chuncks of Read request
  // Make each read request, extract the neccessary bytes to put back into readV

  // first, find the associated CephFileRef for the file descrptor; we want the stripe info
  // for now, assume that 32k for each read request.

  const ssize_t sizeRead = 32 * 1024;
  char buffer[sizeRead]; // allocate read buffer
  XrdCephEroute.Say("JW: readV: ", std::to_string(n).c_str(), " in chunks of: ", std::to_string(sizeRead).c_str());

  off_t currentBlock = 0;
  std::vector<int> currentRequests;
  std::vector<off_t> currentOffsets;
  std::vector<ssize_t> currentSizes;
  std::vector<off_t> currentBufferOffset;


  unsigned int count_flushes = 0;
  unsigned int count_reads = 0;
  unsigned int count_memcopy = 0;
  unsigned int count_intrablock = 0;
  unsigned int count_crossblock = 0;
  unsigned int count_bigblock = 0;
  unsigned int count_readvitems = 0;

  ssize_t nbytes = 0;
  ssize_t flush_nbytes = 0;

  for (int i = 0; i < n; i++)
  {
    ++count_readvitems;
    // which block is the request in
    off_t blockId = readV[i].offset /  sizeRead;
    // what is the offset within the block;
    off_t blockOff = readV[i].offset % sizeRead;
    // what is the size of the request (which might extend over a block)
    ssize_t len = readV[i].size;

    XrdCephEroute.Say("JW: readV: In : Offset: ", std::to_string(readV[i].offset).c_str(), " size: ", std::to_string(readV[i].size).c_str());
    XrdCephEroute.Say("JW: readV: Out: blockId: ", std::to_string(blockId).c_str(), " blockOff: ", std::to_string(blockOff).c_str(), " size: ", std::to_string(len).c_str());


    if (i == 0)
    {
      XrdCephEroute.Say("Initial read block",  std::to_string(currentBlock).c_str() );
      currentBlock = blockId;
    }

    bool doFlush = false; // do we need to clear the results

    // all cases for a flush
    if (blockId != currentBlock)
    {
      XrdCephEroute.Say("blockId != currentBlock: ",  std::to_string(blockId).c_str() , " " , std::to_string(currentBlock).c_str() );
      doFlush = true;
    }
    if (len > sizeRead)
    {
      XrdCephEroute.Say("len > sizeRead: ",  std::to_string(len).c_str() , " >  " , std::to_string(sizeRead).c_str() );
      doFlush = true;
    }
    else if (len + blockOff > sizeRead)
    {
      XrdCephEroute.Say("len + blockOff > sizeRead: ",  std::to_string(len + blockOff).c_str(), " >  " , std::to_string(sizeRead).c_str() );
      doFlush = true;
    }

    if (doFlush && currentRequests.size())
    {
      ssize_t  flush_curCount = 0;
      // read one block
      flush_curCount = Read((void *)buffer,
                            (off_t)(currentBlock * sizeRead),
                            (size_t)sizeRead);
      ++count_reads;
      XrdCephEroute.Say("Flush: ", std::to_string(flush_curCount).c_str() );
      if (flush_curCount < 0)
      {
        return flush_curCount;
        //return -ESPIPE;
      }
      flush_nbytes += flush_curCount;

      for (size_t iflush = 0; iflush < currentRequests.size(); ++iflush)
      {
        size_t irequest = currentRequests.at(iflush);
        XrdCephEroute.Say("memcpy: ", std::to_string(irequest).c_str(), " " , std::to_string(currentBufferOffset[irequest])).c_str(), " " , 
                                      std::to_string(currentSizes[irequest])).c_str(), " " , std::to_string(iflush).c_str(), " "  );

        memcpy((void *)readV[irequest].data, (void *)(buffer + currentBufferOffset[irequest]), (size_t)currentSizes[irequest]);
        nbytes += currentSizes[irequest];
      }
      ++count_flushes;
      currentRequests.clear();
      currentOffsets.clear();
      currentSizes.clear();
      currentBufferOffset.clear();
    }

    if (blockId != currentBlock)
    {
      // reset currentl block
      currentBlock = blockId;
    }

    if (len > sizeRead)
    {
      // request is larger than one block size
      //flush old requests
      //FLUSH
      // make this request as a single request (might not be any more efficiecnt ... )
      ++count_bigblock;
      currentRequests.push_back(i);
      currentOffsets.push_back(blockOff);
      currentSizes.push_back(len);
      currentBufferOffset.push_back ( currentBufferOffset.size() ? currentBufferOffset.back() + currentSizes.back()  : 0); // starting offset in temp buffer
      // on next loop, blockId will not be current block, and so will flush
    }
    else if (len + blockOff > sizeRead)
    {
      // request crosses block boundary, but is smaller that one whole block
      //FLUSH
      ++count_crossblock;
      currentRequests.push_back(i);
      currentOffsets.push_back(blockOff);
      currentSizes.push_back(len);
      currentBufferOffset.push_back ( currentBufferOffset.size() ? currentBufferOffset.back() + currentSizes.back()  : 0); // starting offset in temp buffer
      // on next loop, blockId will not be current block, and so will flush
    }
    else
    {
      // request falls within one block
      ++count_intrablock;
      currentRequests.push_back(i);
      currentOffsets.push_back(blockOff);
      currentSizes.push_back(len);
      currentBufferOffset.push_back ( currentBufferOffset.size() ? currentBufferOffset.back() + currentSizes.back()  : 0); // starting offset in temp buffer
    }
  } // end of for loop

  // might still have unflushed data read,
  if (currentRequests.size())
  {
    ssize_t  flush_curCount = 0;
    // read one block
    flush_curCount = Read((void *)buffer,
                          (off_t)(currentBlock*sizeRead),
                          (size_t)sizeRead);
    XrdCephEroute.Say("FlushEnd: ", std::to_string(flush_curCount).c_str() );

    ++count_reads;
    if (flush_curCount < 0)
    {
      return flush_curCount;
      //return -ESPIPE;
    }
    flush_nbytes += flush_curCount;

    for (size_t iflush = 0; iflush < currentRequests.size(); ++iflush)
    {
      size_t irequest = currentRequests.at(iflush);
      XrdCephEroute.Say("memcpyEnd: ", std::to_string(irequest).c_str(), " " , std::to_string(currentBufferOffset[irequest])).c_str(), " " , 
                                       std::to_string(currentSizes[irequest])).c_str(), " " , std::to_string(iflush).c_str(), " "  );
      memcpy((void *)readV[irequest].data, (void *)(buffer + currentBufferOffset[irequest]), (size_t)currentSizes[irequest]);
      nbytes += currentSizes[irequest];
      ++count_memcopy;
    }
    ++count_flushes;
    currentRequests.clear();
    currentOffsets.clear();
    currentSizes.clear();
    currentBufferOffset.clear();
  } // end of final flush

  //  ssize_t nbytes = 0, curCount = 0;
  //  for (int i=0; i<n; i++)
  //      {curCount = Read((void *)readV[i].data,
  //                        (off_t)readV[i].offset,
  //                       (size_t)readV[i].size);
  //       if (curCount != readV[i].size)
  //          {if (curCount < 0) return curCount;
  //           return -ESPIPE;
  //          }
  //       nbytes += curCount;
  //      }


    XrdCephEroute.Say("JW: readV: count_readvitems ", std::to_string(count_readvitems).c_str());

    XrdCephEroute.Say("JW: readV: count_flushes ", std::to_string(count_flushes).c_str());
    XrdCephEroute.Say("JW: readV: count_reads ", std::to_string(count_reads).c_str());
    XrdCephEroute.Say("JW: readV: count_memcopy ", std::to_string(count_memcopy).c_str());
    XrdCephEroute.Say("JW: readV: count_intrablock ", std::to_string(count_intrablock).c_str());
    XrdCephEroute.Say("JW: readV: count_crossblock ", std::to_string(count_crossblock).c_str());
    XrdCephEroute.Say("JW: readV: count_bigblock ", std::to_string(count_bigblock).c_str());

    XrdCephEroute.Say("JW: readV: flush_nbytes ", std::to_string(flush_nbytes).c_str());
    XrdCephEroute.Say("JW: readV:       nbytes ", std::to_string(nbytes).c_str());

      

  return nbytes;
}

int XrdCephOssFile::Fstat(struct stat *buff) {
  return ceph_posix_fstat(m_fd, buff);
}

ssize_t XrdCephOssFile::Write(const void *buff, off_t offset, size_t blen) {
  return ceph_posix_pwrite(m_fd, buff, blen, offset);
}

static void aioWriteCallback(XrdSfsAio *aiop, size_t rc) {
  aiop->Result = rc;
  aiop->doneWrite();
}

int XrdCephOssFile::Write(XrdSfsAio *aiop) {
  return ceph_aio_write(m_fd, aiop, aioWriteCallback);
}

int XrdCephOssFile::Fsync() {
  return ceph_posix_fsync(m_fd);
}

int XrdCephOssFile::Ftruncate(unsigned long long len) {
  return ceph_posix_ftruncate(m_fd, len);
}
