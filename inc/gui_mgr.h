// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifndef GUI_MGR_H
#define GUI_MGR_H

#ifdef __cplusplus
extern "C" {
#include <cstdint>
#else
#include <stdint.h>
#endif /* __cplusplus */

#include "umock_c/umock_c_prod.h"

typedef struct GUI_MGR_INFO_TAG* GUI_MGR_HANDLE;

MOCKABLE_FUNCTION(, GUI_MGR_HANDLE, gui_mgr_create);
MOCKABLE_FUNCTION(, void, gui_mgr_destroy, GUI_MGR_HANDLE, handle);

MOCKABLE_FUNCTION(, int, gui_mgr_initialize, GUI_MGR_HANDLE, handle);
MOCKABLE_FUNCTION(, void, gui_mgr_process_items, GUI_MGR_HANDLE, handle);
MOCKABLE_FUNCTION(, int, gui_create_win, GUI_MGR_HANDLE, handle);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // GUI_MGR_H
