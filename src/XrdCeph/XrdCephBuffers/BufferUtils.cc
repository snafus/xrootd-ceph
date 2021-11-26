
#include "BufferUtils.hh"

using namespace XrdCephBuffer;

Timer_ns::Timer_ns(long &output) : m_output_val(output)
{
    m_start = std::chrono::steady_clock::now();
}

Timer_ns::~Timer_ns()
{
    auto end = std::chrono::steady_clock::now();
    m_output_val = std::chrono::duration_cast<std::chrono::nanoseconds>(end - m_start).count();
}
