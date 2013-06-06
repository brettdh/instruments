#include "thread_pool.h"

#include <thread>
#include <queue>

using std::unique_lock; using std::thread;
using std::mutex; using std::condition_variable;
using std::queue;


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
    mutex lock;
    condition_variable cv;
    queue<std::function<void()> > q;
    bool running;
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
}

ThreadPool::~ThreadPool()
{
    unique_lock<mutex> guard(lock);
    shutting_down = true;
    // no more tasks allowed while we're waiting for workers to finish
    while (workers.size() != available_workers.size()) {
        cv.wait(guard);
    }
    
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
    while (available_workers.empty()) {
        cv.wait(guard);
    }
    Worker *worker = available_workers.front();
    available_workers.pop();
    worker->startTask(fn);
    return true;
}

void 
ThreadPool::idle(Worker *worker)
{
    unique_lock<mutex> guard(lock);
    available_workers.push(worker);
    cv.notify_all();
}

ThreadPool::Worker::Worker(ThreadPool *pool_)
    : pool(pool_), running(true)
{
    my_thread = new thread([&]() {
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
    pool->idle(this);
    
    unique_lock<mutex> guard(lock);
    while (running) {
        while (q.empty()) {
            cv.wait(guard);
        }
        auto task = q.front();
        q.pop();
        
        task();
        
        pool->idle(this);
    }
}

void 
ThreadPool::Worker::startTask(std::function<void()> fn)
{
    unique_lock<mutex> guard(lock);
    q.push(fn);
    cv.notify_one();
}

void 
ThreadPool::Worker::kill()
{
    startTask([&]() {
            running = false;
        });
    
    my_thread->join();
}
