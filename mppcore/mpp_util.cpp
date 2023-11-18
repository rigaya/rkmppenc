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

#include "rgy_util.h"
#include "rgy_aspect_ratio.h"
#include "mpp_util.h"
#include "mpp_param.h"
#include "mpp_frame.h"
#include "mpp_log_def.h"
#include "rga/rga.h"
#include "rgy_frame.h"

static const auto RGY_CODEC_TO_MPP = make_array<std::pair<RGY_CODEC, MppCodingType>>(
    std::make_pair(RGY_CODEC_UNKNOWN, MPP_VIDEO_CodingUnused),
    std::make_pair(RGY_CODEC_H264,    MPP_VIDEO_CodingAVC),
    std::make_pair(RGY_CODEC_HEVC,    MPP_VIDEO_CodingHEVC),
    std::make_pair(RGY_CODEC_AV1,     MPP_VIDEO_CodingAV1)
    );
MAP_PAIR_0_1(codec, rgy, RGY_CODEC, enc, MppCodingType, RGY_CODEC_TO_MPP, RGY_CODEC_UNKNOWN, MPP_VIDEO_CodingUnused);

static const auto MPP_CODEC_DEC_NAME = make_array<std::pair<RGY_CODEC, MppCodingType>>(
    std::make_pair(RGY_CODEC_H264,  MPP_VIDEO_CodingAVC ),
    std::make_pair(RGY_CODEC_HEVC,  MPP_VIDEO_CodingHEVC ),
    //std::make_pair(RGY_CODEC_VC1,   MPP_VIDEO_CodingVC1 ),
    //std::make_pair( RGY_CODEC_WMV3,  MPP_VIDEO_CodingWMV ),
    std::make_pair(RGY_CODEC_VP9,   MPP_VIDEO_CodingVP9),
    std::make_pair(RGY_CODEC_AV1,   MPP_VIDEO_CodingAV1),
    std::make_pair(RGY_CODEC_MPEG2, MPP_VIDEO_CodingMPEG2 )
);

MAP_PAIR_0_1(codec, rgy, RGY_CODEC, dec, MppCodingType, MPP_CODEC_DEC_NAME, RGY_CODEC_UNKNOWN, MPP_VIDEO_CodingUnused);

static const auto RGY_CSP_TO_MPP = make_array<std::pair<RGY_CSP, MppFrameFormat>>(
    std::make_pair(RGY_CSP_NA,        MPP_FMT_BUTT),
    std::make_pair(RGY_CSP_NV12,      MPP_FMT_YUV420SP),
    std::make_pair(RGY_CSP_YV12,      MPP_FMT_YUV420P),
    std::make_pair(RGY_CSP_YUY2,      MPP_FMT_YUV422_YUYV),
    std::make_pair(RGY_CSP_NV16,      MPP_FMT_YUV422SP),
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
    std::make_pair(RGY_CSP_BGR24,     MPP_FMT_BGR888),
    std::make_pair(RGY_CSP_BGR32,     MPP_FMT_ABGR8888),
    std::make_pair(RGY_CSP_RGB24,     MPP_FMT_RGB888),
    std::make_pair(RGY_CSP_RGB32,     MPP_FMT_RGBA8888),
    std::make_pair(RGY_CSP_YC48,      MPP_FMT_BUTT)
    );

__attribute__((noinline)) MppFrameFormat csp_rgy_to_enc(RGY_CSP var0) {
    auto ret = std::find_if(RGY_CSP_TO_MPP.begin(), RGY_CSP_TO_MPP.end(), [var0](std::pair<RGY_CSP, MppFrameFormat> a)
                            { return a.first == var0; });
    return (ret == RGY_CSP_TO_MPP.end()) ? (MPP_FMT_BUTT) : ret->second;
}
__attribute__((noinline)) RGY_CSP csp_enc_to_rgy(MppFrameFormat var1) {
    auto ret = std::find_if(RGY_CSP_TO_MPP.begin(), RGY_CSP_TO_MPP.end(), [var1](std::pair<RGY_CSP, MppFrameFormat> a)
                            { return a.second == (var1 & MPP_FRAME_FMT_MASK); });
    return (ret == RGY_CSP_TO_MPP.end()) ? (RGY_CSP_NA) : ret->first;
}

