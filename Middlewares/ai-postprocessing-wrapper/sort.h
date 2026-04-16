 /**
 ******************************************************************************
 * @file    sort.c
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

#include "stai.h"

#include <assert.h>
#include <stdlib.h>

static stai_network_info *NN_Info_current;

static int compare_order_by_ascending_order(const void *el1, const void *el2)
{
  stai_network_info *NN_Info = NN_Info_current;
  size_t *idx1 = (size_t *) el1;
  size_t *idx2 = (size_t *) el2;
  const stai_tensor *t1 = &NN_Info->outputs[*idx1];
  const stai_tensor *t2 = &NN_Info->outputs[*idx2];
  stai_size rank;
  int diff;

  diff = (int)t1->size_bytes - (int)t2->size_bytes;
  if (diff != 0)
    return diff;

  /* Secondary sort: compare shape dimensions (height, width, channel) */
  rank = t1->shape.size < t2->shape.size ? t1->shape.size : t2->shape.size;
  for (stai_size i = 0; i < rank; i++) {
    diff = (int)(t1->shape.data[i] - t2->shape.data[i]);
    if (diff != 0)
      return diff;
  }
  return (int)t1->shape.size - (int)t2->shape.size;
}

/* Function to sort model outputs based on their size in ascending order */
static void sort_model_outputs(size_t *outputs_idx, size_t size, stai_network_info *NN_Info)
{
  assert(NN_Info);
  assert(NN_Info->n_outputs == size);

  for (size_t i = 0; i < size; i++) {
    outputs_idx[i] = i;
  }

  NN_Info_current = NN_Info;
  qsort(outputs_idx, size, sizeof(size_t), compare_order_by_ascending_order);
  NN_Info_current = NULL;
}
