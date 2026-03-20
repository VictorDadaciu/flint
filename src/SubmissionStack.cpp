#include "SubmissionStack.h"

#include "SubmissionInfo.h"

#include <cassert>

namespace flint
{
SubmissionStack::SubmissionStack() noexcept : m_data(new SubmissionInfo[MAX_SUBMISSIONS])
{
}

SubmissionStack::SubmissionStack(SubmissionStack&& other) noexcept : m_data(other.m_data), m_size((other.m_size))
{
    other.m_data = nullptr;
    other.m_size = 0;
}

void SubmissionStack::operator=(SubmissionStack&& other) noexcept
{
    clear();
    m_data = other.m_data;
    m_size = other.m_size;
    other.m_data = nullptr;
    other.m_size = 0;
}

int SubmissionStack::get() noexcept
{
    assert(m_size < SubmissionStack::MAX_SUBMISSIONS);
    return m_size++;
}

void SubmissionStack::clear() noexcept
{
    delete[] m_data;
    m_data = new SubmissionInfo[MAX_SUBMISSIONS];
    m_size = 0;
}

SubmissionStack::~SubmissionStack()
{
    clear();
}
} // namespace flint