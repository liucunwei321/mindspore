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

#ifndef MINDSPORE_LITE_SRC_LITE_KERNEL_H_
#define MINDSPORE_LITE_SRC_LITE_KERNEL_H_
#include <vector>
#include <string>
#ifdef ENABLE_FP16
#include <arm_neon.h>
#endif
#include "src/runtime/kernel/arm/opclib/op_base.h"
// #include "backend/kernel_compiler/kernel.h"
#include "include/context.h"
#include "src/ir/tensor.h"
#include "src/ops/ops.h"

#ifdef ENABLE_FP16
using FLOAT_t = float16_t;
#else
using FLOAT_t = float;
#endif

// using mindspore::kernel::AddressPtr;
namespace mindspore::kernel {
enum KERNEL_ARCH { kCPU, kGPU, kNPU, kInferShape };
struct KernelKey {
  KERNEL_ARCH arch;
  schema::PrimitiveType type;

  bool operator<(const KernelKey &dst) const {
    if (arch != dst.arch) {
      return arch < dst.arch;
    } else {
      return type < dst.type;
    }
  }
};

class LiteKernel;
struct CallBackParam {
  std::string name_callback_aram;
};

using KernelCallBack = std::function<bool(std::vector<lite::tensor::Tensor *> inputs,
                                          std::vector<lite::tensor::Tensor *> outputs, const CallBackParam &opInfo)>;

// class LiteKernel : public KernelMod {
class LiteKernel {
 public:
  LiteKernel() = default;
  explicit LiteKernel(OpParameter *parameter, const std::vector<lite::tensor::Tensor *> &inputs,
                      const std::vector<lite::tensor::Tensor *> &outputs)
      : opParameter(parameter), inputs_(inputs), outputs_(outputs) {
    this->in_kernel_.clear();
    this->out_kernel_.clear();
  }

  virtual ~LiteKernel() { delete opParameter; }

  //  bool Launch(const std::vector<AddressPtr> &inputs, const std::vector<AddressPtr> &workspace,
  //              const std::vector<AddressPtr> &outputs, void *stream_ptr) override {
  //    return false;
  //  };
  //
  //  const std::vector<size_t> &GetInputSizeList() const override { return {}; }
  //
  //  const std::vector<size_t> &GetOutputSizeList() const override { return {}; }
  //
  //  const std::vector<size_t> &GetWorkspaceSizeList() const override { return {}; }

  virtual int Prepare() { return -1; }
  virtual int Init() { return -1; }
  virtual int ReSize() { return -1; }
  virtual int Run() { return -1; }

  std::string Name() { return this->name; }

  void set_name(const std::string &name) { this->name = name; }

  schema::PrimitiveType type() { return (schema::PrimitiveType)this->opParameter->type_; }

  std::string type_str() { return schema::EnumNamePrimitiveType((schema::PrimitiveType)this->opParameter->type_); }

  void SetInputs(const std::vector<lite::tensor::Tensor *> &inputs) { this->inputs_ = inputs; }

  void SetOutputs(const std::vector<lite::tensor::Tensor *> &outputs) { this->outputs_ = outputs; }

  std::vector<lite::tensor::Tensor *> &GetInputs() { return this->inputs_; }

  std::vector<lite::tensor::Tensor *> &GetOutputs() { return this->outputs_; }

  void AddInKernel(LiteKernel *kernel) { this->in_kernel_.emplace_back(kernel); }

  void AddOutKernel(LiteKernel *kernel) { this->out_kernel_.emplace_back(kernel); }

  std::vector<LiteKernel *> &GetInKernels() { return this->in_kernel_; }

  std::vector<LiteKernel *> &GetOutKernels() { return this->out_kernel_; }

  void InitOutTensorRefCount();

  int DecOutTensorRefCount(lite::Allocator *allocator = nullptr);

  const KernelKey Desc() const { return desc; }

  void set_desc(const KernelKey kernel_key) { desc = kernel_key; }

 protected:
  KernelKey desc;
  std::string name;
  OpParameter *opParameter = nullptr;
  // tensor will free in ~lite_session()
  std::vector<lite::tensor::Tensor *> inputs_;
  std::vector<lite::tensor::Tensor *> outputs_;
  std::vector<LiteKernel *> in_kernel_;
  std::vector<LiteKernel *> out_kernel_;
};

class SubGraphKernel : public LiteKernel {
 public:
  explicit SubGraphKernel(const std::vector<lite::tensor::Tensor *> &inputs,
                          const std::vector<lite::tensor::Tensor *> &outputs,
                          const std::vector<kernel::LiteKernel *> &inKernels,
                          const std::vector<kernel::LiteKernel *> &outKernels,
                          const std::vector<kernel::LiteKernel *> &nodes)
      : LiteKernel(nullptr, inputs, outputs),
        inputs_(inputs),
        outputs_(outputs),
        inkernels_(inKernels),
        outkernels_(outKernels),
        nodes_(nodes) {}

  virtual int Init() { return -1; }
  virtual int InferShape() { return -1; }
  virtual int ReSize() { return -1; }
  virtual int Run() { return -1; }

 protected:
  std::vector<lite::tensor::Tensor *> inputs_;
  std::vector<lite::tensor::Tensor *> outputs_;
  std::vector<LiteKernel *> inkernels_;
  std::vector<LiteKernel *> outkernels_;
  std::vector<LiteKernel *> nodes_;
};

typedef LiteKernel *(*KernelCreator)(const std::vector<lite::tensor::Tensor *> &inputs,
                                     const std::vector<lite::tensor::Tensor *> &outputs, OpParameter *parameter,
                                     const lite::Context *ctx, const KernelKey &desc);

class LiteKernelUtil {
 public:
  static void TopologicalSortKernels(std::vector<kernel::LiteKernel *> &kernels);

  static std::vector<kernel::LiteKernel *> SubgraphInputKernels(const std::vector<kernel::LiteKernel *> &kernels);

  static std::vector<kernel::LiteKernel *> SubgraphOutputKernels(const std::vector<kernel::LiteKernel *> &kernels);

  static std::vector<lite::tensor::Tensor *> SubgraphInputTensors(const std::vector<kernel::LiteKernel *> &kernels);

  static std::vector<lite::tensor::Tensor *> SubgraphOutputTensors(const std::vector<kernel::LiteKernel *> &kernels);

  static void InitTensorRefCount(std::vector<kernel::LiteKernel *> &kernels);

  static int SetInput(LiteKernel &kernelMod, std::vector<lite::tensor::Tensor *> inputs);
};
}  // namespace mindspore::kernel

#endif  // MINDSPORE_LITE_SRC_LITE_KERNEL_H_