static const auto RGY_CSP_TO_RKRGA = make_array<std::pair<RGY_CSP, RgaSURF_FORMAT>>(
    std::make_pair(RGY_CSP_NA,        RK_FORMAT_UNKNOWN),
    std::make_pair(RGY_CSP_NV12,      RK_FORMAT_YCbCr_420_SP),
    std::make_pair(RGY_CSP_YV12,      RK_FORMAT_YCbCr_420_P),
    std::make_pair(RGY_CSP_YUY2,      RK_FORMAT_YUYV_422),
    std::make_pair(RGY_CSP_NV16,      RK_FORMAT_YCbCr_422_SP),
    std::make_pair(RGY_CSP_YUV422,    RK_FORMAT_YCbCr_422_P),
    std::make_pair(RGY_CSP_YUV444,    RK_FORMAT_UNKNOWN),
    std::make_pair(RGY_CSP_YV12_09,   RK_FORMAT_UNKNOWN),
    std::make_pair(RGY_CSP_YV12_10,   RK_FORMAT_UNKNOWN),
    std::make_pair(RGY_CSP_YV12_12,   RK_FORMAT_UNKNOWN),
    std::make_pair(RGY_CSP_YV12_14,   RK_FORMAT_UNKNOWN),
    std::make_pair(RGY_CSP_YV12_16,   RK_FORMAT_UNKNOWN),
    std::make_pair(RGY_CSP_P010,      RK_FORMAT_YCbCr_420_SP_10B),
    std::make_pair(RGY_CSP_YUV422_09, RK_FORMAT_UNKNOWN),
    std::make_pair(RGY_CSP_YUV422_10, RK_FORMAT_UNKNOWN),
    std::make_pair(RGY_CSP_YUV422_12, RK_FORMAT_UNKNOWN),
    std::make_pair(RGY_CSP_YUV422_14, RK_FORMAT_UNKNOWN),
    std::make_pair(RGY_CSP_YUV422_16, RK_FORMAT_UNKNOWN),
    std::make_pair(RGY_CSP_P210,      RK_FORMAT_YCbCr_422_10b_SP),
    std::make_pair(RGY_CSP_YUV444_09, RK_FORMAT_UNKNOWN),
    std::make_pair(RGY_CSP_YUV444_10, RK_FORMAT_UNKNOWN),
    std::make_pair(RGY_CSP_YUV444_12, RK_FORMAT_UNKNOWN),
    std::make_pair(RGY_CSP_YUV444_14, RK_FORMAT_UNKNOWN),
    std::make_pair(RGY_CSP_YUV444_16, RK_FORMAT_UNKNOWN),
    std::make_pair(RGY_CSP_BGR24,     RK_FORMAT_BGR_888),
    std::make_pair(RGY_CSP_BGR32,     RK_FORMAT_ABGR_8888),
    std::make_pair(RGY_CSP_RGB24,     RK_FORMAT_RGB_888),
    std::make_pair(RGY_CSP_RGB32,     RK_FORMAT_RGBA_8888),
    std::make_pair(RGY_CSP_YC48,      RK_FORMAT_UNKNOWN)
    );

MAP_PAIR_0_1(csp, rgy, RGY_CSP, rkrga, RgaSURF_FORMAT, RGY_CSP_TO_RKRGA, RGY_CSP_NA, RK_FORMAT_UNKNOWN);

static const auto RGY_PICSTRUCT_TO_MPP = make_array<std::pair<RGY_PICSTRUCT, uint32_t>>(
    std::make_pair(RGY_PICSTRUCT_UNKNOWN,      MPP_FRAME_FLAG_FRAME),
    std::make_pair(RGY_PICSTRUCT_FRAME,        MPP_FRAME_FLAG_FRAME),
    std::make_pair(RGY_PICSTRUCT_FRAME_TFF,    MPP_FRAME_FLAG_PAIRED_FIELD | MPP_FRAME_FLAG_TOP_FIRST),
    std::make_pair(RGY_PICSTRUCT_FRAME_BFF,    MPP_FRAME_FLAG_PAIRED_FIELD | MPP_FRAME_FLAG_BOT_FIRST),
    std::make_pair(RGY_PICSTRUCT_TFF,          MPP_FRAME_FLAG_TOP_FIRST),
    std::make_pair(RGY_PICSTRUCT_BFF,          MPP_FRAME_FLAG_BOT_FIRST),
    std::make_pair(RGY_PICSTRUCT_FIELD_TOP,    MPP_FRAME_FLAG_TOP_FIELD),
    std::make_pair(RGY_PICSTRUCT_FIELD_BOTTOM, MPP_FRAME_FLAG_BOT_FIELD)
    );
