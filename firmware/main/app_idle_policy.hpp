#pragma once

#include <cstdint>

namespace brookesia::power {

// Supply one complete snapshot of running apps between begin_scan/end_scan.
// Call close_failed after an unsuccessful close and forget on lifecycle events
// so a stop/reopen between snapshots starts fresh. The caller serializes access.
class AppIdlePolicy {
public:
    static constexpr uint32_t FOREGROUND_TIMEOUT_MS = 300000;
    static constexpr uint32_t BACKGROUND_TIMEOUT_MS = 120000;
    static constexpr uint32_t CLOSE_RETRY_MS = 30000;

    void begin_scan()
    {
        for (Entry &entry : entries_) {
            entry.seen = false;
        }
    }

    bool observe(int id, bool foreground, uint32_t now_ms, uint32_t user_idle_ms)
    {
        if (id < 0) {
            return false;
        }

        Entry *entry = find(id);
        if (entry == nullptr) {
            for (Entry &candidate : entries_) {
                if (candidate.id < 0) {
                    entry = &candidate;
                    entry->id = id;
                    entry->foreground = foreground;
                    entry->role_since_ms = now_ms;
                    break;
                }
            }
            // Keep tracked apps intact when capacity is exhausted. An unseen
            // slot can become available at end_scan for the next snapshot.
            if (entry == nullptr) {
                return false;
            }
        } else if (entry->foreground != foreground) {
            entry->foreground = foreground;
            entry->role_since_ms = now_ms;
            entry->retry_pending = false;
        }
        entry->seen = true;

        const uint32_t residency_ms = now_ms - entry->role_since_ms;
        const uint32_t timeout_ms = foreground ? FOREGROUND_TIMEOUT_MS : BACKGROUND_TIMEOUT_MS;
        if (residency_ms < timeout_ms || (foreground && user_idle_ms < timeout_ms)) {
            return false;
        }
        return !entry->retry_pending ||
            static_cast<uint32_t>(now_ms - entry->failed_at_ms) >= CLOSE_RETRY_MS;
    }

    void end_scan()
    {
        for (Entry &entry : entries_) {
            if (!entry.seen) {
                entry = Entry{};
            }
        }
    }

    void close_failed(int id, uint32_t now_ms)
    {
        Entry *entry = find(id);
        if (entry != nullptr) {
            entry->failed_at_ms = now_ms;
            entry->retry_pending = true;
        }
    }

    void forget(int id)
    {
        Entry *entry = find(id);
        if (entry != nullptr) {
            *entry = Entry{};
        }
    }

    void reset()
    {
        for (Entry &entry : entries_) {
            entry = Entry{};
        }
    }

private:
    struct Entry {
        int id = -1;
        uint32_t role_since_ms = 0;
        uint32_t failed_at_ms = 0;
        bool foreground = false;
        bool seen = false;
        bool retry_pending = false;
    };

    Entry *find(int id)
    {
        if (id >= 0) {
            for (Entry &entry : entries_) {
                if (entry.id == id) {
                    return &entry;
                }
            }
        }
        return nullptr;
    }

    Entry entries_[32]{};
};

} // namespace brookesia::power
