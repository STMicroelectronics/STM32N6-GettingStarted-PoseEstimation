 /**
 ******************************************************************************
 * @file    app_postprocess_spe_movenet_ui.c
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
#include <assert.h>
#include <string.h>


#if POSTPROCESS_TYPE == POSTPROCESS_SPE_MOVENET_UI
static spe_pp_outBuffer_t out_detections[AI_POSE_PP_POSE_KEYPOINTS_NB];

static void movenet_decode_layout_int8(const int8_t *input,
                                       spe_pp_outBuffer_t *output,
                                       const spe_movenet_pp_static_param_t *params,
                                       int channel_last)
{
  const uint32_t width = params->heatmap_width;
  const uint32_t height = params->heatmap_height;
  const uint32_t nb_keypoints = params->nb_keypoints;
  const uint32_t plane_size = width * height;

  for (uint32_t keypoint = 0; keypoint < nb_keypoints; keypoint++)
  {
    int8_t best_raw = input[channel_last ? keypoint : (keypoint * plane_size)];
    uint32_t best_index = 0;

    for (uint32_t index = 1; index < plane_size; index++)
    {
      uint32_t raw_index = channel_last ? ((index * nb_keypoints) + keypoint) : ((keypoint * plane_size) + index);
      int8_t raw = input[raw_index];

      if (raw > best_raw)
      {
        best_raw = raw;
        best_index = index;
      }
    }

    output[keypoint].x_center = ((best_index % height) + 0.5f) / height;
    output[keypoint].y_center = ((best_index / height) + 0.5f) / width;
    output[keypoint].proba = params->raw_scale * (float32_t)((int32_t)best_raw - (int32_t)params->raw_zero_point);
  }
}

static uint32_t movenet_layout_spread_score(const spe_pp_outBuffer_t *output,
                                            const spe_movenet_pp_static_param_t *params)
{
  uint32_t score = 0;

  for (uint32_t i = 0; i < params->nb_keypoints; i++)
  {
    uint32_t xi = (uint32_t)(output[i].x_center * params->heatmap_width);
    uint32_t yi = (uint32_t)(output[i].y_center * params->heatmap_height);

    for (uint32_t j = 0; j < i; j++)
    {
      uint32_t xj = (uint32_t)(output[j].x_center * params->heatmap_width);
      uint32_t yj = (uint32_t)(output[j].y_center * params->heatmap_height);

      if ((xi != xj) || (yi != yj))
      {
        score++;
      }
    }
  }

  return score;
}

static float32_t movenet_layout_confidence_score(const spe_pp_outBuffer_t *output,
                                                 const spe_movenet_pp_static_param_t *params)
{
  float32_t score = 0.0f;

  for (uint32_t i = 0; i < params->nb_keypoints; i++)
  {
    score += output[i].proba;
  }

  return score;
}

int32_t app_postprocess_init(void *params_postprocess, stai_network_info *NN_Info)
{
  int32_t error = AI_SPE_POSTPROCESS_ERROR_NO;
  spe_movenet_pp_static_param_t *params = (spe_movenet_pp_static_param_t *) params_postprocess;

  params->raw_scale = NN_Info->outputs[0].scale.data[0];
  params->raw_zero_point = NN_Info->outputs[0].zeropoint.data[0];
  params->heatmap_width = AI_SPE_MOVENET_POSTPROC_HEATMAP_WIDTH;
  params->heatmap_height = AI_SPE_MOVENET_POSTPROC_HEATMAP_HEIGHT;
  params->nb_keypoints = AI_POSE_PP_POSE_KEYPOINTS_NB;
  error = spe_movenet_pp_reset(params);
  return error;
}

int32_t app_postprocess_run(void *pInput[], int nb_input, void *pOutput, void *pInput_param)
{
  assert(nb_input == 1);
  spe_pp_out_t *pPoseOutput = (spe_pp_out_t *) pOutput;
  spe_movenet_pp_static_param_t *params = (spe_movenet_pp_static_param_t *) pInput_param;
  int8_t *input = (int8_t *)pInput[0];
  spe_pp_outBuffer_t channel_last_detections[AI_POSE_PP_POSE_KEYPOINTS_NB];
  spe_pp_outBuffer_t channel_first_detections[AI_POSE_PP_POSE_KEYPOINTS_NB];
  uint32_t channel_last_spread;
  uint32_t channel_first_spread;
  float32_t channel_last_confidence;
  float32_t channel_first_confidence;

  pPoseOutput->pOutBuff = out_detections;

  movenet_decode_layout_int8(input, channel_last_detections, params, 1);
  movenet_decode_layout_int8(input, channel_first_detections, params, 0);

  channel_last_spread = movenet_layout_spread_score(channel_last_detections, params);
  channel_first_spread = movenet_layout_spread_score(channel_first_detections, params);
  channel_last_confidence = movenet_layout_confidence_score(channel_last_detections, params);
  channel_first_confidence = movenet_layout_confidence_score(channel_first_detections, params);

  if ((channel_first_spread > channel_last_spread) ||
      ((channel_first_spread == channel_last_spread) && (channel_first_confidence > channel_last_confidence)))
  {
    memcpy(out_detections, channel_first_detections, sizeof(out_detections));
  }
  else
  {
    memcpy(out_detections, channel_last_detections, sizeof(out_detections));
  }

  return AI_SPE_POSTPROCESS_ERROR_NO;
}
#endif
