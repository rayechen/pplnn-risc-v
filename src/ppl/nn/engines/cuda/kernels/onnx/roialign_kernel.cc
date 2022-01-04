// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#include "ppl/nn/engines/cuda/kernels/onnx/roialign_kernel.h"

#include "cudakernel/nn/roialign.h"

namespace ppl { namespace nn { namespace cuda {

ppl::common::RetCode ROIAlignKernel::DoExecute(KernelExecContext* ctx) {
    auto input = ctx->GetInput<TensorImpl>(0);
    auto rois = ctx->GetInput<TensorImpl>(1);
    auto batch_indices = ctx->GetInput<TensorImpl>(2);
    auto output = ctx->GetOutput<TensorImpl>(0);

    ppl::common::RetCode status =
        PPLCUDAROIAlignForwardImp(GetStream(), input->GetShape(), input->GetBufferPtr(), rois->GetShape(),
                                  rois->GetBufferPtr(), batch_indices->GetShape(), batch_indices->GetBufferPtr(),
                                  output->GetShape(), output->GetBufferPtr(), *param_);
    return status;
}

}}} // namespace ppl::nn::cuda
