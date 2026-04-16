 /**
 ******************************************************************************
 * @file    app_postprocess_iseg_yolo_v8_ui.c
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

#if POSTPROCESS_TYPE == POSTPROCESS_ISEG_YOLO_V8_UI
#include <assert.h>
#include "sort.h"

#define PP_OUTPUT_NB 2

POSTPROCESS_WRAPPER_SECTION
uint8_t _iseg_mask[AI_YOLOV8_SEG_PP_MASK_SIZE * AI_YOLOV8_SEG_PP_MASK_SIZE * AI_YOLOV8_SEG_PP_MAX_BOXES_LIMIT];
POSTPROCESS_WRAPPER_SECTION
iseg_pp_outBuffer_t out_detections[AI_YOLOV8_SEG_PP_MAX_BOXES_LIMIT];
POSTPROCESS_WRAPPER_SECTION
iseg_yolov8_pp_scratchBuffer_s8_t scratch_detections[AI_YOLOV8_SEG_PP_TOTAL_BOXES];
POSTPROCESS_WRAPPER_SECTION
float32_t _out_buf_mask[AI_YOLOV8_SEG_PP_MASK_NB];
POSTPROCESS_WRAPPER_SECTION
int8_t _out_buf_mask_s8[AI_YOLOV8_SEG_PP_MASK_NB * AI_YOLOV8_SEG_PP_TOTAL_BOXES];
/* will contain output index ordered by ascending output size */
static size_t output_order_index[PP_OUTPUT_NB];

int32_t app_postprocess_init(void *params_postprocess, stai_network_info *NN_Info)
{
  int32_t error = AI_ISEG_POSTPROCESS_ERROR_NO;
  iseg_yolov8_pp_static_param_t *params = (iseg_yolov8_pp_static_param_t *) params_postprocess;

  assert(NN_Info);
  sort_model_outputs(output_order_index, PP_OUTPUT_NB, NN_Info);

  params->nb_classes = AI_YOLOV8_SEG_PP_NB_CLASSES;
  params->nb_total_boxes = AI_YOLOV8_SEG_PP_TOTAL_BOXES;
  params->max_boxes_limit = AI_YOLOV8_SEG_PP_MAX_BOXES_LIMIT;
  params->conf_threshold = AI_YOLOV8_SEG_PP_CONF_THRESHOLD;
  params->iou_threshold = AI_YOLOV8_SEG_PP_IOU_THRESHOLD;
  params->size_masks = AI_YOLOV8_SEG_PP_MASK_SIZE;
  /* sorted[0]=masks (smaller), sorted[1]=raw_detections (larger) */
  params->raw_output_scale       = NN_Info->outputs[output_order_index[1]].scale.data[0];
  params->raw_output_zero_point  = NN_Info->outputs[output_order_index[1]].zeropoint.data[0];
  params->nb_masks = AI_YOLOV8_SEG_PP_MASK_NB;
  params->mask_raw_output_scale      = NN_Info->outputs[output_order_index[0]].scale.data[0];
  params->mask_raw_output_zero_point = NN_Info->outputs[output_order_index[0]].zeropoint.data[0];
  params->pMask = _out_buf_mask;
  params->pTmpBuff = scratch_detections;
  for (size_t i = 0; i < AI_YOLOV8_SEG_PP_TOTAL_BOXES; i++) {
    scratch_detections[i].pMask = &_out_buf_mask_s8[i * AI_YOLOV8_SEG_PP_MASK_NB];
  }
  for (size_t i = 0; i < AI_YOLOV8_SEG_PP_MAX_BOXES_LIMIT; i++) {
    out_detections[i].pMask = &_iseg_mask[i * AI_YOLOV8_SEG_PP_MASK_SIZE * AI_YOLOV8_SEG_PP_MASK_SIZE];
  }
  error = iseg_yolov8_pp_reset(params);
  return error;
}

int32_t app_postprocess_run(void *pInput[], int nb_input, void *pOutput, void *pInput_param)
{
  assert(nb_input == PP_OUTPUT_NB);
  int32_t error = AI_ISEG_POSTPROCESS_ERROR_NO;
  iseg_pp_out_t *pSegOutput = (iseg_pp_out_t *) pOutput;
  pSegOutput->pOutBuff = out_detections;
  iseg_yolov8_pp_in_centroid_t pp_input =
  {
      .pRaw_detections = (int8_t *) pInput[output_order_index[1]],
      .pRaw_masks      = (int8_t *) pInput[output_order_index[0]]
  };
  error = iseg_yolov8_pp_process_int8(&pp_input, pOutput,
                                      (iseg_yolov8_pp_static_param_t *) pInput_param);

  return error;
}
#endif
