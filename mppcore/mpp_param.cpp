// -----------------------------------------------------------------------------------------
//     VCEEnc by rigaya
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

#include <algorithm>
#include "rgy_util.h"
#include "mpp_param.h"

static const int MPP_DEFAULT_QPI = 23;
static const int MPP_DEFAULT_QPP = 26;
static const int MPP_DEFAULT_QPB = 29;

MPPParam::MPPParam() :
    input(),
    inprm(),
    common(),
    ctrl(),
    vpp(),
    codec(RGY_CODEC_H264),
    codecParam(),
    outputDepth(8),
    rateControl(MPP_ENC_RC_MODE_FIXQP),
    qualityPreset(MPP_ENC_RC_QUALITY_MEDIUM),
    bitrate(5000),
    maxBitrate(0),
    VBVBufferSize(0),
    qpI(MPP_DEFAULT_QPI),
    qpP(MPP_DEFAULT_QPP),
    qpB(MPP_DEFAULT_QPB),
    gopLen(0) {
    codecParam[RGY_CODEC_H264].level   = 51;
    codecParam[RGY_CODEC_H264].profile = 100;
    codecParam[RGY_CODEC_HEVC].level   = 0;
    codecParam[RGY_CODEC_HEVC].profile = 0;
    codecParam[RGY_CODEC_HEVC].tier    = 0;
    codecParam[RGY_CODEC_AV1].level    = -1; // as auto
    codecParam[RGY_CODEC_AV1].profile  = 0;
    codecParam[RGY_CODEC_AV1].tier     = 0;
    input.vui = VideoVUIInfo();
}

MPPParam::~MPPParam() {

}
