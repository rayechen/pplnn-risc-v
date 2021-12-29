#ifndef __ST_PPL_KERNEL_RISCV_FP16_CLIP_H_
#define __ST_PPL_KERNEL_RISCV_FP16_CLIP_H_

#include "ppl/kernel/riscv/common/general_include.h"

namespace ppl { namespace kernel { namespace riscv {

ppl::common::RetCode clip_fp16(
    const ppl::nn::TensorShape *shape,
    const __fp16 clip_max,
    const __fp16 clip_min,
    const __fp16 *src,
    __fp16 *dst
);

}}};    //  namespace ppl::kernel::riscv

#endif  //  __ST_PPL_KERNEL_RISCV_FP16_CLIP_H_