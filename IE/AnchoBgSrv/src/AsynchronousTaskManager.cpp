#include "stdafx.h"
#include "AnchoBackgroundServer/AsynchronousTaskManager.hpp"


namespace AnchoBackgroundServer {

namespace detail {
typedef std::deque<boost::function<void()> > TaskQueue;

/**
  Functor which is invoked in task manager's worker thread.
  Access to task queue is synchronized by condition variable.
 **/
struct WorkerThreadFunc
{
  WorkerThreadFunc(TaskQueue &aQueue, boost::condition_variable &aCondVariable, boost::mutex &aMutex)
    : mQueue(aQueue), mCondVariable(aCondVariable), mMutex(aMutex)
  { /*empty*/ }

  TaskQueue &mQueue;
  boost::condition_variable &mCondVariable;
  boost::mutex &mMutex;

  void operator()()
  {
    CoInitializeEx(NULL, COINIT_MULTITHREADED);

    try {
      while (true) {

        boost::function<void()> task;

        {//synchronize only queue management
          boost::unique_lock<boost::mutex> lock(mMutex);
          while (mQueue.empty()) {
              mCondVariable.wait(lock);
          }
          task = mQueue.front();
          mQueue.pop_front();
        }
        //Allow thread interruption before processing the task
        boost::this_thread::interruption_point();

        task();
      }
    } catch (boost::thread_interrupted &) {
      //Thread execution was properly ended.
      return;
    }
  }
};
} //namespace detail



struct AsynchronousTaskManager::Pimpl
{
  detail::TaskQueue mQueue;
  boost::condition_variable mCondVariable;
  boost::mutex mMutex;

  boost::thread mWorkerThread; //TODO - used thread_group after testing to increase parallelism
};


AsynchronousTaskManager::AsynchronousTaskManager(): mPimpl(new AsynchronousTaskManager::Pimpl())
{
  mPimpl->mWorkerThread = boost::thread(detail::WorkerThreadFunc(mPimpl->mQueue, mPimpl->mCondVariable, mPimpl->mMutex));
}

AsynchronousTaskManager::~AsynchronousTaskManager()
{
  //ask the worker thread to stop
  mPimpl->mWorkerThread.interrupt();
  //wait till it finishes
  mPimpl->mWorkerThread.join();
}

void AsynchronousTaskManager::addPackagedTask(boost::function<void()> aTask)
{
  {//synchronize only queue management
    boost::unique_lock<boost::mutex> lock(mPimpl->mMutex);
    mPimpl->mQueue.push_back(aTask);
  }

  //notify the threads waiting on the condition variable (threads are waiting only in case of empty queue)
  mPimpl->mCondVariable.notify_one();
}

} //namespace AnchoBackgroundServer
