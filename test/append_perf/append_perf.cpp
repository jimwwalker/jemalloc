/**
  Time an increasing size "append" 

  Modelled after a perf regression seen in Couchbase when updating jemalloc 4.0.4

  n threads (configured with 4) will call the append function to grow a buffer and copy into it.
  The threads work on their own buffer.

  1. Build with something like (and store binary outside of git if using the crunch script)

  g++ -o /tmp/append_perf  -std=c++11 append_perf.cpp -O3 -lpthread -ljemalloc -L /home/jim/jemalloc_perf/jemalloc/lib/ -I /home/jim/jemalloc_perf/jemalloc/include/

  2. Outputs the following CSV timing data
  
  "Mean, Median, 5%, 95%, 99%" 
	

**/

#include <vector>
#include <chrono>
#include <string>
#include <stdlib.h>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <numeric>
#include <thread>
#include <memory>
#include <mutex>
#include <atomic>
#include <cstring>

#include "jemalloc/jemalloc.h"

// borrowed/modded from platform/src/gethrtime.c
static uint64_t gethrtime(void) {
    auto now = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
}


char* append(char* orginal, size_t orginal_len, const char* data, size_t data_len) {
    size_t newsize = orginal_len + data_len;
    char* newbuf = (char*)je_malloc(newsize);
    memcpy(newbuf, orginal, orginal_len);
    memcpy(newbuf + orginal_len, data, data_len);
    je_free(orginal);
    return newbuf;
}

void print_values(std::vector<uint64_t>& values) {
    std::sort(values.begin(), values.end());
    double median = values[(values.size() * 50) / 100];
    double pct5 = values[(values.size() * 5) / 100];
    double pct95 = values[(values.size() * 95) / 100];
    double pct99 = values[(values.size() * 99) / 100];

    const double sum = std::accumulate(values.begin(), values.end(), 0.0);
    double mean = sum / values.size();

//    std::cout << "Mean, Median, 5%, 95%, 99%\n";
    std::cout << std::fixed << std::setprecision(3)
              << (mean/1e3) << ", "
              << (median/1e3) << ", "
              << (pct5/1e3) << ", "
              << (pct95/1e3) << ", "
              << (pct99/1e3) << std::endl;
    return;
}


class Worker {
public:

    Worker(int i, int a, std::mutex& sg, std::atomic<int>& c)
      : iterations(i),
        append_size(a),
        starting_gun(sg),
        count_me_in(c) {
    }

    void runloop() {
        count_me_in++;
        {
            std::lock_guard<std::mutex> lg(starting_gun);
        }

        std::string data('x', append_size);

        char* buffer = (char*)je_malloc(append_size);
        size_t buffer_len = append_size;

        for (int ii = 0; ii < iterations; ii++) {
            uint64_t start = gethrtime();
            buffer = append(buffer, buffer_len, data.c_str(), data.length());
            buffer_len += data.length();
            timings.push_back(gethrtime() - start);
        }
    }

    void start() {
        thread = std::thread(&Worker::runloop, this);
    }

    void wait() {
        thread.join();
    }

    std::vector<uint64_t>& getTimings() {
        return timings;
    }

private:
    int append_size;
    int iterations;
    std::mutex& starting_gun;
    std::vector<uint64_t> timings;
    std::atomic<int>& count_me_in;
    std::thread thread;

};

int main() {
    const int append_size = 50;
    const int iterations = 10000;
    const int worker_count = 4;

    std::vector<std::unique_ptr<Worker> > workers(worker_count);
    std::vector<uint64_t> timings;

    std::mutex starting_gun;
    {
        std::lock_guard<std::mutex> lg(starting_gun);
        std::atomic<int> workers_running(0);
        for (int i = 0; i < worker_count; i++) {
            workers[i] = std::unique_ptr<Worker>(new Worker(iterations, append_size, starting_gun, workers_running));
            workers[i]->start();
        }

        while (workers_running != worker_count) {
            //urgh cba to do proper async
        }
    }

    for (auto &w: workers) {
        w->wait();
        // merge timings
        timings.insert(timings.end(),
                       w->getTimings().begin(),
                       w->getTimings().end());
    }

    print_values(timings);
    return 0;
}
