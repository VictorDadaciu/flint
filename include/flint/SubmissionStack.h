#pragma once

#include "SubmissionInfo.h"

namespace flint
{
class SubmissionStack final
{
public:
    inline static constexpr int MAX_SUBMISSIONS = 256;

    SubmissionStack() noexcept;

    SubmissionStack(SubmissionStack&) = delete;
    SubmissionStack(SubmissionStack&&) noexcept;

    void operator=(SubmissionStack&) = delete;
    void operator=(SubmissionStack&&) noexcept;

    int get() noexcept;

    inline int size() const noexcept { return m_size; }

    inline SubmissionInfo& operator[](int index) const noexcept { return m_data[index]; }

    inline SubmissionInfo* data() const noexcept { return m_data; }

    void clear() noexcept;

    ~SubmissionStack();

private:
    SubmissionInfo* m_data;
    int m_size{};
};
} // namespace flint