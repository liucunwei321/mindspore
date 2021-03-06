/**
 * Copyright 2019 Huawei Technologies Co., Ltd
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

#include <set>
#include <string>
#include "src/kernel_registry.h"
#include "src/runtime/opencl/opencl_runtime.h"
#include "src/runtime/kernel/arm/opclib/fp32/matmul.h"
#include "src/runtime/kernel/opencl/kernel/matmul.h"
#ifndef PROGRAM_WITH_IL
#include "src/runtime/kernel/opencl/cl/fp16/matmul.cl.inc"
#include "src/runtime/kernel/opencl/cl/fp32/matmul.cl.inc"
#endif

using mindspore::kernel::KERNEL_ARCH::kGPU;
using mindspore::lite::KernelRegistrar;
using mindspore::schema::PrimitiveType_FullConnection;
using mindspore::schema::PrimitiveType_MatMul;

namespace mindspore::kernel {

int MatMulOpenCLKernel::Init() {
  std::string kernel_name = "MatMul";
  auto ocl_runtime = lite::opencl::OpenCLRuntime::GetInstance();

#ifdef PROGRAM_WITH_IL
  ocl_runtime->CreateKernelFromIL(kernel_(), kernel_name);
#else
  std::set<std::string> build_options;
// build_options.emplace("-DPOOL_AVG");
#ifdef ENABLE_FP16
  std::string source = matmul_source_fp16;
#else
  std::string source = matmul_source_fp32;
#endif
  std::string program_name = "MatMul";
  ocl_runtime->LoadSource(program_name, source);
  ocl_runtime->BuildKernel(kernel_, program_name, kernel_name, build_options);
#endif
  int ci = inputs_[1]->shape()[1];
  int co = inputs_[1]->shape()[0];
  sizeCI = {ci, UP_DIV(ci, 4)};
  sizeCO = {co, UP_DIV(co, 4)};
  auto allocator = ocl_runtime->GetAllocator();
  padWeight_ = reinterpret_cast<FLOAT_T *>(allocator->Malloc(sizeCI.s[1] * sizeCO.s[1] * 16 * sizeof(FLOAT_T)));
  padWeight_ = reinterpret_cast<FLOAT_T *>(allocator->MapBuffer(padWeight_, CL_MAP_WRITE, nullptr, true));
  if (hasBias_) {
    bias_ = reinterpret_cast<FLOAT_T *>(allocator->Malloc(sizeCO.s[1] * 4 * sizeof(FLOAT_T)));
    bias_ = reinterpret_cast<FLOAT_T *>(allocator->MapBuffer(bias_, CL_MAP_WRITE, nullptr, true));
  }
  PadWeight();
  allocator->UnmapBuffer(padWeight_);
  if (hasBias_) {
    allocator->UnmapBuffer(bias_);
  }
  outputs_[0]->SetFormat(schema::Format_NHWC4);
  MS_LOG(DEBUG) << kernel_name << " Init Done!";
  return 0;
}

int MatMulOpenCLKernel::ReSize() { return 0; }

void MatMulOpenCLKernel::PadWeight() {
  auto origin_weight = reinterpret_cast<FLOAT_T *>(inputs_.at(kWeightIndex)->Data());
  int divCI = sizeCI.s[1];
  int divCO = sizeCO.s[1];
  int index = 0;
  for (int i = 0; i < divCI; ++i) {
    for (int j = 0; j < divCO; ++j) {
      for (int k = 0; k < 4; ++k) {
        for (int l = 0; l < 4; ++l) {
          int src_x = i * 4 + l;
          int src_y = j * 4 + k;
          if (src_x < sizeCI.s[0] && src_y < sizeCO.s[0]) {
            padWeight_[index++] = origin_weight[src_y * sizeCI.s[0] + src_x];
          } else {
            padWeight_[index++] = 0;
          }
        }
      }
    }
  }
  if (hasBias_) {
    memcpy(inputs_[2]->Data(), bias_, sizeof(FLOAT_T) * sizeCI.s[0]);
    for (int i = sizeCI.s[0]; i < sizeCI.s[1] * 4; i++) {
      bias_[i] = 0;
    }
  }
}

int MatMulOpenCLKernel::Run() {
  MS_LOG(DEBUG) << this->Name() << " Running!";
  std::vector<int> shapex = inputs_[0]->shape();
  int n = shapex[0];
  if (n > 1) {
    MS_LOG(ERROR) << "MatMul n > 1 not supported!";
    return 1;
  }
  auto ocl_runtime = lite::opencl::OpenCLRuntime::GetInstance();
  // local size should less than MAX_GROUP_SIZE
  std::vector<size_t> local = {64, 4};
  std::vector<size_t> global = {UP_ROUND(sizeCO.s[1], local[0]), 4};

  ocl_runtime->SetKernelArg(kernel_, 0, inputs_[0]->Data());
  ocl_runtime->SetKernelArg(kernel_, 1, padWeight_);
  ocl_runtime->SetKernelArg(kernel_, 2, outputs_[0]->Data());
  if (hasBias_) {
    ocl_runtime->SetKernelArg(kernel_, 3, inputs_[2]->Data());
  } else {
    ocl_runtime->SetKernelArg(kernel_, 3, nullptr);
  }
  ocl_runtime->SetKernelArg(kernel_, 4, sizeCI);
  ocl_runtime->SetKernelArg(kernel_, 5, sizeCO);
  ocl_runtime->SetKernelArg(kernel_, 6, hasBias_ ? 1 : 0);
  ocl_runtime->RunKernel(kernel_, global, local, nullptr);
  return 0;
}

kernel::LiteKernel *OpenCLMatMulKernelCreator(const std::vector<lite::tensor::Tensor *> &inputs,
                                              const std::vector<lite::tensor::Tensor *> &outputs,
                                              OpParameter *opParameter, const lite::Context *ctx,
                                              const kernel::KernelKey &desc) {
  bool hasBias = false;
  if (opParameter->type_ == PrimitiveType_FullConnection) {
    hasBias = (reinterpret_cast<MatMulParameter *>(opParameter))->has_bias_;
  }
  auto *kernel = new MatMulOpenCLKernel(reinterpret_cast<OpParameter *>(opParameter), inputs, outputs, hasBias);
  auto ret = kernel->Init();
  if (0 != ret) {
    // MS_LOG(ERROR) << "Init kernel failed, name: " << opDef.name()->str()
    //               << ", type: " << lite::EnumNameOpT(opDef.attr_type());
    delete kernel;
    return nullptr;
  }
  return kernel;
}

REG_KERNEL(kGPU, PrimitiveType_MatMul, OpenCLMatMulKernelCreator)
REG_KERNEL(kGPU, PrimitiveType_FullConnection, OpenCLMatMulKernelCreator)
}  // namespace mindspore::kernel

