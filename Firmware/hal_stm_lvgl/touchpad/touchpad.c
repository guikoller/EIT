/**
 * @file indev.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "hal_stm_lvgl/tft/tft.h"
#include "lvgl/lvgl.h"
#include "eit_config.h"

#include "stm32f7xx.h"
#include "stm32f769i_discovery.h"
#include "stm32f769i_discovery_ts.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void touchpad_read_cb(lv_indev_t * indev, lv_indev_data_t *data);

/**********************
 *  STATIC VARIABLES
 **********************/
static TS_StateTypeDef  TS_State;

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/**
 * Initialize your input devices here
 */
void touchpad_init(void)
{
  BSP_TS_Init(TFT_HOR_RES, TFT_VER_RES);

  lv_indev_t * indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, touchpad_read_cb);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void touchpad_read_cb(lv_indev_t * indev, lv_indev_data_t *data)
{
	static int16_t last_x = 0;
	static int16_t last_y = 0;
	BSP_TS_GetState(&TS_State);
	if(TS_State.touchDetected != 0) {
		int16_t raw_x = (int16_t)TS_State.touchX[0];
		int16_t raw_y = (int16_t)TS_State.touchY[0];
		int16_t adj_x = (int16_t)(raw_x - (int16_t)EIT_TOUCH_X_OFFSET);
		int16_t adj_y = (int16_t)(raw_y - (int16_t)EIT_TOUCH_Y_OFFSET);

		/* Clamp to valid screen coordinates */
		if(adj_x < 0) adj_x = 0;
		if(adj_y < 0) adj_y = 0;
		if(adj_x >= TFT_HOR_RES) adj_x = TFT_HOR_RES - 1;
		if(adj_y >= TFT_VER_RES) adj_y = TFT_VER_RES - 1;

		data->point.x = adj_x;
		data->point.y = adj_y;
		last_x = data->point.x;
		last_y = data->point.y;
		data->state = LV_INDEV_STATE_PR;
	} else {
		data->point.x = last_x;
		data->point.y = last_y;
		data->state = LV_INDEV_STATE_REL;
	}
}
