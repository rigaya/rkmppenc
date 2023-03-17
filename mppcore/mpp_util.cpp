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

#include "rgy_util.h"
#include "rgy_aspect_ratio.h"
#include "mpp_util.h"
#include "mpp_param.h"
#include "rgy_frame.h"

static const auto RGY_CODEC_TO_MPP = make_array<std::pair<RGY_CODEC, const wchar_t *>>(
    std::make_pair(RGY_CODEC_UNKNOWN, nullptr),
    std::make_pair(RGY_CODEC_H264,    AMFVideoEncoderVCE_AVC),
    std::make_pair(RGY_CODEC_HEVC,    AMFVideoEncoder_HEVC),
    std::make_pair(RGY_CODEC_AV1,     AMFVideoEncoder_AV1)
    );
MAP_PAIR_0_1(codec, rgy, RGY_CODEC, enc, const wchar_t *, RGY_CODEC_TO_MPP, RGY_CODEC_UNKNOWN, nullptr);

static const auto VCE_CODEC_UVD_NAME = make_array<std::pair<RGY_CODEC, const wchar_t *>>(
    std::make_pair(RGY_CODEC_H264,  AMFVideoDecoderUVD_H264_AVC ),
    std::make_pair(RGY_CODEC_HEVC,  AMFVideoDecoderHW_H265_HEVC ),
    std::make_pair(RGY_CODEC_VC1,   AMFVideoDecoderUVD_VC1 ),
    //std::make_pair( RGY_CODEC_WMV3,  AMFVideoDecoderUVD_WMV3 ),
    std::make_pair(RGY_CODEC_VP9,   AMFVideoDecoderHW_VP9),
    std::make_pair(RGY_CODEC_AV1,   AMFVideoDecoderHW_AV1),
    std::make_pair(RGY_CODEC_MPEG2, AMFVideoDecoderUVD_MPEG2 )
);

MAP_PAIR_0_1(codec, rgy, RGY_CODEC, dec, const wchar_t *, VCE_CODEC_UVD_NAME, RGY_CODEC_UNKNOWN, nullptr);

const wchar_t * codec_rgy_to_dec_10bit(const RGY_CODEC codec) {
    switch (codec) {
    case RGY_CODEC_AV1:  return AMFVideoDecoderHW_AV1_12BIT;
    case RGY_CODEC_HEVC: return AMFVideoDecoderHW_H265_MAIN10;
    case RGY_CODEC_VP9:  return AMFVideoDecoderHW_VP9_10BIT;
    default:
        return nullptr;
    }
}

static const auto RGY_CSP_TO_MPP = make_array<std::pair<RGY_CSP, MppFrameFormat>>(
    std::make_pair(RGY_CSP_NA,        MPP_FMT_BUTT),
    std::make_pair(RGY_CSP_NV12,      MPP_FMT_YUV420SP),
    std::make_pair(RGY_CSP_YV12,      MPP_FMT_YUV420P),
    std::make_pair(RGY_CSP_YUY2,      MPP_FMT_YUV422_YUYV),
    std::make_pair(RGY_CSP_YUV422,    MPP_FMT_YUV422P),
    std::make_pair(RGY_CSP_YUV444,    MPP_FMT_YUV444P),
    std::make_pair(RGY_CSP_YV12_09,   MPP_FMT_BUTT),
    std::make_pair(RGY_CSP_YV12_10,   MPP_FMT_BUTT),
    std::make_pair(RGY_CSP_YV12_12,   MPP_FMT_BUTT),
    std::make_pair(RGY_CSP_YV12_14,   MPP_FMT_BUTT),
    std::make_pair(RGY_CSP_YV12_16,   MPP_FMT_BUTT),
    std::make_pair(RGY_CSP_P010,      MPP_FMT_YUV420SP_10BIT),
    std::make_pair(RGY_CSP_YUV422_09, MPP_FMT_BUTT),
    std::make_pair(RGY_CSP_YUV422_10, MPP_FMT_BUTT),
    std::make_pair(RGY_CSP_YUV422_12, MPP_FMT_BUTT),
    std::make_pair(RGY_CSP_YUV422_14, MPP_FMT_BUTT),
    std::make_pair(RGY_CSP_YUV422_16, MPP_FMT_BUTT),
    std::make_pair(RGY_CSP_P210,      MPP_FMT_YUV422SP_10BIT),
    std::make_pair(RGY_CSP_YUV444_09, MPP_FMT_BUTT),
    std::make_pair(RGY_CSP_YUV444_10, MPP_FMT_BUTT),
    std::make_pair(RGY_CSP_YUV444_12, MPP_FMT_BUTT),
    std::make_pair(RGY_CSP_YUV444_14, MPP_FMT_BUTT),
    std::make_pair(RGY_CSP_YUV444_16, MPP_FMT_BUTT),
    std::make_pair(RGY_CSP_RGB24R,    MPP_FMT_BUTT),
    std::make_pair(RGY_CSP_RGB32R,    MPP_FMT_BUTT),
    std::make_pair(RGY_CSP_RGB24,     MPP_FMT_BUTT),
    std::make_pair(RGY_CSP_RGB32,     MPP_FMT_BUTT),
    std::make_pair(RGY_CSP_YC48,      MPP_FMT_BUTT)
    );

MAP_PAIR_0_1(csp, rgy, RGY_CSP, enc, MppFrameFormat, RGY_CSP_TO_MPP, RGY_CSP_NA, MPP_FMT_BUTT);

