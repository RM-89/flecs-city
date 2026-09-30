#pragma once

#include <Module/Macros.h>
#include <Module/Module.h>

#ifdef EXPORTS
#define CORE_API API_EXPORT
#else
#define CORE_API API_IMPORT
#endif

namespace fc::Core
{

CORE_API extern Module MODULE;

}; // namespace fc::Core
