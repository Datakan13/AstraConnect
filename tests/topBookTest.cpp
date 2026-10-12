#include <api/orderbook/topBook.hpp>
#include <chrono>
#include <iostream>
#include <map>
#include <memory>
#include <random>
#include <vector>

// Differential test: drives SortedBook and a std::map with the same random updates and
// checks they agree on every level and on the full best-first order. Price drifts far
// past the window so recentring and the fallback blocks are exercised on both sides.

template<bool IsBid>
int run(uint32_t seed) {
    constexpr double tick = 0.1;
    auto book = std::make_unique<SortedBook<IsBid>>();
    book->setTickSize(tick);
    std::map<int64_t,double> reference;   // tick -> volume
    std::mt19937 rng(seed);
    std::normal_distribution<double> spread(0, 400);
    double mid = 80000;
    int failures = 0;
    std::vector<Entry> got(SortedBook<IsBid>::MAX_LEVELS);

    for(int step = 0; step < 200000; step++) {
        // Trend reverses every 50k steps so both sides recentre in both directions
        mid += ((step / 50000) % 2 ? -0.1 : 0.1) + std::uniform_real_distribution<double>(-3, 3)(rng);
        int64_t t = std::llround((mid + spread(rng)) / tick);
        double price = t * tick;
        // About half the updates remove an existing level, so the book stays bounded
        if(!reference.empty() && rng() % 2) {
            auto it = reference.lower_bound(t);
            if(it == reference.end()) --it;
            t = it->first;
            price = t * tick;
            book->update(price, 0);
            reference.erase(it);
        } else {
            double volume = 1 + rng() % 1000;
            bool known = reference.count(t);
            if(book->update(price, volume)) {
                reference[t] = volume;
            } else if(known) {
                std::cout << "update of an existing level failed at step " << step << "\n";
                failures++;
            }
        }

        if(step % 1000 == 0 || step < 2000) {
            size_t n = book->top(got.data(), got.size());
            if(n != reference.size()) {
                std::cout << "step " << step << ": " << n << " levels, expected " << reference.size() << "\n";
                return failures + 1;
            }
            size_t i = 0;
            auto check = [&](auto it) {
                if(std::llround(got[i].price / tick) != it->first || got[i].volume != it->second) {
                    std::cout << "step " << step << ": level " << i << " is " << got[i].price
                              << " x " << got[i].volume << ", expected " << it->first * tick << "\n";
                    failures++;
                }
                i++;
            };
            if constexpr (IsBid) { for(auto it = reference.rbegin(); it != reference.rend(); ++it) check(it); }
            else { for(auto it = reference.begin(); it != reference.end(); ++it) check(it); }
            if(failures) return failures;
            Entry e;
            if(!reference.empty() && !book->find(reference.begin()->first * tick, e)) {
                std::cout << "step " << step << ": find missed an existing level\n";
                return failures + 1;
            }
        }
    }
    return failures;
}

int main() {
    int failures = 0;
    for(uint32_t seed = 1; seed <= 3; seed++) {
        failures += run<true>(seed);
        failures += run<false>(seed);
    }

    // Timing: best level and top 20 on a populated book, averaged over many calls
    auto book = std::make_unique<SortedBook<true>>();
    book->setTickSize(0.1);
    for(int i = 0; i < 1000; i++) book->update(80000 - i * 0.1, 1);
    Entry out[20];
    volatile double sink = 0;
    auto average = [&](size_t count) {
        constexpr int iterations = 1000000;
        auto start = std::chrono::steady_clock::now();
        for(int i = 0; i < iterations; i++) { book->top(out, count); sink = sink + out[0].price; }
        auto elapsed = std::chrono::steady_clock::now() - start;
        return std::chrono::duration<double,std::nano>(elapsed).count() / iterations;
    };
    std::cout << "best bid: " << average(1) << " ns, top 20: " << average(20) << " ns" << std::endl;

    std::cout << (failures ? "FAILED" : "all checks passed") << std::endl;
    return failures ? 1 : 0;
}