static const auto RGY_LOGLEVEL_TO_VCE = make_array<std::pair<int, int>>(
    std::make_pair(RGY_LOG_TRACE, AMF_TRACE_TRACE),
    std::make_pair(RGY_LOG_DEBUG, AMF_TRACE_DEBUG),
    std::make_pair(RGY_LOG_INFO,  AMF_TRACE_INFO),
    std::make_pair(RGY_LOG_WARN,  AMF_TRACE_WARNING),
    std::make_pair(RGY_LOG_ERROR, AMF_TRACE_ERROR)
    );
MAP_PAIR_0_1(loglevel, rgy, int, enc, int, RGY_LOGLEVEL_TO_VCE, RGY_LOG_INFO, AMF_TRACE_INFO);


static const auto RGY_PICSTRUCT_TO_MPP = make_array<std::pair<RGY_PICSTRUCT, uint32_t>>(
    std::make_pair(RGY_PICSTRUCT_UNKNOWN,      MPP_FRAME_FLAG_FRAME),
    std::make_pair(RGY_PICSTRUCT_FRAME,        MPP_FRAME_FLAG_FRAME),
    std::make_pair(RGY_PICSTRUCT_FRAME_TFF,    MPP_FRAME_FLAG_TOP_FIRST),
    std::make_pair(RGY_PICSTRUCT_FRAME_BFF,    MPP_FRAME_FLAG_BOT_FIRST),
    std::make_pair(RGY_PICSTRUCT_TFF,          MPP_FRAME_FLAG_TOP_FIELD),
    std::make_pair(RGY_PICSTRUCT_BFF,          MPP_FRAME_FLAG_BOT_FIELD),
    std::make_pair(RGY_PICSTRUCT_FIELD_TOP,    MPP_FRAME_FLAG_TOP_FIELD),
    std::make_pair(RGY_PICSTRUCT_FIELD_BOTTOM, MPP_FRAME_FLAG_BOT_FIELD)
    );
MAP_PAIR_0_1(frametype, rgy, RGY_PICSTRUCT, enc, uint32_t, RGY_PICSTRUCT_TO_MPP, RGY_PICSTRUCT_UNKNOWN, MPP_FRAME_FLAG_FRAME);

void RGYBitstream::addFrameData(RGYFrameData *frameData) {
    if (frameData != nullptr) {
        frameDataNum++;
        frameDataList = (RGYFrameData **)realloc(frameDataList, frameDataNum * sizeof(frameDataList[0]));
        frameDataList[frameDataNum - 1] = frameData;
    }
}

void RGYBitstream::clearFrameDataList() {
    frameDataNum = 0;
    if (frameDataList) {
        for (int i = 0; i < frameDataNum; i++) {
            if (frameDataList[i]) {
                delete frameDataList[i];
            }
        }
        free(frameDataList);
        frameDataList = nullptr;
    }
}
std::vector<RGYFrameData *> RGYBitstream::getFrameDataList() {
    return make_vector(frameDataList, frameDataNum);
}

RGY_NOINLINE
VideoInfo videooutputinfo(
    RGY_CODEC codec,
    amf::AMF_SURFACE_FORMAT encFormat,
    const AMFParams& prm,
    RGY_PICSTRUCT picstruct,
    const VideoVUIInfo& vui) {

    const int bframes = (codec == RGY_CODEC_H264) ? prm.get<int>(AMF_VIDEO_ENCODER_B_PIC_PATTERN) : 0;

    VideoInfo info;
    info.codec = codec;
    info.codecLevel = prm.get<int>(AMF_PARAM_PROFILE_LEVEL(codec));
    info.codecProfile = prm.get<int>(AMF_PARAM_PROFILE(codec));
    info.videoDelay = (codec == RGY_CODEC_AV1) ? 0 : ((bframes > 0) + (bframes > 2));
    info.dstWidth = prm.get<int>(VCE_PARAM_KEY_OUTPUT_WIDTH);
    info.dstHeight = prm.get<int>(VCE_PARAM_KEY_OUTPUT_HEIGHT);
    info.fpsN = prm.get<AMFRate>(AMF_PARAM_FRAMERATE(codec)).num;
    info.fpsD = prm.get<AMFRate>(AMF_PARAM_FRAMERATE(codec)).den;
    info.sar[0] = prm.get<AMFRatio>(AMF_PARAM_ASPECT_RATIO(codec)).num;
    info.sar[1] = prm.get<AMFRatio>(AMF_PARAM_ASPECT_RATIO(codec)).den;
    adjust_sar(&info.sar[0], &info.sar[1], info.dstWidth, info.dstHeight);
    info.picstruct = picstruct;
    info.csp = csp_enc_to_rgy(encFormat);
    info.vui = vui;
    return info;
}

#if !ENABLE_AVSW_READER
#define TTMATH_NOASM
#pragma warning(push)
#pragma warning(disable: 4244)
#include "ttmath/ttmath.h"
#pragma warning(pop)

int64_t rational_rescale(int64_t v, rgy_rational<int> from, rgy_rational<int> to) {
    auto mul = rgy_rational<int64_t>((int64_t)from.n() * (int64_t)to.d(), (int64_t)from.d() * (int64_t)to.n());

#if _M_IX86
#define RESCALE_INT_SIZE 4
#else
#define RESCALE_INT_SIZE 2
#endif
    ttmath::Int<RESCALE_INT_SIZE> tmp1 = v;
    tmp1 *= mul.n();
    ttmath::Int<RESCALE_INT_SIZE> tmp2 = mul.d();

    tmp1 = (tmp1 + tmp2 - 1) / tmp2;
    int64_t ret;
    tmp1.ToInt(ret);
    return ret;
}

#endif