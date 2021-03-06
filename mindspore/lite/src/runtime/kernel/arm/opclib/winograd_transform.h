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

#ifndef MINDSPORE_LITE_SRC_RUNTIME_KERNEL_ARM_OPCLIB_WINOGRAD_TRANSFORM_H_
#define MINDSPORE_LITE_SRC_RUNTIME_KERNEL_ARM_OPCLIB_WINOGRAD_TRANSFORM_H_

#ifdef ENABLE_ARM
#include <arm_neon.h>
#endif
#include <string.h>
#include "src/runtime/kernel/arm/opclib/pack.h"
#include "src/runtime/kernel/arm/opclib/fp32/conv.h"
#include "src/runtime/kernel/arm/opclib/winograd_utils.h"
#include "src/runtime/kernel/arm/opclib/quantization/fixed_point.h"

#define OUPUT_UNIT 2

// for fp32 winograd input/output transform
void WinogradInputTransform(const float *input_data, float *trans_input, float *tmp_data, int cal_num,
                            int out_tile_index, int out_w_block_num, ConvParameter *conv_param,
                            InputTransformUnitFunc input_trans_func);

void WinogradOutputTransform(const float *gemm_out, float *tmp_out_data, const float *bias_data, int cal_num,
                             int out_tile_index, int output_unit_num, ConvParameter *conv_param,
                             OutputTransformUnitFunc output_trans_func);

// for fp32 convolution 3x3 filter/input/output transform
void Conv3x3Fp32InputUnit(const float *tmp_data, float *trans_input_data, size_t step);

void Conv3x3Fp32InputTransform(const float *input_data, float *trans_input, float *tmp_data, int start_index,
                               int real_cal_num, int out_w_block, ConvParameter *conv_param);

void Conv3x3Fp32FilterTransform(float *weight_data, float *trans_weight, int iC4, int output_channel, int kernel_plane);

void Conv3x3Fp32OutputUnit(const float *gemm_out, const float *bias_data, float *output_data, bool h_not_bound,
                           bool w_not_bound, int output_w);

void Conv3x3Fp32OutputTransform(const float *gemm_out, float *out_data, const float *bias_data, int start_index,
                                int real_cal_num, int out_w_block, ConvParameter *conv_param);

#ifdef ENABLE_FP16
// for fp16 convolution 3x3 filter/input/output transform
void Conv3x3Fp16InputUnit(float16_t *tmp_data, float16_t *trans_input_data, size_t step);

void Conv3x3Fp16InputTransform(const float16_t *input_data, float16_t *trans_input, float16_t *tmp_data,
                               int start_index, int real_cal_num, int out_w_block, ConvParameter *conv_param);

void Conv3x3Fp16FilterTransform(const float16_t *weight_data, float16_t *trans_weight, int iC8, int output_channel,
                                int kernel_plane);

void Conv3x3Fp16OutputUnit(const float16_t *gemm_out, const float16_t *bias_data, float16_t *output_data, int output_w);

void Conv3x3Fp16OutputTransform(const float16_t *gemm_out, float16_t *out_data, const float16_t *bias_data,
                                int start_index, int real_cal_num, int out_w_block, ConvParameter *conv_param);
#endif

// for int8 convolution 3x3 filter/input/output transform
void Conv3x3Uint8InputUnit(int16_t *tmp_data, int16_t *trans_input_data, size_t step, int input_zp);

void Conv3x3Uint8InputTransform(const int16_t *input_data, int16_t *trans_input, int16_t *tmp_data, int start_index,
                                int real_cal_num, int out_w_block, ConvParameter *conv_param);

void Conv3x3Int8FilterTransform(const int16_t *weight_data, int16_t *trans_weight, int iC8, int output_channel,
                                int kernel_plane);

void Conv3x3Uint8OutputUnit(const int32_t *gemm_out, const int32_t *bias_data, int8_t *output_data, bool h_not_bound,
                            bool w_not_bound, int output_w, int real_num, ConvParameter *conv_param);

void Conv3x3Uint8OutputTransform(const int32_t *gemm_out, int8_t *out_data, const int32_t *bias_data, int start_index,
                                 int real_cal_num, int out_w_block, ConvParameter *conv_param);

#endif  // MINDSPORE_LITE_SRC_RUNTIME_KERNEL_ARM_OPCLIB_WINOGRAD_TRANSFORM_H_

