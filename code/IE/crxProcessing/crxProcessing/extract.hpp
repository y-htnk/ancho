#pragma once

#include <boost/thread/future.hpp>
#include <boost/filesystem.hpp>

namespace crx {

void extract(const boost::filesystem::wpath &aCRXFile, const boost::filesystem::wpath &aOutputPath);

} //namespace crx
