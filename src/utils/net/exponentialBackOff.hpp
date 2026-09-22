#pragma once
// status.hpp refers to beast::error_code, so boost has to come first for this
// header to be includable on its own.
#include "net/includeBoost.hpp"
#include "core/types/status.hpp"
#include <chrono>
#include <thread>
#include <cmath>
#include <cstdint>
#include <utility>
#include <concepts>
#include <mutex>
#include <condition_variable>

namespace AstraConnect {
namespace Utils {

// Lets a waiter in exponentialBackOff() be woken before its interval is up.
// Whoever flips the keepGoing() condition calls signal() straight after, and the
// parked wait returns at once instead of sleeping out the rest of the interval.
//
// signal() is sticky: a signal that lands before the wait starts is still seen,
// so there is no lost-wakeup window between testing the condition and parking.
class BackoffInterrupt {
    std::mutex mutex;
    std::condition_variable cv;
    bool signalled = false;

    public:
    void signal() {
        {
            std::lock_guard<std::mutex> guard(mutex);
            signalled = true;
        }
        cv.notify_all();
    }

    // Clears the signal so the handle can be reused across reconnects.
    void reset() {
        std::lock_guard<std::mutex> guard(mutex);
        signalled = false;
    }

    // Returns true if signalled, false if the timeout expired.
    bool waitFor(std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> guard(mutex);
        return cv.wait_for(guard, timeout, [this]{ return signalled; });
    }
};

// Retry schedule shared by every connection holder.
//
// Sleeps delay ms, then delay*increasePerTry, and so on. It gives up once the
// sleep would reach delay * increasePerTry^maxTryCount.
//
// Defaults: 500ms, doubling, ceiling at 500 * 2^7 = 64s. A full run to exhaustion
// sleeps 500+1000+...+32000 ms, so roughly 64s of retrying before giving up.
struct BackoffPolicy {
    int delay = 500;
    int increasePerTry = 2;
    int maxTryCount = 7;

    // int64 because delay * increasePerTry^maxTryCount overflows a 32 bit int
    // as soon as maxTryCount goes much past the default.
    int64_t maxDelay() const {
        return static_cast<int64_t>(delay) *
               static_cast<int64_t>(std::pow(increasePerTry, maxTryCount));
    }
};

// Retries attempt() on an exponential schedule.
//
// attempt():   one connection try, returns ConnectionStatus
// keepGoing(): checked before every try, return false to abort
//
// Returns
//   SUCCESS  attempt() succeeded
//   FAIL     the schedule was exhausted
//   CLOSED   keepGoing() returned false, so we stopped on purpose
//
// Pass a BackoffInterrupt to make the sleep interruptible: whoever flips the
// keepGoing() condition then calls signal() on it, and the wait ends at once
// instead of running out the interval. Without one the sleep is uninterruptible
// and an abort is only noticed after the current interval finishes, which at the
// default schedule can be up to 32s.
template<typename Attempt, typename KeepGoing>
    requires std::invocable<KeepGoing&>   // keeps a BackoffPolicy from binding here
ConnectionStatus exponentialBackOff(Attempt&& attempt, KeepGoing&& keepGoing,
                                    const BackoffPolicy& policy = BackoffPolicy{},
                                    BackoffInterrupt* wakeup = nullptr) {
    std::chrono::milliseconds backoff(policy.delay);
    const int64_t maxDelay = policy.maxDelay();

    for(;;) {
        if(!keepGoing()) return ConnectionStatus::CLOSED;

        // waitFor() re-tests the sticky flag under the lock, so a signal arriving
        // between the check above and the wait below is still observed.
        if(wakeup) wakeup->waitFor(backoff);
        else       std::this_thread::sleep_for(backoff);

        // Woken for shutdown rather than for a retry.
        if(!keepGoing()) return ConnectionStatus::CLOSED;

        if(attempt() == ConnectionStatus::SUCCESS) {
            return ConnectionStatus::SUCCESS;
        }
        backoff *= policy.increasePerTry;
        if(backoff.count() >= maxDelay) {
            return ConnectionStatus::FAIL;
        }
    }
}

// For holders with no shutdown flag to honour. Never returns CLOSED.
template<typename Attempt>
ConnectionStatus exponentialBackOff(Attempt&& attempt,
                                    const BackoffPolicy& policy = BackoffPolicy{}) {
    return exponentialBackOff(std::forward<Attempt>(attempt),
                              []{ return true; },
                              policy);
}

} // namespace Utils
} // namespace AstraConnect
