/*---------------------------------------------------------------------------------------------
 * Copyright (c) 2023 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file in
 * the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *--------------------------------------------------------------------------------------------*/

#include "od_pp_loc.h"
#include "od_ssd_pp_if.h"
#include "vision_models_pp.h"

/* Can't be removed if qsort is not re-written... */
static int32_t AI_SSD_PP_SORT_CLASS;

int32_t ssd_nms_comparator(const void *pa, const void *pb)
{
  od_pp_outBuffer_t *pa_s = (od_pp_outBuffer_t *)pa;
  od_pp_outBuffer_t *pb_s = (od_pp_outBuffer_t *)pb;
  float32_t a = (pa_s->class_index == AI_SSD_PP_SORT_CLASS) ? pa_s->conf : 0;
  float32_t b = (pb_s->class_index == AI_SSD_PP_SORT_CLASS) ? pb_s->conf : 0;
  float32_t diff = 0;

  diff = a - b;

  if (diff < 0) return 1;
  else if (diff > 0) return -1;
  return 0;
}

static int32_t SSD_quick_sort_partition(float32_t *pScores,
                                        float32_t *pBoxes,
                                        int32_t first,
                                        int32_t last,
                                        int32_t dir,
                                        int32_t ssd_sort_class,
                                        int32_t ssd_nb_classes)
{
    int32_t i, j, pivot_index;
    float32_t pivot;
    pivot_index = first;
    pivot = pScores[pivot_index * ssd_nb_classes + ssd_sort_class];
    i = first - 1;
    j = last + 1;

    while (i < j)
    {
        if (dir)
        {
            do
            {
                i++;
            } while ((pScores[i * ssd_nb_classes + ssd_sort_class] < pivot) &&
                     (i < last));
            do
            {
                j--;
            } while (pScores[j * ssd_nb_classes + ssd_sort_class] > pivot);
        }
        else
        {
            do
            {
                i++;
            } while ((pScores[i * ssd_nb_classes + ssd_sort_class] > pivot) &&
                     (i < last));
            do
            {
                j--;
            } while (pScores[j * ssd_nb_classes + ssd_sort_class] < pivot);
        }

        if (i < j)
        {
            for (int _i = 0; _i < AI_SSD_PP_BOX_STRIDE; _i++){
              float32_t _tmp = pBoxes[i * AI_SSD_PP_BOX_STRIDE + _i];
              pBoxes[i * AI_SSD_PP_BOX_STRIDE + _i] = pBoxes[j * AI_SSD_PP_BOX_STRIDE + _i];
              pBoxes[j * AI_SSD_PP_BOX_STRIDE + _i] = _tmp;
            }
            for (int _i = 0; _i < ssd_nb_classes; _i++){
              float32_t _tmp = pScores[i * ssd_nb_classes + _i];
              pScores[i * ssd_nb_classes + _i] = pScores[j * ssd_nb_classes + _i];
              pScores[j * ssd_nb_classes + _i] = _tmp;
            }
        }
    }
    return j;
}

static void SSD_quick_sort_core(float32_t *pScores,
                                float32_t *pBoxes,
                                int32_t first,
                                int32_t last,
                                int32_t dir,
                                int32_t ssd_sort_class,
                                int32_t ssd_nb_classes)
{
    /*
     dir 0  : descending
     dir 1  : ascending
    */
    if (first < last)
    {
        int32_t pivot;
        pivot = SSD_quick_sort_partition(pScores,
                                         pBoxes,
                                         first,
                                         last,
                                         dir,
                                         ssd_sort_class,
                                         ssd_nb_classes);
        SSD_quick_sort_core(pScores,
                            pBoxes,
                            first,
                            pivot,
                            dir,
                            ssd_sort_class,
                            ssd_nb_classes);
        SSD_quick_sort_core(pScores,
                            pBoxes,
                            pivot + 1,
                            last,
                            dir,
                            ssd_sort_class,
                            ssd_nb_classes);
    }
}



