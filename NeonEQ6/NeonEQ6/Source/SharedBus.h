#pragma once
#include <JuceHeader.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

// Lock-free single-writer audio ring (benign races, used for analysis / key signals)
template <int Size>
struct AudioRing
{
    static_assert ((Size & (Size - 1)) == 0, "power of two");

    std::array<float, Size> data {};
    std::atomic<uint64_t> wp { 0 };

    void put (uint64_t pos, const float* src, int n)
    {
        for (int i = 0; i < n; ++i)
            data[(size_t) ((pos + (uint64_t) i) & (uint64_t) (Size - 1))] = src[i];
    }

    void push (const float* src, int n)
    {
        const auto p = wp.load (std::memory_order_relaxed);
        put (p, src, n);
        wp.store (p + (uint64_t) n, std::memory_order_release);
    }

    // reads `cnt` samples starting `offset` samples into the most recent block of size `blockN`
    bool readBlock (float* dst, int blockN, int offset, int cnt) const
    {
        const auto w = wp.load (std::memory_order_acquire);

        if (blockN > Size / 2 || w < (uint64_t) blockN)
        {
            std::fill (dst, dst + cnt, 0.0f);
            return false;
        }

        const uint64_t start = w - (uint64_t) blockN + (uint64_t) offset;
        for (int i = 0; i < cnt; ++i)
            dst[i] = data[(size_t) ((start + (uint64_t) i) & (uint64_t) (Size - 1))];
        return true;
    }

    bool readLatest (float* dst, int n) const { return readBlock (dst, n, 0, n); }
};

// One per plugin instance: exposes the instance's input signal + its track name
struct SharedEntry
{
    AudioRing<32768> ring;
    std::atomic<bool> alive { true };
    mutable juce::CriticalSection lock;
    juce::String name;

    juce::String getName() const { juce::ScopedLock l (lock); return name; }
    void setName (const juce::String& n) { juce::ScopedLock l (lock); name = n; }
};

class Registry
{
public:
    static Registry& get()
    {
        static Registry r;
        return r;
    }

    void add (const std::shared_ptr<SharedEntry>& e)
    {
        std::lock_guard<std::mutex> l (m);
        entries.push_back (e);
    }

    void remove (const SharedEntry* e)
    {
        std::lock_guard<std::mutex> l (m);
        entries.erase (std::remove_if (entries.begin(), entries.end(),
                                       [e] (const std::weak_ptr<SharedEntry>& w)
                                       {
                                           auto s = w.lock();
                                           return ! s || s.get() == e;
                                       }),
                       entries.end());
    }

    std::vector<std::shared_ptr<SharedEntry>> snapshot()
    {
        std::lock_guard<std::mutex> l (m);
        std::vector<std::shared_ptr<SharedEntry>> out;
        for (auto& w : entries)
            if (auto s = w.lock())
                if (s->alive.load())
                    out.push_back (s);
        return out;
    }

private:
    std::mutex m;
    std::vector<std::weak_ptr<SharedEntry>> entries;
};
