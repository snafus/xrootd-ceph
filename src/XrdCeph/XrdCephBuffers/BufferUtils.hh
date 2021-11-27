#ifndef __CEPH_BUFFER_UTILS_HH__
#define __CEPH_BUFFER_UTILS_HH__

// holder of various small utility classes for debugging, profiling, logging, and general stuff

#include <list>
#include <vector>
#include <atomic>
#include <chrono>
#include <sys/types.h>


namespace XrdCephBuffer
{

    class Timer_ns
    {
        // RAII based timer information in ns
        // improve to template the output type and the time ratio
    public:
        explicit Timer_ns(long &output_ns);
        ~Timer_ns();

    private:
        std::chrono::steady_clock::time_point m_start;
        long &m_output_val;

    }; //Timer_ns

    class Extent
    {
    public:
        Extent(off_t offset, size_t len) : m_offset(offset), m_len(len) {}
        inline off_t offset() const { return m_offset; }
        inline size_t len() const { return m_len; }
        inline off_t begin() const { return m_offset; }
        inline off_t end() const { return m_offset + m_len; } // std::vector style end
        inline bool empty() const {return m_len == 0;}

        bool isContiguous(const Extent& rhs) const; // does the rhs connect directly to the end of the first

        inline off_t last_pos() const { return m_offset + m_len - 1; } // last real position

        bool in_extent(off_t pos) const;
        bool allInExtent(off_t pos, size_t len) const;  // is all the range in this extent
        bool someInExtent(off_t pos, size_t len) const; // is some the range in this extent

        Extent containedExtent(off_t pos, size_t len) const; // return the subset of range that is in this extent
        Extent containedExtent(const Extent &in) const;        //does the extent

        bool operator<(const Extent &rhs) const;
        bool operator==(const Extent &rhs) const;
        

    private:
        off_t m_offset;
        size_t m_len;
    };


    typedef std::vector<Extent> ExtentContainer;
    class ExtentHolder {
        // holder of a list of extent objects
        public:
        ExtentHolder();
        explicit ExtentHolder(size_t elements); // reserve memory only
        explicit ExtentHolder(const ExtentContainer& extents);
        ~ExtentHolder();

        off_t begin() const {return m_begin;}
        off_t end() const {return m_end;}
        size_t len() const {return m_end - m_begin;}

        bool empty() const {return m_extents.empty();}
        size_t size() const {return m_extents.size();}

        Extent asExtent() const; // return an extent covering the whole range


        size_t bytesContained() const; // number of bytes across the extent not considering overlaps! 
        size_t bytesMissing() const; // number of bytes missing across the extent, not considering overlaps!

        void push_back(const Extent & in);
        void sort();

        const ExtentContainer & extents() const {return m_extents;}
        //ExtentContainer & extents() {return m_extents;}

        ExtentContainer getSortedExtents() const;
        ExtentContainer getExtents() const;


        protected:
        ExtentContainer m_extents;

        off_t m_begin{0}; //lowest offset value
        off_t m_end{0}; // one past end of last byte used. 

    };

    class ExtentMap{
        // map one range of extents to another range of extents 

    };


}

#endif
