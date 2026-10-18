#pragma once

#include <Module/Macros.h>
#include <Module/Module.h>

#ifdef EXPORTS
#define EDITOR_API API_EXPORT
#else
#define EDITOR_API API_IMPORT
#endif

namespace fc::Editor
{

EDITOR_API extern Module MODULE;

}; // namespace fc::Editor
