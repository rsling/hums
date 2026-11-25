#include "AllocationCounter.h"

#include <atomic>
#include <cstdlib>
#include <new>

namespace
{

std::atomic<std::size_t> allocationCount { 0 };

} // namespace

namespace guardrails
{

bool AllocationCounter::isCounting() noexcept
{
#if HUMS_COUNT_ALLOCATIONS
    return true;
#else
    return false;
#endif
}

std::size_t AllocationCounter::count() noexcept
{
    return allocationCount.load();
}

} // namespace guardrails

#if HUMS_COUNT_ALLOCATIONS

namespace
{

// malloc gives no memory for 0 bytes on some systems; `new` must.
void* countedAllocation (std::size_t size) noexcept
{
    ++allocationCount;
    return std::malloc (size == 0 ? 1 : size);
}

void* countedAlignedAllocation (std::size_t size, std::align_val_t alignment) noexcept
{
    ++allocationCount;

    // aligned_alloc wants a size that is a multiple of the alignment.
    const auto step = static_cast<std::size_t> (alignment);
    const auto roundedSize = ((size == 0 ? 1 : size) + step - 1) / step * step;

    return std::aligned_alloc (step, roundedSize);
}

} // namespace

// The replacements for the whole program. The array forms and the nothrow
// forms of the standard library call these on their own, but that is not
// promised for every one of them, so each is written out.

void* operator new (std::size_t size)
{
    if (void* memory = countedAllocation (size))
        return memory;

    throw std::bad_alloc();
}

void* operator new[] (std::size_t size)
{
    if (void* memory = countedAllocation (size))
        return memory;

    throw std::bad_alloc();
}

void* operator new (std::size_t size, const std::nothrow_t&) noexcept
{
    return countedAllocation (size);
}

void* operator new[] (std::size_t size, const std::nothrow_t&) noexcept
{
    return countedAllocation (size);
}

void* operator new (std::size_t size, std::align_val_t alignment)
{
    if (void* memory = countedAlignedAllocation (size, alignment))
        return memory;

    throw std::bad_alloc();
}

void* operator new[] (std::size_t size, std::align_val_t alignment)
{
    if (void* memory = countedAlignedAllocation (size, alignment))
        return memory;

    throw std::bad_alloc();
}

void* operator new (std::size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept
{
    return countedAlignedAllocation (size, alignment);
}

void* operator new[] (std::size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept
{
    return countedAlignedAllocation (size, alignment);
}

// Everything above comes from malloc or aligned_alloc, and free takes both.

void operator delete (void* memory) noexcept
{
    std::free (memory);
}

void operator delete[] (void* memory) noexcept
{
    std::free (memory);
}

void operator delete (void* memory, std::size_t) noexcept
{
    std::free (memory);
}

void operator delete[] (void* memory, std::size_t) noexcept
{
    std::free (memory);
}

void operator delete (void* memory, const std::nothrow_t&) noexcept
{
    std::free (memory);
}

void operator delete[] (void* memory, const std::nothrow_t&) noexcept
{
    std::free (memory);
}

void operator delete (void* memory, std::align_val_t) noexcept
{
    std::free (memory);
}

void operator delete[] (void* memory, std::align_val_t) noexcept
{
    std::free (memory);
}

void operator delete (void* memory, std::size_t, std::align_val_t) noexcept
{
    std::free (memory);
}

void operator delete[] (void* memory, std::size_t, std::align_val_t) noexcept
{
    std::free (memory);
}

void operator delete (void* memory, std::align_val_t, const std::nothrow_t&) noexcept
{
    std::free (memory);
}

void operator delete[] (void* memory, std::align_val_t, const std::nothrow_t&) noexcept
{
    std::free (memory);
}

#endif
