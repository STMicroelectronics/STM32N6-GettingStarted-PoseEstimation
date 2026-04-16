/*---------------------------------------------------------------------------------------------
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file in
  * the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *--------------------------------------------------------------------------------------------*/

#ifndef __OD_YOLO_D_PP_IF_H__
#define __OD_YOLO_D_PP_IF_H__


#ifdef __cplusplus
 extern "C" {
#endif

#include "od_pp_output_if.h"


/* I/O structures for Yolo-d detector type */
/* --------------------------------------- */
typedef struct od_yolo_d_pp_in_centroid
{
  void *pRaw_detections;
} od_yolo_d_pp_in_centroid_t;


typedef struct od_yolo_d_pp_static_param {
  int32_t  nb_classes;
  uint32_t height;
  uint32_t width;
  int32_t  max_boxes_limit;
  float32_t conf_threshold;
  float32_t iou_threshold;
  float32_t raw_output_scale;
  uint32_t nb_detect;
  uint8_t *strides;
  uint8_t strides_nb;
  int8_t raw_output_zero_point;
} od_yolo_d_pp_static_param_t;



/* Exported functions ------------------------------------------------------- */

/*!
 * @brief Resets object detection Yolo-d post processing
 *
 * @param [IN] Input static parameters
 * @retval Error code
 */
int32_t od_yolo_d_pp_reset(od_yolo_d_pp_static_param_t *pInput_static_param);


/*!
 * @brief Object detector post processing : includes output detector remapping,
 *        nms and score filtering for YoloV8.
 *
 * @param [IN] Pointer on input data
 *             Pointer on output data
 *             pointer on static parameters
 * @retval Error code
 */
int32_t od_yolo_d_pp_process(od_yolo_d_pp_in_centroid_t *pInput,
                             od_pp_out_t *pOutput,
                             od_yolo_d_pp_static_param_t *pInput_static_param);


/*!
 * @brief Object detector post processing : includes output detector remapping,
 *        nms and score filtering for Yolo-d with 8-bits quantized inputs.
 *
 * @param [IN] Pointer on input data
 *             Pointer on output data
 *             pointer on static parameters
 * @retval Error code
 */
int32_t od_yolo_d_pp_process_int8(od_yolo_d_pp_in_centroid_t *pInput,
                                  od_pp_out_t *pOutput,
                                  od_yolo_d_pp_static_param_t *pInput_static_param);

#ifdef __cplusplus
  }
#endif

#endif      /* __OD_YOLO_D_PP_IF_H__  */


