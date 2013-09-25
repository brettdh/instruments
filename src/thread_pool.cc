#include "thread_pool.h"
#include "debug.h"
namespace inst = instruments;
using inst::dbgprintf;

#include <thread>
#include <chrono>
#include <queue>
#include <vector>
#include <memory>
#include <string>
#include <sstream>

using std::string; using std::ostringstream;
using std::shared_ptr;
using std::unique_lock; using std::thread;
using std::mutex; using std::condition_variable;
using std::queue; using std::priority_queue;
using std::vector;
using std::chrono::time_point;
using std::chrono::duration;
using std::chrono::system_clock;
using std::chrono::microseconds;
using std::chrono::milliseconds;
using std::chrono::duration_cast;

class ThreadPool::Worker {
  public:
    Worker(ThreadPool *pool);
    ~Worker();
    void startTask(std::function<void()> fn);
    void kill();
  private:
    void run();
    
    ThreadPool *pool;
    
    thread *my_thread;
    condition_variable cv;
    queue<std::function<void()> > q;
    bool running;
};

template <typename T>
struct PtrGreater {
    bool operator()(T* a, T* b) {
        // highest priority task is the one with the earliest (lowest) timestamp.
        return !((*a) < (*b));
    }
    bool operator()(shared_ptr<T> a, shared_ptr<T> b) {
        return (*this)(a.get(), b.get());
    }
};

bool ThreadPool::TimerTask::operator<(const TimerTask& other)
{
    return task_time < other.task_time;
}


class ThreadPool::TimerThread {
  public:
    TimerThread();
    ~TimerThread();
    TimerTaskPtr scheduleTask(double seconds_in_future, std::function<void()> fn);
    void cancelTask(TimerTask *task);
  private:
    void run();
    bool noTaskReady();
    TimerTaskPtr getReadyTask();

    bool running;
    thread *my_thread;
    mutex lock;
    condition_variable tasks_cv;
    typedef priority_queue<ThreadPool::TimerTaskPtr, 
                           vector<ThreadPool::TimerTaskPtr>, 
                           PtrGreater<TimerTask> > TaskQueue;
    TaskQueue tasks;
};

ThreadPool::ThreadPool(int num_threads)
    : shutting_down(false)
{
    unique_lock<mutex> guard(lock);
    for (int i = 0; i < num_threads; ++i) {
        // threads will initialize themselves and add themselves
        //  to the idle queue by calling ThreadPool::idle.
        workers.push(new Worker(this));
    }
    timerThread = new TimerThread;
}

ThreadPool::~ThreadPool()
{
    unique_lock<mutex> guard(lock);
    shutting_down = true;
    // no more tasks allowed while we're waiting for workers to finish
    while (workers.size() != available_workers.size()) {
        available_workers_cv.wait(guard);
    }
    
    delete timerThread;
    
    queue<Worker *> local_workers = workers;
    guard.unlock();
    while (!local_workers.empty()) {
        Worker *worker = local_workers.front();
        local_workers.pop();
        worker->kill(); // assume all tasks will eventually finish (TODO: timeout?)
        delete worker;
    }
}

bool
ThreadPool::startTask(std::function<void()> fn)
{
    unique_lock<mutex> guard(lock);
    if (shutting_down) {
        return false;
    }
    
    if (available_workers.empty()) {
        waiting_tasks.push(fn);
    } else {
        Worker *worker = available_workers.front();
        available_workers.pop();
        worker->startTask(fn);
    }
    return true;
}

ThreadPool::TimerTaskPtr
ThreadPool::scheduleTask(double seconds_in_future, std::function<void()> fn)
{
    return timerThread->scheduleTask(seconds_in_future, fn);
}

void 
ThreadPool::idle(Worker *worker)
{
    if (!waiting_tasks.empty()) {
        auto task = waiting_tasks.front();
        waiting_tasks.pop();
        worker->startTask(task);
    } else {
        available_workers.push(worker);
        available_workers_cv.notify_all();
    }
}

ThreadPool::Worker::Worker(ThreadPool *pool_)
    : pool(pool_), running(true)
{
    my_thread = new thread([=]() {
            run();
        });
}

ThreadPool::Worker::~Worker()
{
    delete my_thread;
}

