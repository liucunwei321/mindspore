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

#include "src/runtime/kernel/arm/int8/convolution_depthwise_int8.h"
#include "schema/model_generated.h"
#include "src/kernel_registry.h"
#include "include/errorcode.h"
#include "src/runtime/kernel/arm/opclib/int8/conv_depthwise_int8.h"
#include "src/runtime/runtime_api.h"

using mindspore::kernel::KERNEL_ARCH::kCPU;
using mindspore::lite::KernelRegistrar;
using mindspore::lite::RET_ERROR;
using mindspore::lite::RET_OK;

namespace mindspore::kernel {
int ConvolutionDepthwiseInt8CPUKernel::InitWeightBias() {
  // init weight, int8 -> int16
  // o, h, w, i -> o/8, h, w, i, 8; o == group, i == 1
  auto origin_weight = reinterpret_cast<int8_t *>(inputs_[kWeightIndex]->Data());
  int OC4 = UP_DIV(conv_param_->output_channel_, C4NUM);
  int pack_weight_size = C4NUM * OC4 * conv_param_->kernel_h_ * conv_param_->kernel_w_;
  packed_weight_ = reinterpret_cast<int16_t *>(malloc(pack_weight_size * sizeof(int16_t)));
  memset(packed_weight_, 0, pack_weight_size * sizeof(int16_t));
  PackDepthwiseInt8Weight(origin_weight, packed_weight_, conv_param_);

  // init bias, add output zp
  bias_data_ = reinterpret_cast<int32_t *>(malloc(C4NUM * OC4 * sizeof(int32_t)));
  memset(bias_data_, 0, C4NUM * OC4 * sizeof(int32_t));
  if (inputs_.size() == kInputSize2) {
    auto ori_bias = reinterpret_cast<int32_t *>(inputs_.at(kBiasIndex)->Data());
    memcpy(bias_data_, ori_bias, conv_param_->output_channel_ * sizeof(int32_t));
  }
  return RET_OK;
}

int ConvolutionDepthwiseInt8CPUKernel::Init() {
  // conv base init
  ConvolutionBaseCPUKernel::Init();

  // init sliding window param
  sliding = new SlidingWindowParam;
  InitSlidingParam(sliding, conv_param_, C4NUM);

  // init quant param
  ConvolutionBaseCPUKernel::SetQuantParam();

  // init weight and bias
  auto ret = InitWeightBias();
  if (ret != RET_OK) {
    MS_LOG(ERROR) << "Depthwise int8 InitWeightBias error!";
    return ret;
  }

  ret = ReSize();
  if (ret != RET_OK) {
    MS_LOG(ERROR) << "Depthwise int8 ReSize error!";
    return ret;
  }
  return RET_OK;
}

int ConvolutionDepthwiseInt8CPUKernel::ReSize() {
  // malloc packed input buffer
  int pack_input_size = conv_param_->input_batch_ * conv_param_->input_h_ * conv_param_->input_w_ * C4NUM *
                        UP_DIV(conv_param_->input_channel_, 4);
  packed_input_ = reinterpret_cast<int16_t *>(malloc(pack_input_size * sizeof(int16_t)));
  memset(packed_input_, 0, pack_input_size * sizeof(int16_t));
  if (packed_input_ == nullptr) {
    MS_LOG(ERROR) << "Malloc buffer failed.";
    return RET_ERROR;
  }

  if (conv_param_->input_channel_ % C4NUM != 0) {
    need_align_ = true;
    int pack_output_size = conv_param_->output_batch_ * conv_param_->output_h_ * conv_param_->output_w_ * C4NUM *
                           (conv_param_->output_channel_, C4NUM);
    packed_output_ = reinterpret_cast<int8_t *>(malloc(pack_output_size * sizeof(int8_t)));
    if (packed_input_ == nullptr) {
      MS_LOG(ERROR) << "Malloc buffer failed.";
      return RET_ERROR;
    }
    memset(packed_output_, 0, pack_output_size * sizeof(int8_t));
  }
  return RET_OK;
}

int ConvolutionDepthwiseInt8CPUKernel::Execute(int task_id) {
  ConvDwInt8(packed_output_, packed_input_, packed_weight_, reinterpret_cast<int32_t *>(bias_data_), conv_param_,
             sliding, task_id);
  return RET_OK;
}

int ConvDwInt8Run(int task_id, LiteParallelGroupEnv *penv, void *cdata) {
  auto conv_dw = reinterpret_cast<ConvolutionDepthwiseInt8CPUKernel *>(cdata);
  auto ret = conv_dw->Execute(task_id);
  if (ret != RET_OK) {
    MS_LOG(ERROR) << "ConvolutionDepthwiseInt8Run error task_id[" << task_id << "] error_code[" << ret << "]";
    return RET_ERROR;
  }
  return RET_OK;
}

int ConvolutionDepthwiseInt8CPUKernel::Run() {
  if (conv_param_->input_channel_ != conv_param_->output_channel_) {
    MS_LOG(ERROR) << "Only support input channel equals output channel.";
    return RET_ERROR;
  }

  // pack input, assume input format: NHWC -> NHWC4
  auto input_tensor = inputs_.at(kInputIndex);
  auto input_addr = reinterpret_cast<int8_t *>(input_tensor->Data());
  PackDepthwiseInt8Input(input_addr, packed_input_, conv_param_);

  auto output_addr = reinterpret_cast<int8_t *>(outputs_.at(kOutputIndex)->Data());
  memset(output_addr, 0, outputs_.at(kOutputIndex)->ElementsNum() * sizeof(int8_t));
  if (!need_align_) {
    packed_output_ = output_addr;
  }

  auto ret = LiteBackendParallelLaunch(ConvDwInt8Run, this, conv_param_->thread_num_);
  if (ret != RET_OK) {
    MS_LOG(ERROR) << "ConvDwInt8Run error: error_code[" << ret << "]";
    return RET_ERROR;
  }

  if (need_align_) {
    PackNHWC4ToNHWCInt8(packed_output_, output_addr, conv_param_->output_batch_,
                        conv_param_->output_h_ * conv_param_->output_w_, conv_param_->output_channel_);
  }
  return RET_OK;
}
}  // namespace mindspore::kernel
