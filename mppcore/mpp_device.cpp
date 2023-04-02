// -----------------------------------------------------------------------------------------
//     rkmppenc by rigaya
// -----------------------------------------------------------------------------------------
// The MIT License
//
// Copyright (c) 2014-2017 rigaya
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// IABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// ------------------------------------------------------------------------------------------

#include "rgy_avutil.h"
#include "rk_mpi.h"
#include "mpp_util.h"
#include "mpp_device.h"
#include "rga/im2d.hpp"

DeviceCodecCsp getMPPDecoderSupport() {
    CodecCsp codecCsp;
    for (size_t ic = 0; ic < _countof(HW_DECODE_LIST); ic++) {
        const auto codec = HW_DECODE_LIST[ic].rgy_codec;
        if (err_to_rgy(mpp_check_support_format(MPP_CTX_DEC, codec_rgy_to_dec(codec))) == RGY_ERR_NONE) {
            codecCsp[codec] = { RGY_CSP_YV12 };
        }
    }
    DeviceCodecCsp HWDecCodecCsp;
    HWDecCodecCsp.push_back(std::make_pair(0, codecCsp));
    return HWDecCodecCsp;
}

std::vector<RGY_CODEC> getMPPEncoderSupport() {
    std::vector<RGY_CODEC> encCodec;
    for (auto codec : { RGY_CODEC_H264, RGY_CODEC_HEVC }) {
        if (err_to_rgy(mpp_check_support_format(MPP_CTX_ENC, codec_rgy_to_enc(codec))) == RGY_ERR_NONE) {
            encCodec.push_back(codec);
        }
    }
    return encCodec;
}

tstring getRGAInfo() {
    std::string str;
    str += std::string(querystring(RGA_VENDOR));
    str += std::string(querystring(RGA_VERSION));
    str += std::string(querystring(RGA_MAX_INPUT));
    str += std::string(querystring(RGA_MAX_OUTPUT));
    str += std::string(querystring(RGA_BYTE_STRIDE));
    str += std::string(querystring(RGA_SCALE_LIMIT));
    str += std::string(querystring(RGA_INPUT_FORMAT));
    str += std::string(querystring(RGA_OUTPUT_FORMAT));
    //str += std::string(querystring(RGA_FEATURE));
    str += std::string(querystring(RGA_EXPECTED));
    return char_to_tstring(str);
}