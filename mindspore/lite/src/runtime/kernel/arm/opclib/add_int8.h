/**
 * Copyright 2020 Huawei Technologies Co., Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef MINDSPORE_LITE_SRC_RUNTIME_KERNEL_ARM_OPCLIB_ADD_INT8_H_
#define MINDSPORE_LITE_SRC_RUNTIME_KERNEL_ARM_OPCLIB_ADD_INT8_H_

#include "src/runtime/kernel/arm/opclib/op_base.h"

struct AddQuantParameter {
  int input0_offset_;
  int input1_offset_;
  int output_offset_;
  float input0_scale_;
  float input1_scale_;
  float output_scale_;
  int input0_multiplier_;
  int input1_multiplier_;
  int output_multiplier_;
  int input0_shift_;
  int input1_shift_;
  int output_shift_;
  int output_activation_min_;
  int output_activation_max_;
  int left_shift_result0_;
  int left_shift_result1_;
  int right_shift0_;
  int right_shift1_;
  int left_shift_out_;
  int right_shift_out_;
};

void AddInt8(int8_t *input0_data, int8_t *input1_data, int8_t *output_data, int64_t real_dst_count,
             AddQuantParameter *para);

#ifdef ENABLE_NEON
#include <arm_neon.h>
int16x8_t LoadAndAddOffset(int8_t *data, int index, int offset);
#endif

#endif  // MINDSPORE_LITE_SRC_RUNTIME_KERNEL_ARM_OPCLIB_ADD_INT8_H_

