 /**
 ******************************************************************************
 * @file    display_mpe.c
 * @author  GPM Application Team
 *
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2024 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */

#include "display_mpe.h"
#include "app_config.h"
#include "main.h"
#include "utils.h"
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>

#define NUMBER_COLORS 10
const uint32_t colors[NUMBER_COLORS] = {
  UTIL_LCD_COLOR_GREEN,
  UTIL_LCD_COLOR_RED,
  UTIL_LCD_COLOR_CYAN,
  UTIL_LCD_COLOR_MAGENTA,
  UTIL_LCD_COLOR_YELLOW,
  UTIL_LCD_COLOR_GRAY,
  UTIL_LCD_COLOR_BLACK,
  UTIL_LCD_COLOR_BROWN,
  UTIL_LCD_COLOR_BLUE,
  UTIL_LCD_COLOR_ORANGE
};

static int (*clamp_point)(int *x, int *y);
static void (*convert_length)(float32_t wi, float32_t hi, int *wo, int *ho);
static void (*convert_point)(float32_t xi, float32_t yi, int *xo, int *yo);
static void (*Display_binding_line)(int x0, int y0, int x1, int y1, uint32_t color);

static void Display_keypoint(mpe_pp_keyPoints_t *key, uint32_t color)
{
  int is_clamp;
  int xc, yc;
  int x, y;

  if (key->conf < AI_POSE_PP_CONF_THRESHOLD)
    return ;

  convert_point(key->x, key->y, &x, &y);
  xc = x - CIRCLE_RADIUS / 2;
  yc = y - CIRCLE_RADIUS / 2;
  is_clamp = clamp_point(&xc, &yc);
  xc = x + CIRCLE_RADIUS / 2;
  yc = y + CIRCLE_RADIUS / 2;
  is_clamp |= clamp_point(&xc, &yc);

  if (is_clamp)
    return ;

  UTIL_LCD_FillCircle(x, y, CIRCLE_RADIUS, color);
}

#ifdef USE_BINDINGS
static void Display_binding(mpe_pp_keyPoints_t *from, mpe_pp_keyPoints_t *to, uint32_t color)
{
  int is_clamp;
  int x0, y0;
  int x1, y1;
  int i;

  assert(BINDING_WIDTH % 2 == 1);

  if (from->conf < AI_POSE_PP_CONF_THRESHOLD)
    return ;
  if (to->conf < AI_POSE_PP_CONF_THRESHOLD)
    return ;

  convert_point(from->x, from->y, &x0, &y0);
  is_clamp = clamp_point(&x0, &y0);
  if (is_clamp)
    return ;

  convert_point(to->x, to->y, &x1, &y1);
  is_clamp = clamp_point(&x1, &y1);
  if (is_clamp)
    return ;

  UTIL_LCD_DrawLine(x0, y0, x1, y1, color);
  for (i = 1; i <= (BINDING_WIDTH - 1) / 2; i++) {
    if (abs(y1 - y0) > abs(x1 - x0)) {
      Display_binding_line(x0 + i, y0, x1 + i , y1, color);
      Display_binding_line(x0 - i, y0, x1 - i , y1, color);
    } else {
      Display_binding_line(x0, y0 + i, x1 , y1 + i, color);
      Display_binding_line(x0, y0 - i, x1 , y1 - i, color);
    }
  }

  Display_keypoint(from, color);
  Display_keypoint(to, color);
}
#endif

void Display_mpe_InitFunctions(int clamp_point_init(int *x, int *y),
                               void convert_length_init(float32_t wi, float32_t hi, int *wo, int *ho),
                               void convert_point_init(float32_t xi, float32_t yi, int *xo, int *yo),
                               void Display_binding_line_init(int x0, int y0, int x1, int y1, uint32_t color))
{
  clamp_point = clamp_point_init;
  convert_length = convert_length_init;
  convert_point = convert_point_init;
  Display_binding_line = Display_binding_line_init;
}

void Display_mpe_Detection(mpe_pp_outBuffer_t *detect)
{
  int xc, yc;
  int x0, y0;
  int x1, y1;
  int w, h;
  int i;

  convert_point(detect->x_center, detect->y_center, &xc, &yc);
  convert_length(detect->width, detect->height, &w, &h);
  x0 = xc - (w + 1) / 2;
  y0 = yc - (h + 1) / 2;
  x1 = xc + (w + 1) / 2;
  y1 = yc + (h + 1) / 2;
  clamp_point(&x0, &y0);
  clamp_point(&x1, &y1);

  UTIL_LCD_DrawRect(x0, y0, x1 - x0, y1 - y0, colors[detect->class_index % NUMBER_COLORS]);

#ifdef USE_BINDINGS
  for (i = 0; i < BINDINGS_NB; i++)
    Display_binding(&detect->pKeyPoints[bindings[i][0]], &detect->pKeyPoints[bindings[i][1]], bindings[i][2]);
#else
  for (i = 0; i < AI_POSE_PP_POSE_KEYPOINTS_NB; i++)
    Display_keypoint(&detect->pKeyPoints[i], DEFAULT_KEYPOINTS_COLOR);
#endif
}
