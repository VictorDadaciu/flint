#include "SubmissionStack.h"

#include "SubmissionInfo.h"

#include <cassert>

namespace flint
{
SubmissionStack::SubmissionStack() : m_data(new SubmissionInfo[SubmissionStack::MAX_SUBMISSIONS])
{
}

int SubmissionStack::get() noexcept
{
    assert(m_size < SubmissionStack::MAX_SUBMISSIONS);
    return m_size++;
}

void SubmissionStack::clear() noexcept
{
    for (int i = 0; i < m_size; ++i)
    {
        m_data[i].cleanup();
    }
    m_size = 0;
}

SubmissionStack::~SubmissionStack()
{
    clear();
    delete[] m_data;
}
} // namespace flint