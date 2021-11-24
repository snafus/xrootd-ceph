#ifndef __IXRD_CEPH_READV_ADAPTER_HH__
#define __IXRD_CEPH_READV_ADAPTER_HH__
//------------------------------------------------------------------------------
// Interface to the actual buffer data object used to store the data
// Intention to be able to abstract the underlying implementation and code against the inteface
// e.g. if choice of buffer data object 
//------------------------------------------------------------------------------

#include <sys/types.h> 
#include <vector>

#include "XrdOuc/XrdOucIOVec.hh"

//class XrdOucIOVec;

namespace XrdCephBuffer {

typedef std::vector< std::pair<XrdOucIOVec, std::vector<XrdOucIOVec> > >  ReadVMap;

class IXrdCephReadVAdapter {
    public:
        virtual ~IXrdCephReadVAdapter(){}

        ReadVMap buildMap(XrdOucIOVec *readV, int n) const; 

    protected:
};

}

#endif 

/*
struct XrdOucIOVec
{
       long long offset;    // Offset into the file.
       int       size;      // Size of I/O to perform.
       int       info;      // Available for arbitrary use
       char     *data;      // Location to read into.
};

*/

/*
ssize_t XrdOssDF::ReadV(XrdOucIOVec *readV,
                        int          n)
{
   ssize_t nbytes = 0, curCount = 0;
   for (int i=0; i<n; i++)
       {curCount = Read((void *)readV[i].data,
                         (off_t)readV[i].offset,
                        (size_t)readV[i].size);
        if (curCount != readV[i].size)
           {if (curCount < 0) return curCount;
            return -ESPIPE;
           }
        nbytes += curCount;
       }
   return nbytes;
}
*/