/**
 *
 * int32_t ssd_pp_getNNBoxes
 *
**/
/* Overwrite inputs */
int32_t ssd_pp_getNNBoxes_in_place(od_ssd_pp_in_centroid_t *pInput,
                                   od_ssd_pp_static_param_t *pInput_static_param)
{
  float32_t *pScoresIn  = (float32_t *)pInput->pScores;
  float32_t *pScoresOut  = (float32_t *)pInput->pScores;
  float32_t *pBoxesIn   = (float32_t *)pInput->pBoxes;
  float32_t *pBoxesOut   = (float32_t *)pInput->pBoxes;
  float32_t *pAnchors = (float32_t *)pInput_static_param->pAnchors;
  uint32_t nb_detect = 0;

  for (int32_t i = 0; i < pInput_static_param->nb_detections; i++)
  {
    // Max (removing first elements
    float32_t best_score;
    uint32_t class_index;
    vision_models_maxi_if32ou32(pScoresIn + 1,
                                pInput_static_param->nb_classes - 1,
                                &best_score,
                                &class_index);
    int bSkip = 0;
    float32_t max = MAX(best_score, pScoresIn[0]);
    if (expf(best_score - max) < pInput_static_param->conf_threshold * ( expf(best_score - max) + expf(pScoresIn[0] - max)) ) {
      bSkip = 1;
    }

    if (bSkip == 0)
    {
      class_index += 1;
      // Softmax
      vision_models_softmax_f(pScoresIn, pScoresIn, pInput_static_param->nb_classes, pScoresIn);
      best_score = pScoresIn[class_index];


      if (best_score >= pInput_static_param->conf_threshold)
      {
          // Discard class 0 (background)
          pScoresOut[0] = 0.0f;
          for (int32_t k = 1; k < pInput_static_param->nb_classes; ++k)
          {
              pScoresOut[k] = pScoresIn[k];
          }
          pBoxesOut[AI_SSD_PP_CENTROID_XCENTER]   =
                pBoxesIn[AI_SSD_PP_CENTROID_XCENTER] \
                * pInput_static_param->XY_inv_scale \
                * pAnchors[AI_SSD_PP_CENTROID_WIDTHREL] \
                + pAnchors[AI_SSD_PP_CENTROID_XCENTER];
          pBoxesOut[AI_SSD_PP_CENTROID_YCENTER]   =
                pBoxesIn[AI_SSD_PP_CENTROID_YCENTER] \
                * pInput_static_param->XY_inv_scale \
                * pAnchors[AI_SSD_PP_CENTROID_HEIGHTREL] \
                + pAnchors[AI_SSD_PP_CENTROID_YCENTER];
          pBoxesOut[AI_SSD_PP_CENTROID_WIDTHREL]  = expf(pBoxesIn[AI_SSD_PP_CENTROID_WIDTHREL] \
                                                    * pInput_static_param->WH_inv_scale) \
                                                    * pAnchors[AI_SSD_PP_CENTROID_WIDTHREL];
          pBoxesOut[AI_SSD_PP_CENTROID_HEIGHTREL] = expf(pBoxesIn[AI_SSD_PP_CENTROID_HEIGHTREL] \
                                                    * pInput_static_param->WH_inv_scale) \
                                                    * pAnchors[AI_SSD_PP_CENTROID_HEIGHTREL];

          pBoxesOut  += AI_SSD_PP_BOX_STRIDE;
          pScoresOut += pInput_static_param->nb_classes;
          nb_detect++;
      }
    }
    pBoxesIn  += AI_SSD_PP_BOX_STRIDE;
    pAnchors  += AI_SSD_PP_BOX_STRIDE;
    pScoresIn += pInput_static_param->nb_classes;
  }
  pInput_static_param->nb_detect = nb_detect;


  return (AI_OD_POSTPROCESS_ERROR_NO);
}