void 
ThreadPool::Worker::run()
{
    unique_lock<mutex> guard(pool->lock);
    pool->idle(this);
    
    while (running) {
        while (q.empty()) {
            cv.wait(guard);
        }
        auto task = q.front();
        q.pop();
        
        guard.unlock();
        task();
        guard.lock();
        
        pool->idle(this);
    }
}

void 
ThreadPool::Worker::startTask(std::function<void()> fn)
{
    // holding pool->lock
    q.push(fn);
    cv.notify_one();
}

void 
ThreadPool::Worker::kill()
{
    startTask([=]() {
            running = false;
        });
    
    my_thread->join();
}

ThreadPool::TimerTask::TimerTask(TimerThread *owner_, std::function<void()> fn_, 
                                 time_point<system_clock> task_time_)
    : owner(owner_), execute(fn_), task_time(task_time_), cancelled(false)
{
}

void 
ThreadPool::TimerTask::cancel()
{
    owner->cancelTask(this);
}

string
ThreadPool::TimerTask::toString()
{
    if (!inst::is_debugging_on(inst::DEBUG)) {
        return "(not printed)";
    }
    ostringstream s;
    
    auto now = system_clock::now();
    bool past = (now >= task_time);
    auto millis = duration_cast<milliseconds>(past ? (now - task_time) : (task_time - now));
    auto stamp = duration_cast<milliseconds>(task_time.time_since_epoch());
    s << "Task " << this << ": " << stamp.count() 
      << " (" << (past ? "" : "in ")
      << millis.count() << "ms"
      << (past ? " ago" : "")
      << "), "
      << (cancelled ? "" : "not ")
      << "cancelled";
    return s.str();
}

bool
ThreadPool::TimerTask::ready()
{
    auto now = system_clock::now();
    bool past = (now >= task_time);
    return !cancelled && past;
}

ThreadPool::TimerThread::TimerThread()
    : running(true)
{
    my_thread = new thread([=]() {
            run();
        });
}

ThreadPool::TimerThread::~TimerThread()
{
    lock.lock();
    running = false;
    while (!tasks.empty()) {
        tasks.pop();
    }
    tasks_cv.notify_one();
    lock.unlock();

    my_thread->join();
    delete my_thread;
}

ThreadPool::TimerTaskPtr 
ThreadPool::TimerThread::scheduleTask(double seconds_in_future, std::function<void()> fn)
{
    unique_lock<mutex> guard(lock);
    time_point<system_clock> task_time = system_clock::now() + microseconds((long long) (seconds_in_future * 1000000));
    TimerTaskPtr task(new TimerTask(this, fn, task_time));
    tasks.push(task);
    tasks_cv.notify_one();
    return task;
}

void 
ThreadPool::TimerThread::cancelTask(TimerTask *task)
{
    unique_lock<mutex> guard(lock);
    // hold the lock here to make sure I never try to execute
    //  a cancelled task.
    task->cancelled = true;
}

void ThreadPool::TimerThread::run()
{
    TimerTaskPtr nextTask;
    while ( (nextTask = getReadyTask()) ) {
        dbgprintf(inst::DEBUG, "Executing %s\n", nextTask->toString().c_str());
        nextTask->execute();
    }
}

ThreadPool::TimerTaskPtr
ThreadPool::TimerThread::getReadyTask()
{
    unique_lock<mutex> guard(lock);
    while (noTaskReady()) {
        if (!running) {
            return TimerTaskPtr();
        }
        if (tasks.empty()) {
            dbgprintf(inst::DEBUG, "No tasks; waiting\n");
            tasks_cv.wait(guard);
        } else {
            if (tasks.top()->cancelled) {
                dbgprintf(inst::DEBUG, "Skipping %s\n", tasks.top()->toString().c_str());
                tasks.pop();
                continue;
            }
            
            auto task_time = tasks.top()->task_time;
            dbgprintf(inst::DEBUG, "Waiting for %s\n", tasks.top()->toString().c_str());
            tasks_cv.wait_until(guard, task_time);
        }
    }
    TimerTaskPtr readyTask = tasks.top();
    tasks.pop();
    return readyTask;
}

bool 
ThreadPool::TimerThread::noTaskReady()
{
    // must be holding lock.
    return (tasks.empty() || !tasks.top()->ready());
}
