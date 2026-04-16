 /**
 ******************************************************************************
 * @file    app_postprocess_od_yolo_d_ui.c
 * @author  GPM Application Team
 *
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2023 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */


#include "app_postprocess.h"
#include "app_config.h"

#if POSTPROCESS_TYPE == POSTPROCESS_OD_ST_YOLOD_UI
#include <assert.h>

POSTPROCESS_WRAPPER_SECTION
static od_pp_outBuffer_t out_detections[(AI_OD_YOLO_D_PP_IMG_WIDTH / AI_OD_YOLO_D_PP_STRIDE_0) * ( AI_OD_YOLO_D_PP_IMG_HEIGHT / AI_OD_YOLO_D_PP_STRIDE_0)
                                      + (AI_OD_YOLO_D_PP_IMG_WIDTH / AI_OD_YOLO_D_PP_STRIDE_1) * ( AI_OD_YOLO_D_PP_IMG_HEIGHT / AI_OD_YOLO_D_PP_STRIDE_1)
                                      + (AI_OD_YOLO_D_PP_IMG_WIDTH / AI_OD_YOLO_D_PP_STRIDE_2) * ( AI_OD_YOLO_D_PP_IMG_HEIGHT / AI_OD_YOLO_D_PP_STRIDE_2)];
static uint8_t strides[3] = {AI_OD_YOLO_D_PP_STRIDE_0, AI_OD_YOLO_D_PP_STRIDE_1, AI_OD_YOLO_D_PP_STRIDE_2};

int32_t app_postprocess_init(void *params_postprocess, stai_network_info *NN_Info)
{
  int32_t error = AI_OD_POSTPROCESS_ERROR_NO;

  assert(NN_Info);

  od_yolo_d_pp_static_param_t *params = (od_yolo_d_pp_static_param_t *) params_postprocess;
  params->nb_classes = AI_OD_YOLO_D_PP_NB_CLASSES;
  params->height = AI_OD_YOLO_D_PP_IMG_HEIGHT;
  params->width = AI_OD_YOLO_D_PP_IMG_WIDTH;
  params->max_boxes_limit = AI_OD_YOLO_D_PP_MAX_BOXES_LIMIT;
  params->conf_threshold = AI_OD_YOLO_D_PP_CONF_THRESHOLD;
  params->iou_threshold = AI_OD_YOLO_D_PP_IOU_THRESHOLD;
  params->strides = strides;
  params->strides_nb = 3;
  params->raw_output_scale = NN_Info->outputs[0].scale.data[0];
  params->raw_output_zero_point = NN_Info->outputs[0].zeropoint.data[0];
  error = od_yolo_d_pp_reset(params);
  return error;
}

int32_t app_postprocess_run(void *pInput[], int nb_input, void *pOutput, void *pInput_param)
{
  assert(nb_input == 1);
  int32_t error = AI_OD_POSTPROCESS_ERROR_NO;
  ((od_yolo_d_pp_static_param_t *) pInput_param)->nb_detect = 0;
  od_pp_out_t *pObjDetOutput = (od_pp_out_t *) pOutput;
  pObjDetOutput->pOutBuff = out_detections;
  od_yolo_d_pp_in_centroid_t pp_input =
  {
      .pRaw_detections = pInput[0],
  };
  error = od_yolo_d_pp_process_int8(&pp_input, pObjDetOutput,
                              (od_yolo_d_pp_static_param_t *) pInput_param);
  return error;
}
#endif
