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

#ifndef __ST_PPL_KERNEL_RISCV_FP16_CONV2D_WG_VEC128_COMMON_WG_OFFLINE_H_
#define __ST_PPL_KERNEL_RISCV_FP16_CONV2D_WG_VEC128_COMMON_WG_OFFLINE_H_

#include "ppl/kernel/riscv/common/math.h"
#include <cstdio>
namespace ppl { namespace kernel { namespace riscv {

typedef void (*PPL3_conv_wg_riscv_fp16_cvt_filter_blk_kernel_func_t) (
    const __fp16 *filter,
    const __fp16 *trans_mat, // TODO: should be removed
    int filter_out_stride,

    __fp16 *filter_cvt,
    int filter_cvt_wg_tile_stride
);


template<int wgb, int wgf>
size_t ppl3_conv_wg_bxfxs1_get_cvt_filter_size_fp16(
    int channels,
    int num_outs,
    int group) {
    
    const int wg_tile_len = wgb + wgf - 1;
    const int wg_tile_size = wg_tile_len * wg_tile_len;

    int channels_per_group = channels / group;
    int num_outs_per_group = num_outs / group;
    int pad_channels_per_group = round_up(channels_per_group, 8);
    int pad_num_outs_per_group = round_up(num_outs_per_group, 8);

    int num_filter_data = wg_tile_size * group * pad_channels_per_group * pad_num_outs_per_group;
    return num_filter_data * sizeof(__fp16);
}

template<int wgb, int wgf>
size_t ppl3_conv_wg_bxfxs1_get_temp_buffer_size_fp16(
    int src_h,
    int src_w,
    int channels,
    int num_outs,
    int group,
    int padding_h,
    int padding_w,

    int blk_dst_h,
    int blk_dst_w,
    int blk_channels,
    int blk_num_outs) {
    
    // pad blk param
    blk_dst_h = round_up(blk_dst_h, wgb);
    blk_dst_w = round_up(blk_dst_w, wgb);

    const int wg_tile_len = wgb + wgf - 1;
    const int wg_tile_size = wg_tile_len * wg_tile_len;

    int channels_per_group = channels / group;
    int num_outs_per_group = num_outs / group;
    int pad_channels_per_group = round_up(channels_per_group, 8);
    int pad_num_outs_per_group = round_up(num_outs_per_group, 8);

    const int flt_stride = 1;
    int dst_h = (src_h + 2 * padding_h) - wgf + flt_stride;
    int dst_w = (src_w + 2 * padding_w) - wgf + flt_stride;

    // fix blk pad
    blk_channels = round_up(min(blk_channels, channels_per_group), 8);
    blk_num_outs = round_up(min(blk_num_outs, num_outs_per_group), 8);

    int dst_pad_h = round_up(dst_h, wgb);
    int dst_pad_w = round_up(dst_w, wgb);
    int src_pad_h = dst_pad_h + wgf - 1;
    int src_pad_w = dst_pad_w + wgf - 1;

    int blk_dst_pad_h = min(dst_pad_h, blk_dst_h);
    int blk_dst_pad_w = min(dst_pad_w, blk_dst_w);
    int blk_src_pad_h = blk_dst_pad_h + wgf - 1;
    int blk_src_pad_w = blk_dst_pad_w + wgf - 1;

    int num_tile_per_blk = (blk_dst_pad_h / wgb) * (blk_dst_pad_w / wgb);

    size_t src_pad_blk_size = pad_channels_per_group * blk_src_pad_h * blk_src_pad_w * sizeof(__fp16);
    size_t src_trans_size = wg_tile_size * pad_channels_per_group * num_tile_per_blk * sizeof(__fp16);
    size_t dst_trans_size = wg_tile_size * blk_num_outs * num_tile_per_blk * sizeof(__fp16);

    size_t src_pad_size_for_group = 0;
    size_t dst_pad_size_for_group = 0;
    if (channels_per_group % 8 != 0 && group != 1) {
        src_pad_size_for_group = group * pad_channels_per_group * src_h * src_w * sizeof(__fp16);
    }
    if (num_outs_per_group % 8 != 0 && group != 1) {
        dst_pad_size_for_group = group * pad_num_outs_per_group * dst_h * dst_w * sizeof(__fp16); 
    }

    return src_pad_size_for_group + dst_pad_size_for_group + src_pad_blk_size + src_trans_size + dst_trans_size;
}


template <
    int wgb,
    int wgf,
    PPL3_conv_wg_riscv_fp16_cvt_filter_blk_kernel_func_t cvt_filter_blk_kernel>
void ppl3_conv_wg_bxfxs1_cvt_filter_blk_fp16(
    const __fp16 *filter,
    const __fp16 *trans_mat,
    int channels,
    int num_outs,
    int filter_out_stride,
    int filter_kernel_size,
    __fp16 *filter_cvt) {
    
    int tile_len = wgb + wgf - 1;
    int tile_size = tile_len * tile_len;

    int num_outs_div8 = num_outs / 8;
    int channels_div8 = channels / 8;
    int num_outs_left = num_outs - num_outs_div8 * 8;
    int channels_left = channels - channels_div8 * 8;
    int pad_channels = (channels + 8 - 1) / 8 * 8;
    int pad_num_outs = (num_outs + 8 - 1) / 8 * 8;

    // printf("channels:%d num_outs:%d filter_out_stride:%d filter_kernel_size:%d\n", channels, num_outs, filter_out_stride, filter_kernel_size);
    // int flt_cvt_tile_stride = num_outs * channels;
    int flt_cvt_tile_stride = pad_num_outs * pad_channels;
    for (int oc = 0; oc < num_outs_div8; oc++) {
        for (int ic = 0; ic < channels; ic++) {
            for (int oc8 = 0; oc8 < 8; oc8++) {
                int flt_src_offset = (oc * 8 + oc8) * filter_out_stride + ic * filter_kernel_size;
                int flt_cvt_offset = oc8 + ic * 8 + oc * pad_channels * 8;
                cvt_filter_blk_kernel(
                    filter + flt_src_offset,
                    trans_mat,
                    filter_out_stride,
                    filter_cvt + flt_cvt_offset,
                    flt_cvt_tile_stride
                );
            }
        }
        // pad flt in IC
        for (int ic = channels; ic < pad_channels; ic++) {
            for (int oc8 = 0; oc8 < 8; oc8++) {
                int flt_cvt_offset = oc8 + ic * 8 + oc * pad_channels * 8;
                for (int i = 0; i < tile_len; i++) {
                    for (int j = 0; j < tile_len; j++) {
                        int flt_cvt_idx = (i * tile_len + j) * flt_cvt_tile_stride;
                        filter_cvt[flt_cvt_offset + flt_cvt_idx] = 0.0f;
                    }
                }
            }
        }
    }
    // pad flt in OC
    if (num_outs_left > 0) {
        for (int ic = 0; ic < channels; ic++) {
            for (int oc8 = 0; oc8 < num_outs_left; oc8++) {
                int flt_src_offset = (num_outs_div8 * 8 + oc8) * filter_out_stride + ic * filter_kernel_size;
                int flt_cvt_offset = oc8 + ic * 8 + num_outs_div8 * pad_channels * 8;
                cvt_filter_blk_kernel(
                    filter + flt_src_offset,
                    trans_mat,
                    filter_out_stride,
                    filter_cvt + flt_cvt_offset,
                    flt_cvt_tile_stride
                );
            }
            for (int oc8 = num_outs_left; oc8 < 8; oc8++) {
                int flt_cvt_offset = oc8 + ic * 8 + num_outs_div8 * pad_channels * 8;
                for (int i = 0; i < tile_len; i++) {
                    for (int j = 0; j < tile_len; j++) {
                        int flt_cvt_idx = (i * tile_len + j) * flt_cvt_tile_stride;
                        filter_cvt[flt_cvt_offset + flt_cvt_idx] = 0.0f;
                    }
                }            
            }       
        }
        for (int ic = channels; ic < pad_channels; ic++) {
            for (int oc8 = 0; oc8 < 8; oc8++) {
                int flt_cvt_offset = oc8 + ic * 8 + num_outs_div8 * pad_channels * 8;
                for (int i = 0; i < tile_len; i++) {
                    for (int j = 0; j < tile_len; j++) {
                        int flt_cvt_idx = (i * tile_len + j) * flt_cvt_tile_stride;
                        filter_cvt[flt_cvt_offset + flt_cvt_idx] = 0.0f;
                    }
                }
            }
        }
    }

}


template <
    int wgb,
    int wgf,
    PPL3_conv_wg_riscv_fp16_cvt_filter_blk_kernel_func_t cvt_filter_blk_kernel>
void ppl3_conv_wg_bxfxs1_cvt_filter_fp16(
    const __fp16 *filter,
    const __fp16 *trans_mat,
    int channels,
    int num_outs,
    int group,

    int blk_channels,
    int blk_num_outs,
    __fp16 *filter_cvt) {

    // printf("blk_channels:%d blk_num_outs:%d\n", blk_channels, blk_num_outs);
    const int wg_tile_len = wgb + wgf - 1;
    const int wg_tile_size = wg_tile_len * wg_tile_len;

    int channels_per_group = channels / group;
    int num_outs_per_group = num_outs / group;
    int pad_channels_per_group = round_up(channels_per_group, 8);
    int pad_num_outs_per_group = round_up(num_outs_per_group, 8);

    // fix blk pad
    blk_channels = round_up(min(blk_channels, channels_per_group), 8);
    blk_num_outs = round_up(min(blk_num_outs, num_outs_per_group), 8);

    int filter_kernel_size = wgf * wgf;
    int filter_out_stride = channels_per_group * filter_kernel_size;

    auto filter_per_group = filter;
    auto filter_cvt_per_group = filter_cvt;
    int64_t filter_group_stride = filter_kernel_size * num_outs_per_group * channels_per_group;
    int64_t filter_cvt_group_stride = wg_tile_size * pad_num_outs_per_group * pad_channels_per_group;


    for (int g = 0; g < group; g += 1) {
        auto filter_cvt_per_blk = filter_cvt_per_group;
        for (int i = 0; i < num_outs_per_group; i += blk_num_outs) {
            int real_blk_num_outs = min(num_outs_per_group - i, blk_num_outs);
            for (int j = 0; j < channels_per_group; j += blk_channels) {
                int real_blk_channels = min(channels_per_group - j, blk_channels);
                ppl3_conv_wg_bxfxs1_cvt_filter_blk_fp16<wgb, wgf, cvt_filter_blk_kernel>(
                    filter_per_group + i * filter_out_stride + j * filter_kernel_size,
                    trans_mat,
                    real_blk_channels,
                    real_blk_num_outs,
                    filter_out_stride,
                    filter_kernel_size,
                    filter_cvt_per_blk
                );

                filter_cvt_per_blk += round_up(real_blk_num_outs, 8) * round_up(real_blk_channels, 8) * wg_tile_size;
            }
        }
        filter_per_group += filter_group_stride;
        filter_cvt_per_group += filter_cvt_group_stride;
    }
}

}}};    //  namespace ppl::kernel::riscv

#endif  //  __ST_PPL_KERNEL_RISCV_FP16_CONV2D_WG_VEC128_COMMON_WG_OFFLINE_H_