int32_t ssd_pp_getNNBoxes_scratchBuffer(od_ssd_pp_in_centroid_t *pInput,
                                        od_pp_outBuffer_t *pScratchBufferIn,
                                        od_ssd_pp_static_param_t *pInput_static_param)
{
  float32_t * restrict pScoresIn = (float32_t *)pInput->pScores;
  float32_t * restrict pBoxesIn = (float32_t *)pInput->pBoxes;
  float32_t * restrict pAnchors = (float32_t *)pInput_static_param->pAnchors;
  uint32_t nb_detect = 0;
  od_pp_outBuffer_t *pScratchBuffer = pScratchBufferIn;


  for (int32_t i = 0; i < pInput_static_param->nb_detections; i++)
  {
    float32_t best_score;
    uint32_t class_index;
    vision_models_maxi_if32ou32(pScoresIn + 1,
                                pInput_static_param->nb_classes-1,
                                &best_score,
                                &class_index);
    int bSkip = 0;
    float32_t max = MAX(best_score, pScoresIn[0]);
    if (expf(best_score - max) < pInput_static_param->conf_threshold * ( expf(best_score - max) + expf(pScoresIn[0] - max))) {
      bSkip = 1;
    }

    if (bSkip == 0)
    {
      class_index+=1;
      // Softmax
      vision_models_softmax_f(pScoresIn, pScoresIn, pInput_static_param->nb_classes, pScoresIn);
      best_score = pScoresIn[class_index];

      if (best_score >= pInput_static_param->conf_threshold)
      {
        pScratchBuffer->class_index = class_index;
        pScratchBuffer->conf = best_score;
        pScratchBuffer->x_center = pBoxesIn[AI_SSD_PP_CENTROID_XCENTER] \
                                  * pInput_static_param->XY_inv_scale \
                                  * pAnchors[AI_SSD_PP_CENTROID_WIDTHREL] \
                                  + pAnchors[AI_SSD_PP_CENTROID_XCENTER];
        pScratchBuffer->y_center = pBoxesIn[AI_SSD_PP_CENTROID_YCENTER] \
                                  * pInput_static_param->XY_inv_scale \
                                  * pAnchors[AI_SSD_PP_CENTROID_HEIGHTREL] \
                                  + pAnchors[AI_SSD_PP_CENTROID_YCENTER];
        pScratchBuffer->width  = expf(pBoxesIn[AI_SSD_PP_CENTROID_WIDTHREL] \
                                      * pInput_static_param->WH_inv_scale) \
                                      * pAnchors[AI_SSD_PP_CENTROID_WIDTHREL];
        pScratchBuffer->height = expf(pBoxesIn[AI_SSD_PP_CENTROID_HEIGHTREL] \
                                      * pInput_static_param->WH_inv_scale) \
                                      * pAnchors[AI_SSD_PP_CENTROID_HEIGHTREL];

        pScratchBuffer++;
        nb_detect++;
      }
    }
    pBoxesIn  += AI_SSD_PP_BOX_STRIDE;
    pAnchors  += AI_SSD_PP_BOX_STRIDE;
    pScoresIn += pInput_static_param->nb_classes;

  }
  pInput_static_param->nb_detect = nb_detect;

  return (AI_OD_POSTPROCESS_ERROR_NO);
}


