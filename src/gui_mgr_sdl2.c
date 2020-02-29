// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <stdio.h>
#include <stdlib.h>

#include <SDL2/SDL.h>

#include "lib-util-c/sys_debug_shim.h"
#include "lib-util-c/app_logging.h"

#include "gui_mgr.h"
#include "lvgl/lvgl.h"
#include "lv_drivers/display/monitor.h"
#include "lv_drivers/indev/mouse.h"

static lv_disp_buf_t disp_buf1;
static lv_color_t buf1_1[480*10];

LV_IMG_DECLARE(cogwheel);

typedef struct GUI_MGR_INFO_TAG
{
    lv_obj_t* main_win;
} GUI_MGR_INFO;

static int tick_thread(void * data)
{
    (void)data;
    while(1)
    {
        SDL_Delay(5);   /*Sleep for 5 millisecond*/
        lv_tick_inc(5); /*Tell LittelvGL that 5 milliseconds were elapsed*/
    }
    return 0;
}

static void event_handler(lv_obj_t* obj, lv_event_t event)
{
    (void)obj;
    switch (event)
    {
        case LV_EVENT_CLICKED:
            printf("Clicked\n");
            break;
    }
}

GUI_MGR_HANDLE gui_mgr_create(void)
{
    GUI_MGR_INFO* result;
    if ((result = (GUI_MGR_INFO*)malloc(sizeof(GUI_MGR_INFO))) == NULL)
    {
        log_error("Failure allocating gui manager");
    }
    else
    {
    }
    return result;
}

void gui_mgr_destroy(GUI_MGR_HANDLE handle)
{
    if (handle != NULL)
    {
        free(handle);
    }
}

int gui_mgr_initialize(GUI_MGR_HANDLE handle)
{
    (void)handle;
    lv_init();

    // Use the 'monitor' driver which creates window on PC's
    // monitor to simulate a display
    monitor_init();

    // Create a display buffer
    lv_disp_buf_init(&disp_buf1, buf1_1, NULL, 480*10);

    // Create a display
    lv_disp_drv_t disp_drv;
    // Basic initialization
    lv_disp_drv_init(&disp_drv);
    disp_drv.buffer = &disp_buf1;
    // Used when `LV_VDB_SIZE != 0` in lv_conf.h (buffered drawing)
    disp_drv.flush_cb = monitor_flush;
    lv_disp_drv_register(&disp_drv);

    // Add the mouse as input device
    // Use the 'mouse' driver which reads the PC's mouse
    mouse_init();
    lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    // This function will be called periodically (by the library) to get the mouse position and state
    indev_drv.read_cb = mouse_read;
    /*lv_indev_t* mouse_indev = */(void)lv_indev_drv_register(&indev_drv);

    //  Tick init.
    // You have to call 'lv_tick_inc()' in periodically to inform LittelvGL about how much time were elapsed
    // Create an SDL thread to do this
    SDL_CreateThread(tick_thread, "tick", NULL);

    // Optional:
    // Create a memory monitor task which prints the memory usage in periodically.
    //lv_task_create(memory_monitor, 250, LV_TASK_PRIO_MID, NULL);

    return 0;
}

int gui_create_win(GUI_MGR_HANDLE handle)
{
    //lv_coord_t hres = lv_disp_get_hor_res(NULL);
    //lv_coord_t vres = lv_disp_get_ver_res(NULL);

    //printf("", hres.)

    static lv_style_t win_style;
    lv_style_copy(&win_style, &lv_style_transp);
    win_style.body.padding.left= LV_DPI / 6;
    win_style.body.padding.right = LV_DPI / 6;
    win_style.body.padding.top = LV_DPI / 6;
    win_style.body.padding.bottom = LV_DPI / 6;
    win_style.body.padding.inner = LV_DPI / 6;

    handle->main_win = lv_win_create(lv_disp_get_scr_act(NULL), NULL);
    lv_win_set_title(handle->main_win, "Group test");
    lv_obj_set_event_cb(lv_win_get_content(handle->main_win), event_handler);

    //lv_obj_t* hour_img = lv_img_create(handle->main_win, NULL);
    //lv_img_set_src(hour_img, &cogwheel);
    //lv_obj_align(hour_img, NULL, LV_ALIGN_CENTER, 0, -20);

    return 0;
}

void gui_mgr_process_items(GUI_MGR_HANDLE handle)
{
    (void)handle;
    lv_task_handler();
}