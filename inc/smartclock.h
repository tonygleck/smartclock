// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once

#ifdef __cplusplus
extern "C" {
#include <cstdint>
#else
#include <stdint.h>
#include <stdbool.h>
#endif /* __cplusplus */

extern int run_application(int argc, char* argv[]);

#ifdef __cplusplus
}
#endif /* __cplusplus */