int32_t ssd_pp_getNNBoxes_int8_scratchBuffer(od_ssd_pp_in_centroid_t *pInput,
                                             od_pp_outBuffer_t *pScratchBuffer,
                                             od_ssd_pp_static_param_t *pInput_static_param)
{
  int8_t *pScoresIn = (int8_t *)pInput->pScores;
  int8_t *pBoxesIn  = (int8_t *)pInput->pBoxes;
  float32_t *pAnchors  = (float32_t *)pInput_static_param->pAnchors;
  uint32_t nb_detect = 0;

  int8_t boxe_zp         = pInput_static_param->boxe_zero_point;
  float32_t boxe_scale   = pInput_static_param->boxe_scale;

  int8_t score_zp        = pInput_static_param->score_zero_point;
  float32_t score_scale  = pInput_static_param->score_scale;

  for (int32_t i = 0; i < pInput_static_param->nb_detections; i++)
  {
    int8_t best_score_i8;
    uint8_t class_index_u8;
    float32_t best_score;

    // Max removing backrground
    vision_models_maxi_is8ou8(pScoresIn+1,
                              pInput_static_param->nb_classes-1,
                              &best_score_i8,
                              &class_index_u8);
    // first rough check reduced softmax
    // exp(x) / (exp(x) + exp(x0)) > threshold
    int bSkip = 0;
    int8_t max = MAX(best_score_i8, pScoresIn[0]);
    if (expf((best_score_i8-max)*score_scale) < pInput_static_param->conf_threshold * ( expf((best_score_i8 - max)*score_scale) + expf((pScoresIn[0] - max)*score_scale))) {
      bSkip = 1;
    }

    if (bSkip == 0) {
      class_index_u8 += 1;
      float32_t *pScratch_buffer_float = pInput_static_param->pScratchBufferSoftMax;
      // Dequantize
      for (int e = 0; e < pInput_static_param->nb_classes; e++) {
        pScratch_buffer_float[e] = (float32_t)((int32_t)pScoresIn[e] - score_zp) * score_scale;
      }
      // Softmax
      vision_models_softmax_f(pScratch_buffer_float, pScratch_buffer_float, pInput_static_param->nb_classes, pScratch_buffer_float);
      best_score = pScratch_buffer_float[class_index_u8];

      if (best_score >= pInput_static_param->conf_threshold)
      {
        float32_t value, anchor_rel_x, anchor_rel_y, anchor_size_w, anchor_size_h;
        pScratchBuffer[nb_detect].class_index = class_index_u8;
        pScratchBuffer[nb_detect].conf = best_score;

        value         = (float32_t)((int32_t)  pBoxesIn[AI_SSD_PP_CENTROID_XCENTER]   - boxe_zp )  * boxe_scale;
        anchor_rel_x  = (float32_t)pAnchors[AI_SSD_PP_CENTROID_XCENTER];
        anchor_size_w = (float32_t)pAnchors[AI_SSD_PP_CENTROID_WIDTHREL];
        pScratchBuffer[nb_detect].x_center =
            value \
            * pInput_static_param->XY_inv_scale \
            * anchor_size_w \
            + anchor_rel_x;

        value         = (float32_t)((int32_t)  pBoxesIn[AI_SSD_PP_CENTROID_YCENTER]   - boxe_zp )  * boxe_scale;
        anchor_rel_y  = (float32_t)pAnchors[AI_SSD_PP_CENTROID_YCENTER];
        anchor_size_h = (float32_t)pAnchors[AI_SSD_PP_CENTROID_HEIGHTREL];
        pScratchBuffer[nb_detect].y_center = value * pInput_static_param->XY_inv_scale * anchor_size_h + anchor_rel_y;

        value         = (float32_t)((int32_t)  pBoxesIn[AI_SSD_PP_CENTROID_WIDTHREL]  - boxe_zp )  * boxe_scale;
        pScratchBuffer[nb_detect].width  = expf(value * pInput_static_param->WH_inv_scale) * anchor_size_w;

        value         = (float32_t)((int32_t)  pBoxesIn[AI_SSD_PP_CENTROID_HEIGHTREL] - boxe_zp )  * boxe_scale;
        pScratchBuffer[nb_detect].height = expf(value * pInput_static_param->WH_inv_scale) * anchor_size_h;

        nb_detect++;
      } // if > thrshold
    }
    pBoxesIn  += AI_SSD_PP_BOX_STRIDE;
    pAnchors  += AI_SSD_PP_BOX_STRIDE;
    pScoresIn += pInput_static_param->nb_classes;

  } // for i
  pInput_static_param->nb_detect = nb_detect;

  return (AI_OD_POSTPROCESS_ERROR_NO);
}



int32_t ssd_pp_nms_filtering(od_ssd_pp_in_centroid_t *pInput,
                             od_ssd_pp_static_param_t *pInput_static_param)
{
  int32_t i, j, k, limit_counter;
  float32_t *pScores = (float32_t *)pInput->pScores;
  float32_t *pBoxes = (float32_t *)pInput->pBoxes;

    for (k = 0; k < pInput_static_param->nb_classes; ++k)
    {
        limit_counter = 0;
        AI_SSD_PP_SORT_CLASS = k;

        SSD_quick_sort_core(pScores,
                            pBoxes,
                            0,
                            pInput_static_param->nb_detect - 1,
                            0,
                            AI_SSD_PP_SORT_CLASS,
                            pInput_static_param->nb_classes);

        for (i = 0; i < pInput_static_param->nb_detect; ++i)
        {
            if (pScores[i * pInput_static_param->nb_classes + k] == 0)
            {
                continue;
            }
            float32_t *pA = &(pBoxes[AI_SSD_PP_BOX_STRIDE * i + AI_SSD_PP_CENTROID_XCENTER]);
            for (j = i + 1; j < pInput_static_param->nb_detect; ++j)
            {
                float32_t *pB = &(pBoxes[AI_SSD_PP_BOX_STRIDE * j + AI_SSD_PP_CENTROID_XCENTER]);
                if (vision_models_box_iou(pA, pB) > pInput_static_param->iou_threshold)
                {
                    pScores[j * pInput_static_param->nb_classes + k] = 0;
                }
            }
        }

        for (int32_t it = 0; it < pInput_static_param->nb_detect; ++it)
        {
            if ((pScores[it * pInput_static_param->nb_classes + k] != 0) &&
                (limit_counter < pInput_static_param->max_boxes_limit))
            {
                limit_counter++;
            }
            else
            {
                pScores[it * pInput_static_param->nb_classes + k] = 0;
            }
        }
    }

    return (AI_OD_POSTPROCESS_ERROR_NO);
}

