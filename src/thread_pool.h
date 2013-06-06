#ifndef THREAD_POOL_H_INCL_HGU9H0UYA8FH9GUERA
#define THREAD_POOL_H_INCL_HGU9H0UYA8FH9GUERA

#include <mutex>
#include <condition_variable>
#include <queue>

class ThreadPool {
    class Worker;
    
  public:
    ThreadPool(int num_threads);
    ~ThreadPool();
    bool startTask(std::function<void()> fn);
    void idle(Worker *worker);
    
  private:
    std::mutex lock;
    std::condition_variable cv;
    std::queue<Worker *> workers;
    std::queue<Worker *> available_workers;
    bool shutting_down;
};

#endif
