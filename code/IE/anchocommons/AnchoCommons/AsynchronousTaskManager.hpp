#pragma once

#include <boost/utility.hpp>
#include <boost/thread/future.hpp>
#include <boost/thread.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>
#include <deque>

namespace Ancho {
namespace Utils {

class AsynchronousTaskManager: public boost::noncopyable
{
public:
  AsynchronousTaskManager();
  ~AsynchronousTaskManager();

  /**
    \param aTask Callable object - defines parameter-less operator()
   **/
  template<typename TTask>
  void
  addTask(TTask aTask);

  void finalize();
private:
  void addPackagedTask(boost::function<void()> aTask);

  //Hidden implementation
  struct Pimpl;
  boost::scoped_ptr<Pimpl> mPimpl;
};

template<typename TTask>
void
AsynchronousTaskManager::addTask(TTask aTask)
{
  boost::function<void()> task(aTask);
  addPackagedTask(task);
}

} //namespace Utils
} //namespace Ancho