int32_t ssd_pp_nms_filtering_scratchBuffer(od_pp_outBuffer_t *pScratchBuffer,
                                           od_ssd_pp_static_param_t *pInput_static_param)
{
  int32_t i, j, k, limit_counter;

    for (k = 0; k < pInput_static_param->nb_classes; ++k)
    {
        limit_counter = 0;
        int32_t detections_per_class = 0;
        AI_SSD_PP_SORT_CLASS = k;

        /* Counts the number of detections with class k */
        for (int32_t i = 0; i < pInput_static_param->nb_detect ; i ++)
        {
            if(pScratchBuffer[i].class_index == k)
            {
                detections_per_class++;
            }
        }

        if (detections_per_class > 0)
        {

          qsort(pScratchBuffer,
              pInput_static_param->nb_detect,
              sizeof(od_pp_outBuffer_t),
              (_Cmpfun *)ssd_nms_comparator);
        for (i = 0; i < detections_per_class; i++)
        {
            if (pScratchBuffer[i].conf == 0.0)
            {
                continue;
            }
            float32_t *pA = &(pScratchBuffer[i].x_center);
            for (j = i + 1; j < detections_per_class; j++)
            {
                if (pScratchBuffer[j].conf == 0.0)
                {
                  continue;
                }
                float32_t *pB = &(pScratchBuffer[j].x_center);
                if (vision_models_box_iou(pA, pB) > pInput_static_param->iou_threshold)
                {
                    pScratchBuffer[j].conf = 0.0;
                }
            }
        }

        for (int32_t it = 0; it < detections_per_class; ++it)
        {
            if ((pScratchBuffer[it].conf != 0.0) &&
                (limit_counter < pInput_static_param->max_boxes_limit))
            {
                limit_counter++;
            }
            else
            {

                pScratchBuffer[it].conf = 0.0;
            }
        }
        } // if detections
    } // for k in classes

    return (AI_OD_POSTPROCESS_ERROR_NO);
}




int32_t ssd_pp_score_filtering(od_ssd_pp_in_centroid_t *pInput,
                               od_pp_out_t *pOutput,
                               od_ssd_pp_static_param_t *pInput_static_param)
{
  float32_t *pScores = (float32_t *)pInput->pScores;
  float32_t *pBoxes = (float32_t *)pInput->pBoxes;


    int32_t count = 0;
    for (int32_t i = 0; i < pInput_static_param->nb_detect; i++)
    {
      uint32_t class_index;
      float32_t best_score;
      vision_models_maxi_if32ou32(pScores + 1,
                                  pInput_static_param->nb_classes-1,
                                  &best_score,
                                  &class_index);

      if (best_score >= pInput_static_param->conf_threshold)
      {
          pOutput->pOutBuff[count].class_index = class_index + 1;
          pOutput->pOutBuff[count].conf = best_score ;
          pOutput->pOutBuff[count].x_center = pBoxes[AI_SSD_PP_CENTROID_XCENTER];
          pOutput->pOutBuff[count].y_center = pBoxes[AI_SSD_PP_CENTROID_YCENTER];
          pOutput->pOutBuff[count].width    = pBoxes[AI_SSD_PP_CENTROID_WIDTHREL];
          pOutput->pOutBuff[count].height   = pBoxes[AI_SSD_PP_CENTROID_HEIGHTREL];

          count++;
      }
      pBoxes+=AI_SSD_PP_BOX_STRIDE;
      pScores+=pInput_static_param->nb_classes;

    }
    pOutput->nb_detect = count;
  return (AI_OD_POSTPROCESS_ERROR_NO);
}

int32_t ssd_pp_score_filtering_scratchBuffer(od_pp_outBuffer_t *pScratch,
                               od_pp_out_t *pOutput,
                               od_ssd_pp_static_param_t *pInput_static_param)
{

  int32_t count = 0;
  od_pp_outBuffer_t *pOutBuff = pOutput->pOutBuff;
  for (int32_t i = 0; i < pInput_static_param->nb_detect; i++)
  {
    if (pScratch->conf != 0.0)
    {
      memcpy(pOutBuff, pScratch, sizeof(od_pp_outBuffer_t));
      pOutBuff++;
      count++;
    }
    pScratch++;
  }

  pOutput->nb_detect = count;
  return (AI_OD_POSTPROCESS_ERROR_NO);
}

/* ----------------------       Exported routines      ---------------------- */

int32_t od_ssd_pp_reset(od_ssd_pp_static_param_t *pInput_static_param)
{
    /* Initializations */
    pInput_static_param->nb_detect = 0;

    return (AI_OD_POSTPROCESS_ERROR_NO);
}


