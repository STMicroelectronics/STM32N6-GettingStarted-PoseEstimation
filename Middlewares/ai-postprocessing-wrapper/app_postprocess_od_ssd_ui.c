 /**
 ******************************************************************************
 * @file    app_postprocess_od_ssd_ui.c
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

#if POSTPROCESS_TYPE == POSTPROCESS_OD_SSD_UI
#include <assert.h>
#include "ssd_anchors.h"

#define MODEL_OUTPUT_NB 2

POSTPROCESS_WRAPPER_SECTION
static od_pp_outBuffer_t out_detections[AI_OD_SSD_PP_TOTAL_DETECTIONS];
POSTPROCESS_WRAPPER_SECTION
static od_pp_outBuffer_t scratch_buffer[AI_OD_SSD_PP_TOTAL_DETECTIONS];
POSTPROCESS_WRAPPER_SECTION
static float32_t scratch_buffer_sm[AI_OD_SSD_PP_NB_CLASSES];
/* will contain model output index ordered as follow: boxes, scores */
static size_t output_order_index[MODEL_OUTPUT_NB];

int32_t app_postprocess_init(void *params_postprocess, stai_network_info *NN_Info)
{
  int32_t error = AI_OD_POSTPROCESS_ERROR_NO;

  assert(NN_Info);
  if (NN_Info->outputs[0].shape.data[1] == AI_OD_SSD_PP_NB_CLASSES) {
    output_order_index[0] = 1;
    output_order_index[1] = 0;
  } else {
    output_order_index[0] = 0;
    output_order_index[1] = 1;
  }

  od_ssd_pp_static_param_t *params = (od_ssd_pp_static_param_t *) params_postprocess;
  params->nb_classes = AI_OD_SSD_PP_NB_CLASSES;
  params->nb_detections = AI_OD_SSD_PP_TOTAL_DETECTIONS;
  params->XY_inv_scale = AI_OD_SSD_PP_XY_VARIANCE;
  params->WH_inv_scale = AI_OD_SSD_PP_WH_VARIANCE;
  params->max_boxes_limit = AI_OD_SSD_PP_MAX_BOXES_LIMIT;
  params->conf_threshold = AI_OD_SSD_PP_CONF_THRESHOLD;
  params->iou_threshold = AI_OD_SSD_PP_IOU_THRESHOLD;
  params->pAnchors = g_Anchors;
  params->pScratchBuffer = scratch_buffer;
  params->pScratchBufferSoftMax = scratch_buffer_sm;
  params->boxe_scale = NN_Info->outputs[output_order_index[0]].scale.data[0];
  params->boxe_zero_point = NN_Info->outputs[output_order_index[0]].zeropoint.data[0];
  params->score_scale = NN_Info->outputs[output_order_index[1]].scale.data[0];
  params->score_zero_point = NN_Info->outputs[output_order_index[1]].zeropoint.data[0];
  error = od_ssd_pp_reset(params);
  return error;
}

int32_t app_postprocess_run(void *pInput[], int nb_input, void *pOutput, void *pInput_param)
{
  assert(nb_input == MODEL_OUTPUT_NB);
  int32_t error = AI_OD_POSTPROCESS_ERROR_NO;
  ((od_ssd_pp_static_param_t *) pInput_param)->nb_detect = 0;
  od_pp_out_t *pObjDetOutput = (od_pp_out_t *) pOutput;
  pObjDetOutput->pOutBuff = out_detections;
  od_ssd_pp_in_centroid_t pp_input =
  {
      .pBoxes = pInput[output_order_index[0]],
      .pScores = pInput[output_order_index[1]],
  };
  error = od_ssd_pp_process_int8(&pp_input, pObjDetOutput,
                              (od_ssd_pp_static_param_t *) pInput_param);
  return error;
}
#endif
