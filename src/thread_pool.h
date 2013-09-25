#ifndef THREAD_POOL_H_INCL_HGU9H0UYA8FH9GUERA
#define THREAD_POOL_H_INCL_HGU9H0UYA8FH9GUERA

#include <mutex>
#include <condition_variable>
#include <queue>
#include <memory>
#include <functional>
#include <chrono>
#include <string>

class ThreadPool {
    class Worker;
    class TimerThread;
    
  public:
    class TimerTask {
      public:
        TimerTask(TimerThread *owner_, std::function<void()> fn_,
                  std::chrono::time_point<std::chrono::system_clock> task_time_);
        void cancel();
        bool operator<(const TimerTask& other);
      private:
        TimerThread *owner;
        std::function<void()> execute;
        std::chrono::time_point<std::chrono::system_clock> task_time;
        bool cancelled;
        
        bool ready();
        std::string toString();
        
        friend class TimerThread;
    };
    
    typedef std::shared_ptr<TimerTask> TimerTaskPtr;

    ThreadPool(int num_threads);
    ~ThreadPool();
    bool startTask(std::function<void()> fn);
    TimerTaskPtr scheduleTask(double seconds_in_future, std::function<void()> fn);
    void idle(Worker *worker);
    
  private:
    std::mutex lock;
    std::condition_variable available_workers_cv;
    std::queue<Worker *> workers;
    std::queue<Worker *> available_workers;
    std::queue<std::function<void()> > waiting_tasks;
    TimerThread *timerThread;
    bool shutting_down;
};

#endif