int32_t od_ssd_pp_process(od_ssd_pp_in_centroid_t *pInput,
                          od_pp_out_t *pOutput,
                          od_ssd_pp_static_param_t *pInput_static_param)
{
  int32_t error = AI_OD_POSTPROCESS_ERROR_NO;

  od_pp_outBuffer_t *pScratchBuffer = pInput_static_param->pScratchBuffer;
  /* if no scratch buffer is specified and space enough in score array, use it as scratch buffer */
  if (   (pScratchBuffer == NULL)
      && (pInput_static_param->nb_classes * sizeof(float32_t) >= sizeof(od_pp_outBuffer_t))) {
    pScratchBuffer = (od_pp_outBuffer_t *)pInput->pScores;
  }
  if ( (pOutput->pOutBuff == NULL))
  {
    if (pInput_static_param->nb_classes * sizeof(float32_t) >= sizeof(od_pp_outBuffer_t))
    {
      pOutput->pOutBuff = (od_pp_outBuffer_t *)pInput->pScores;
    }
    else
    {
      return AI_OD_POSTPROCESS_ERROR;
    }
  }

  if (pScratchBuffer)
  {
    /* Calls Get NN boxes first */
    error = ssd_pp_getNNBoxes_scratchBuffer(pInput,
                                            pScratchBuffer,
                                            pInput_static_param);
    if (error != AI_OD_POSTPROCESS_ERROR_NO) return (error);

    /* Then NMS */
    error = ssd_pp_nms_filtering_scratchBuffer(pScratchBuffer,
                                               pInput_static_param);
    if (error != AI_OD_POSTPROCESS_ERROR_NO) return (error);

    /* And score re-filtering */
    error = ssd_pp_score_filtering_scratchBuffer(pScratchBuffer,
                                                 pOutput,
                                                 pInput_static_param);

  }
  else
  {

  /* Calls Get NN boxes first */
  error = ssd_pp_getNNBoxes_in_place(pInput,
                                     pInput_static_param);
  if (error != AI_OD_POSTPROCESS_ERROR_NO) return (error);

  /* Then NMS */
  error = ssd_pp_nms_filtering(pInput,
                               pInput_static_param);
  if (error != AI_OD_POSTPROCESS_ERROR_NO) return (error);

  /* And score re-filtering */
  error = ssd_pp_score_filtering(pInput,
                                 pOutput,
                                 pInput_static_param);

  }

  return (error);
}
int32_t od_ssd_pp_process_int8(od_ssd_pp_in_centroid_t *pInput,
                               od_pp_out_t *pOutput,
                               od_ssd_pp_static_param_t *pInput_static_param)
{
  int32_t error = AI_OD_POSTPROCESS_ERROR_NO;

  od_pp_outBuffer_t *pScratchBuffer = pInput_static_param->pScratchBuffer;
  /* if no scratch buffer is specified and space enough in score array, use it as scratch buffer */
  if (pScratchBuffer == NULL)
  {
    if (pInput_static_param->nb_classes * sizeof(int8_t) >= sizeof(od_pp_outBuffer_t))
    {
      pScratchBuffer = (od_pp_outBuffer_t *)pInput->pScores;
    }
    else
    {
      // no scratch buffer available
      return AI_OD_POSTPROCESS_ERROR;
    }
  }
  if ( (pOutput->pOutBuff == NULL))
  {
    if (pInput_static_param->nb_classes * sizeof(int8_t) >= sizeof(od_pp_outBuffer_t))
    {
      pOutput->pOutBuff = (od_pp_outBuffer_t *)pInput->pScores;
    }
    else
    {
      return AI_OD_POSTPROCESS_ERROR;
    }
  }

  /* Calls Get NN boxes first */
  error = ssd_pp_getNNBoxes_int8_scratchBuffer(pInput,
                                               pScratchBuffer,
                                               pInput_static_param);
  if (error != AI_OD_POSTPROCESS_ERROR_NO) return (error);

  /* Then NMS */
  error = ssd_pp_nms_filtering_scratchBuffer(pScratchBuffer,
                                             pInput_static_param);
  if (error != AI_OD_POSTPROCESS_ERROR_NO) return (error);

  /* And score re-filtering */
  error = ssd_pp_score_filtering_scratchBuffer(pScratchBuffer,
                                               pOutput,
                                               pInput_static_param);

  return (error);
}
