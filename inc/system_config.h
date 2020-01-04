// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifndef SYSTEM_CONFIG_H
#define SYSTEM_CONFIG_H

#ifdef __cplusplus
extern "C" {
#include <cstdint>
#else
#include <stdint.h>
#include <stdbool.h>
#endif /* __cplusplus */

extern int sys_config_set_display(bool turn_on);

extern int sys_config_set_display_brightness(uint8_t setting);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // SYSTEM_CONFIG_H