MAP_PAIR_0_1(picstruct, rgy, RGY_PICSTRUCT, enc, uint32_t, RGY_PICSTRUCT_TO_MPP, RGY_PICSTRUCT_UNKNOWN, MPP_FRAME_FLAG_FRAME);

static const auto RGY_LOGLEVEL_TO_MPP = make_array<std::pair<int, int>>(
    std::make_pair(RGY_LOG_TRACE, MPP_LOG_VERBOSE),
    std::make_pair(RGY_LOG_DEBUG, MPP_LOG_DEBUG),
    std::make_pair(RGY_LOG_INFO,  MPP_LOG_INFO),
    std::make_pair(RGY_LOG_WARN,  MPP_LOG_WARN),
    std::make_pair(RGY_LOG_ERROR, MPP_LOG_ERROR)
    );
MAP_PAIR_0_1(loglevel, rgy, int, enc, int, RGY_LOGLEVEL_TO_MPP, RGY_LOG_INFO, MPP_LOG_INFO);

static const auto RGY_INTERP_TO_RGA = make_array<std::pair<RGY_VPP_RESIZE_ALGO, IM_SCALE_MODE>>(
    std::make_pair(RGY_VPP_RESIZE_RGA_NEAREST,  INTER_NEAREST),
    std::make_pair(RGY_VPP_RESIZE_RGA_BILINEAR, INTER_LINEAR),
    std::make_pair(RGY_VPP_RESIZE_RGA_BICUBIC,  INTER_CUBIC)
    );
MAP_PAIR_0_1(interp, rgy, RGY_VPP_RESIZE_ALGO, rga, IM_SCALE_MODE, RGY_INTERP_TO_RGA, RGY_VPP_RESIZE_UNKNOWN, (IM_SCALE_MODE)-1);

uint32_t setMppH264ForceConstraintFlags(const std::array<std::pair<bool, bool>, 6>& constraint_flags) {
    uint32_t force = 0x00;
    uint32_t flags = 0x00;
    for (uint32_t i = 0; i < constraint_flags.size(); i++) {
        uint32_t setbit = 1 << i;
        if (constraint_flags[i].first) {
            force |= setbit;
            if (constraint_flags[i].second) {
                flags |= setbit;
            }
        }
    }
    return (force << 16) | flags;
}

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

RGYFrameMpp::RGYFrameMpp() : mppframe(createMPPFrameEmpty()), duration_(0), flags_(RGY_FRAME_FLAG_NONE), frameDataList() {
};

RGYFrameMpp::RGYFrameMpp(const RGYFrameInfo &frame, MppBufferGroup frmGroup, const int x_stride, const int y_stride) :
    mppframe(createMPPFrameEmpty()),
    duration_(0),
    flags_(RGY_FRAME_FLAG_NONE),
    frameDataList() {
    allocate(frame, frmGroup, x_stride, y_stride);
};

RGYFrameMpp::RGYFrameMpp(MppFrame mppframe, uint64_t duration__, RGY_FRAME_FLAGS flags__, std::vector<std::shared_ptr<RGYFrameData>> dataList) :
    mppframe(unique_mppframe(mppframe, RGYMPPDeleter<MppFrame>(mpp_frame_deinit))),
    duration_(duration__),
    flags_(flags__),
    frameDataList(dataList) {

}

RGYFrameMpp::~RGYFrameMpp() {
    mppframe.reset();
    frameDataList.clear();
}

