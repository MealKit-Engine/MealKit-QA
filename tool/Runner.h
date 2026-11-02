#pragma once

// Runs command lines one after another on a worker thread (scenario tests, mp_test) and keeps each one's output and exit code.
// The screen adds jobs and polls finished ones every frame.
#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace mkqa::tool {

class Runner {
public:
    struct Job {
        int id = 0;
        std::vector<std::string> args;
        std::string output;
        int exitCode = -1;
    };

    Runner();
    ~Runner();

    int add(std::vector<std::string> args); // job id
    std::vector<Job> takeFinished();
    bool busy() const;
    void cancelPending(); // the running job finishes

private:
    void loop();

    mutable std::mutex m_mutex;
    std::condition_variable m_wake;
    std::deque<Job> m_pending;
    std::vector<Job> m_finished;
    int m_nextId = 1;
    bool m_running = false;
    bool m_quit = false;
    std::thread m_thread;
};

} // namespace mkqa::tool
