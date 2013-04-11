#pragma once

namespace AnchoBackgroundServer {

class AsynchronousTaskManager: public boost::noncopyable
{
public:
  AsynchronousTaskManager();
  ~AsynchronousTaskManager();

  /**
    \param aTask Callable object - defines parameter-less operator()
    \result Future which will contain the result of passed function object (or exception)
   **/
  template<typename TTask>
  boost::future<typename boost::result_of<TTask()>::type>
  addTask(TTask aTask);


private:
  void addPackagedTask(boost::function<void()> aTask);

  //Hidden implementation
  struct Pimpl;
  boost::scoped_ptr<Pimpl> mPimpl;
};


template<typename TTask>
boost::future<typename boost::result_of<TTask()>::type>
AsynchronousTaskManager::addTask(TTask aTask)
{
  typedef typename boost::result_of<TTask()>::type ReturnType;
  typedef boost::packaged_task<ReturnType> PackagedTask;

  boost::shared_ptr<PackagedTask> packagedTask = boost::make_shared<PackagedTask>(aTask);
  boost::future<typename boost::result_of<TTask()>::type> f = packagedTask->get_future();

  addPackagedTask(boost::bind(&PackagedTask::operator(), packagedTask));
  return boost::move(f);
}

} //namespace AnchoBackgroundServer