RGY_ERR RGYFrameMpp::allocate(const RGYFrameInfo &frame, MppBufferGroup frmGroup, const int x_stride, const int y_stride) {
    if (!frmGroup) {
        return RGY_ERR_NULL_PTR;
    }
    mppframe = createMPPFrame();
    const int frameSize = mpp_frame_size(frame, y_stride);
    MppBuffer mppbuffer = nullptr;
    auto ret = err_to_rgy(mpp_buffer_get(frmGroup, &mppbuffer, frameSize));
    if (ret != RGY_ERR_NONE) {
        return ret;
    }

    mpp_frame_set_fmt(mppframe.get(), csp_rgy_to_enc(frame.csp));
    mpp_frame_set_width(mppframe.get(), frame.width);
    mpp_frame_set_height(mppframe.get(), frame.height);
    mpp_frame_set_hor_stride(mppframe.get(), (x_stride) ? x_stride : mpp_frame_pitch(frame.csp, frame.width));
    mpp_frame_set_ver_stride(mppframe.get(), (y_stride) ? y_stride : frame.height);
    mpp_frame_set_offset_x(mppframe.get(), 0);
    mpp_frame_set_offset_y(mppframe.get(), 0);
    mpp_frame_set_mode(mppframe.get(), picstruct_rgy_to_enc(frame.picstruct));
    mpp_frame_set_pts(mppframe.get(), frame.timestamp);
    mpp_frame_set_buf_size(mppframe.get(), frameSize);
    mpp_frame_set_buffer(mppframe.get(), mppbuffer);
    mpp_frame_set_poc(mppframe.get(), frame.inputFrameId);
    // mpp_frame_set_bufferして、frameに持たせることで、mppbufferの参照カウントはさらに増えている
    // mpp_buffer_get と合わせて +2
    // なので、mppbuffer は put して参照カウントを +1 に戻す
    mpp_buffer_put(mppbuffer);
    return RGY_ERR_NONE;
}

std::unique_ptr<RGYFrameMpp> RGYFrameMpp::createCopy() const {
    MppFrame newFrame = nullptr;
    if (mpp_frame_init(&newFrame) == MPP_OK) {
        mpp_frame_set_fmt(newFrame, mpp_frame_get_fmt(mppframe.get()));
        mpp_frame_set_width(newFrame, mpp_frame_get_width(mppframe.get()));
        mpp_frame_set_height(newFrame, mpp_frame_get_height(mppframe.get()));
        mpp_frame_set_hor_stride(newFrame, mpp_frame_get_hor_stride(mppframe.get()));
        mpp_frame_set_ver_stride(newFrame, mpp_frame_get_ver_stride(mppframe.get()));
        mpp_frame_set_offset_x(newFrame, mpp_frame_get_offset_x(mppframe.get()));
        mpp_frame_set_offset_y(newFrame, mpp_frame_get_offset_y(mppframe.get()));
        mpp_frame_set_mode(newFrame, mpp_frame_get_mode(mppframe.get()));
        mpp_frame_set_pts(newFrame, mpp_frame_get_pts(mppframe.get()));
        mpp_frame_set_buf_size(newFrame, mpp_frame_get_buf_size(mppframe.get()));
        if (mpp_frame_get_buffer(mppframe.get())) {
            mpp_frame_set_buffer(newFrame, mpp_frame_get_buffer(mppframe.get()));
        }
        // TODO: metaの正しいコピーの仕方がわからん
        //if (mpp_frame_has_meta(mppframe.get())) {
        //    mpp_frame_set_meta(newFrame, mpp_frame_get_meta(mppframe.get()));
        //}
        mpp_frame_set_poc(newFrame, mpp_frame_get_poc(mppframe.get()));
    }
    return std::make_unique<RGYFrameMpp>(newFrame, duration_, flags_, frameDataList);
}

RGY_NOINLINE
VideoInfo videooutputinfo(
    const MPPCfg &prm,
    rgy_rational<int>& sar,
    RGY_PICSTRUCT picstruct,
    const VideoVUIInfo& vui) {
    VideoInfo info;
    info.codec = prm.rgy_codec();
    info.codecProfile = prm.codec_profile();
    info.codecLevel = prm.codec_level();
    info.dstWidth = prm.prep.width;
    info.dstHeight = prm.prep.height;
    info.fpsN = prm.rc.fps_out_num;
    info.fpsD = prm.rc.fps_out_denorm;
    info.sar[0] = sar.n();
    info.sar[1] = sar.d();
    adjust_sar(&info.sar[0], &info.sar[1], info.dstWidth, info.dstHeight);
    info.picstruct = picstruct;
    info.csp = csp_enc_to_rgy(prm.prep.format);
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