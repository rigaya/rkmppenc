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

#pragma once
#include <optional>
#include "rgy_util.h"
#include "rgy_caption.h"
#include "rgy_prm.h"

#include "mpp_rc_api.h"
#include "rk_type.h"
#include "rk_venc_cmd.h"
#include "rk_venc_cfg.h"

#include "iep2_api.h"

static const int MPP_DEFAULT_QP_I = 23;
static const int MPP_DEFAULT_QP_P = 26;
static const int MPP_DEFAULT_QP_B = 29;
static const int MPP_DEFAULT_MAX_BITRATE = 25000;
static const int MPP_DEFAULT_GOP_LEN = 300;

const CX_DESC list_codec[] = {
    { _T("h264"), RGY_CODEC_H264 },
    { _T("hevc"), RGY_CODEC_HEVC },
    //{ _T("av1"),  MPP_VIDEO_CodingAV1  },
    { NULL, 0 }
};
const CX_DESC list_codec_all[] = {
    { _T("h264"), RGY_CODEC_H264 },
    { _T("avc"),  RGY_CODEC_H264 },
    { _T("h265"), RGY_CODEC_HEVC },
    { _T("hevc"), RGY_CODEC_HEVC },
    //{ _T("av1"),  MPP_VIDEO_CodingAV1  },
    { NULL, 0 }
};

const CX_DESC list_avc_profile[] = {
    { _T("baseline"), 66   },
    { _T("main"),     77   },
    { _T("high"),     100  },
    { NULL, 0 }
};

static const int mpp_avc_profile_default_idx = 2;

const CX_DESC list_avc_level[] = {
    { _T("auto"), 0   },
    { _T("1"),    10  },
    { _T("1b"),   9   },
    { _T("1.1"),  11  },
    { _T("1.2"),  12  },
    { _T("1.3"),  13  },
    { _T("2"),    20  },
    { _T("2.1"),  21  },
    { _T("2.2"),  22  },
    { _T("3"),    30  },
    { _T("3.1"),  31  },
    { _T("3.2"),  32  },
    { _T("4"),    40  },
    { _T("4.1"),  41  },
    { _T("4.2"),  42  },
    { _T("5"),    50  },
    { _T("5.1"),  51  },
    { _T("5.2"),  52  },
    { NULL, 0 }
};

const CX_DESC list_hevc_profile[] = {
    { _T("main"),     1 },
    //{ _T("main10"),   2 },
    { NULL, 0 }
};

const CX_DESC list_hevc_tier[] = {
    { _T("main"),     0 },
    { _T("high"),     1 },
    { NULL, 0 }
};

const CX_DESC list_hevc_level[] = {
    { _T("auto"),   0 },
    { _T("1"),     30 },
    { _T("2"),     60 },
    { _T("2.1"),   63 },
    { _T("3"),     90 },
    { _T("3.1"),   93 },
    { _T("4"),    120 },
    { _T("4.1"),  123 },
    { _T("5"),    150 },
    { _T("5.1"),  153 },
    { _T("5.2"),  156 },
    { _T("6"),    180 },
    { _T("6.1"),  183 },
    { _T("6.2"),  186 },
    { NULL, 0 }
};

#if 0

const CX_DESC list_hevc_bitdepth[] = {
    { _T("8bit"),   8   },
    { _T("10bit"), 10  },
    { NULL, 0 }
};
const CX_DESC list_av1_bitdepth[] = {
    { _T("8bit"),   8   },
    { _T("10bit"), 10  },
    { NULL, 0 }
};

const CX_DESC list_av1_profile[] = {
    { _T("main"),     AMF_VIDEO_ENCODER_AV1_PROFILE_MAIN },
    { NULL, 0 }
};

const CX_DESC list_av1_tier[] = {
    { _T("main"),     AMF_VIDEO_ENCODER_HEVC_TIER_MAIN },
    { _T("high"),     AMF_VIDEO_ENCODER_HEVC_TIER_HIGH },
    { NULL, 0 }
};

const CX_DESC list_av1_level[] = {
    { _T("auto"), -1 },
    { _T("2"),   AMF_VIDEO_ENCODER_AV1_LEVEL_2_0 },
    { _T("2.1"), AMF_VIDEO_ENCODER_AV1_LEVEL_2_1 },
    { _T("2.2"), AMF_VIDEO_ENCODER_AV1_LEVEL_2_2 },
    { _T("2.3"), AMF_VIDEO_ENCODER_AV1_LEVEL_2_3 },
    { _T("3"),   AMF_VIDEO_ENCODER_AV1_LEVEL_3_0 },
    { _T("3.1"), AMF_VIDEO_ENCODER_AV1_LEVEL_3_1 },
    { _T("3.2"), AMF_VIDEO_ENCODER_AV1_LEVEL_3_2 },
    { _T("3.3"), AMF_VIDEO_ENCODER_AV1_LEVEL_3_3 },
    { _T("4"),   AMF_VIDEO_ENCODER_AV1_LEVEL_4_0 },
    { _T("4.1"), AMF_VIDEO_ENCODER_AV1_LEVEL_4_1 },
    { _T("4.2"), AMF_VIDEO_ENCODER_AV1_LEVEL_4_2 },
    { _T("4.3"), AMF_VIDEO_ENCODER_AV1_LEVEL_4_3 },
    { _T("5"),   AMF_VIDEO_ENCODER_AV1_LEVEL_5_0 },
    { _T("5.1"), AMF_VIDEO_ENCODER_AV1_LEVEL_5_1 },
    { _T("5.2"), AMF_VIDEO_ENCODER_AV1_LEVEL_5_2 },
    { _T("5.3"), AMF_VIDEO_ENCODER_AV1_LEVEL_5_3 },
    { _T("6"),   AMF_VIDEO_ENCODER_AV1_LEVEL_6_0 },
    { _T("6.1"), AMF_VIDEO_ENCODER_AV1_LEVEL_6_1 },
    { _T("6.2"), AMF_VIDEO_ENCODER_AV1_LEVEL_6_2 },
    { _T("6.3"), AMF_VIDEO_ENCODER_AV1_LEVEL_6_3 },
    { _T("7"),   AMF_VIDEO_ENCODER_AV1_LEVEL_7_0 },
    { _T("7.1"), AMF_VIDEO_ENCODER_AV1_LEVEL_7_1 },
    { _T("7.2"), AMF_VIDEO_ENCODER_AV1_LEVEL_7_2 },
    { _T("7.3"), AMF_VIDEO_ENCODER_AV1_LEVEL_7_3 },
    { NULL, 0 }
};

