#pragma once

#include <Geode/platform/cplatform.h>

#include <string>

namespace media {

#ifdef GEODE_IS_ANDROID
void triggerMediaScan(std::string const& path);
#else
inline void triggerMediaScan(std::string const&) {}
#endif

}
