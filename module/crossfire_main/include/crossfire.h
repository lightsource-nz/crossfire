#ifndef _CROSSFIRE_H
#define _CROSSFIRE_H

#include <light.h>

#include <stdint.h>

//   the application's own version, derived from its git tags at build time -- see
// light_project_version(CROSSFIRE) in the top-level CMakeLists. Was a hand-typed "0.1.0" with a
// TODO beside it, which meant a firmware image could not say which commit produced it.
#include <crossfire_version.h>
#define CF_VERSION_STR                  CROSSFIRE_VERSION_STRING

#define CF_INFO_STR                     "Crossfire v" CF_VERSION_STR

extern void crossfire_init();
extern void crossfire_task();

#endif