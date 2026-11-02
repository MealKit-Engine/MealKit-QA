#include "Runner.h"

#include <SDL3/SDL.h>

namespace mkqa::tool {

Runner::Runner() : m_thread([this] { loop(); }) {}

Runner::~Runner() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_quit = true;
        m_pending.clear();
    }
    m_wake.notify_all();
    m_thread.join();
}

int Runner::add(std::vector<std::string> args) {
    std::lock_guard<std::mutex> lock(m_mutex);
    Job job;
    job.id = m_nextId++;
    job.args = std::move(args);
    m_pending.push_back(std::move(job));
    m_wake.notify_all();
    return m_pending.back().id;
}

std::vector<Runner::Job> Runner::takeFinished() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<Job> out = std::move(m_finished);
    m_finished.clear();
    return out;
}

bool Runner::busy() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_running || !m_pending.empty();
}

void Runner::cancelPending() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_pending.clear();
}

void Runner::loop() {
    for (;;) {
        Job job;
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_wake.wait(lock, [this] { return m_quit || !m_pending.empty(); });
            if (m_quit) return;
            job = std::move(m_pending.front());
            m_pending.pop_front();
            m_running = true;
        }
        std::vector<const char*> argv;
        for (const std::string& a : job.args) argv.push_back(a.c_str());
        argv.push_back(nullptr);
        // stdout and stderr together, no console window
        SDL_PropertiesID props = SDL_CreateProperties();
        SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, argv.data());
        SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_APP);
        SDL_SetBooleanProperty(props, SDL_PROP_PROCESS_CREATE_STDERR_TO_STDOUT_BOOLEAN, true);
        SDL_SetBooleanProperty(props, SDL_PROP_PROCESS_CREATE_BACKGROUND_BOOLEAN, true);
        SDL_Process* process = SDL_CreateProcessWithProperties(props);
        SDL_DestroyProperties(props);
        if (!process) {
            job.output = std::string("cannot start ") + job.args[0] + ": " + SDL_GetError() + "\n";
        } else {
            size_t size = 0;
            int code = -1;
            if (void* data = SDL_ReadProcess(process, &size, &code)) {
                job.output.assign(static_cast<const char*>(data), size);
                SDL_free(data);
            }
            job.exitCode = code;
            SDL_DestroyProcess(process);
        }
        std::lock_guard<std::mutex> lock(m_mutex);
        m_running = false;
        m_finished.push_back(std::move(job));
    }
}

} // namespace mkqa::tool