#endif

const CX_DESC list_mpp_rc_method[] = {
    { _T("CQP"),  MPP_ENC_RC_MODE_FIXQP  },
    { _T("CBR"),  MPP_ENC_RC_MODE_CBR    },
    { _T("VBR"),  MPP_ENC_RC_MODE_VBR    },
    { _T("AVBR"), MPP_ENC_RC_MODE_AVBR   },
    { NULL, 0 }
};

const CX_DESC list_mpp_quality_preset[] = {
    { _T("worst"),    MPP_ENC_RC_QUALITY_WORST },
    { _T("worse"),    MPP_ENC_RC_QUALITY_WORSE },
    { _T("medium"),   MPP_ENC_RC_QUALITY_MEDIUM },
    { _T("better"),   MPP_ENC_RC_QUALITY_BETTER },
    { _T("best"),     MPP_ENC_RC_QUALITY_BEST },
    { _T("cqp"),      MPP_ENC_RC_QUALITY_CQP },
    { NULL, 0 }
};

static inline const CX_DESC *get_level_list(RGY_CODEC codec) {
    switch (codec) {
    case RGY_CODEC_H264:    return list_avc_level;
    case RGY_CODEC_HEVC:    return list_hevc_level;
    case RGY_CODEC_AV1:     return list_empty;
    default:                return list_empty;
    }
}
static int get_level_auto(RGY_CODEC codec) {
    return get_cx_value(get_level_list(codec), _T("auto"));
}

static inline const CX_DESC *get_profile_list(RGY_CODEC codec) {
    switch (codec) {
    case RGY_CODEC_H264:    return list_avc_profile;
    case RGY_CODEC_HEVC:    return list_hevc_profile;
    case RGY_CODEC_AV1:     return list_empty;
    default:                return list_empty;
    }
}

static inline const CX_DESC *get_tier_list(RGY_CODEC codec) {
    switch (codec) {
    case RGY_CODEC_HEVC:    return list_hevc_tier;
    default:                return list_empty;
    }
}

enum class IEPDeinterlaceMode {
    DISABLED,
    NORMAL_I5,
    NORMAL_I2,
    BOB_I5,
    BOB_I2,
};

const CX_DESC list_iep_deinterlace[] = {
    { _T("disabled"),    (int)IEPDeinterlaceMode::DISABLED },
    { _T("normal_i5"),   (int)IEPDeinterlaceMode::NORMAL_I5 },
    { _T("normal_i2"),   (int)IEPDeinterlaceMode::NORMAL_I2 },
    { _T("bob_i5"),      (int)IEPDeinterlaceMode::BOB_I5 },
    { _T("bob_i2"),      (int)IEPDeinterlaceMode::BOB_I2 },
    { NULL, 0 }
};

struct VCECodecParam {
    int profile;
    int tier;
    int level;
};

struct MPPParamDec {
    std::optional<bool> sort_pts;
    std::optional<bool> split_parse;
    bool fast_parse;
    std::optional<bool> enable_hdr_meta;
    bool disable_error;
    bool enable_vproc;

    MPPParamDec();
};

struct MPPParam {
    VideoInfo input;              //入力する動画の情報
    RGYParamInput inprm;
    RGYParamCommon common;
    RGYParamControl ctrl;
    RGYParamVpp vpp;

    MPPParamDec hwdec;
    IEPDeinterlaceMode deint;

    RGY_CODEC codec;
    VCECodecParam codecParam[RGY_CODEC_NUM];

    int     outputDepth;

    int     rateControl;
    int     qualityPreset;
    int     bitrate;
    int     maxBitrate;
    int     VBVBufferSize;
    RGYQPSet qp;
    int     qpMax;
    int     qpMin;
    int     gopLen;
    int     chromaQPOffset;
    bool    repeatHeaders;

    int     par[2];

    // H.264
    bool    disableDeblock;
    int     deblockAlpha;
    int     deblockBeta;

    MPPParam();
    ~MPPParam();
};

#pragma warning(pop)
