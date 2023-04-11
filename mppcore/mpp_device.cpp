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

#include "mpp_soc.h"
#include "mpp_platform.h"

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

static bool check_dev_tree(const char *devtree_target) {
    bool okay = false;
    std::string cmd = R"(find /proc/device-tree/ -type d -name ")";
    cmd += devtree_target;
    cmd += R"(@*")";
    cmd += R"( | xargs -I {} sh -c "cat {}/status | grep -q okay > /dev/null")";
    auto ret = system(cmd.c_str());
    return ret == 0;
}

tstring getRGAInfo() {
    const bool hasRGA = (access("/dev/rga", F_OK) == 0);
    std::string str;
    str += strsprintf("RGA (/dev/rga) avail  : %s\n", hasRGA ? _T("yes") : _T("no"));
    str += strsprintf("RGA status            : %s\n", check_dev_tree("rga") ? _T("okay") : _T("no"));
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

const char *mpp_ioctl_ver_name(const MppIoctlVersion ver) {
    switch (ver) {
    case IOCTL_VCODEC_SERVICE: return "vcodec_service";
    case IOCTL_MPP_SERVICE_V1: return "mpp_service_v1";
    default:
        break;
    }
    return "unknown";
}

const char *mpp_kernel_ver_name(const MppKernelVersion ver) {
    switch (ver) {
    case KERNEL_3_10: return "3.10";
    case KERNEL_4_4:  return "4.4";
    case KERNEL_4_19: return "4.19";
    case KERNEL_5_10: return "5.10";
    case KERNEL_UNKNOWN:
    default:
        break;
    }
    return "unknown";
}

tstring getMppInfo() {
    const bool hasMppService = (access("/dev/mpp_service", F_OK | R_OK | W_OK) == 0)
                            || (access("/dev/mpp-service", F_OK | R_OK | W_OK) == 0);
    const bool hasIEPv1      = (access("/dev/iep", F_OK) == 0);
    const bool hasRGA        = (access("/dev/rga", F_OK) == 0);
    const bool statusMppSrv  = check_dev_tree("mpp-srv");
    const bool statusRGA     = check_dev_tree("rga");
    const bool statusIEP     = check_dev_tree("iep");
    const auto soc_name      = mpp_get_soc_name();
    const auto soc_info      = mpp_get_soc_info();
    const auto ioctl_ver     = mpp_get_ioctl_version();
    const auto kernel_ver    = mpp_get_kernel_version();
    std::string str;
    str += strsprintf("SoC name        : %s\n", soc_name);
    str += strsprintf("Mpp service     : %s [%s] (%s)\n",
        hasMppService ? "yes" : "no",
        hasMppService ? mpp_ioctl_ver_name(ioctl_ver) : "-",
        statusMppSrv  ? "okay" : "no");
    str += strsprintf("Mpp kernel      : %s\n", mpp_kernel_ver_name(kernel_ver));
    str += "2D accerelation :";
    if (hasIEPv1)      str += strsprintf(" iepv1(%s)", statusIEP ? "okay" : "no");
    if (hasMppService) str += strsprintf(" iepv2(%s)", statusIEP ? "okay" : "no");
    if (hasRGA)        str += strsprintf(" rga(%s)",   statusRGA ? "okay" : "no");
    str += "\n";
    str += "HW Encode       :";
    for (auto codec : { RGY_CODEC_H264, RGY_CODEC_HEVC }) {
        if (mpp_check_soc_cap(MPP_CTX_ENC, codec_rgy_to_enc(codec) )) {
            str += " " + CodecToStr(codec);
        }
    }
    str += "\n";
    str += "HW Decode       :";
    for (auto codec : { RGY_CODEC_H264, RGY_CODEC_HEVC, RGY_CODEC_MPEG2, RGY_CODEC_AV1 }) {
        if (mpp_check_soc_cap(MPP_CTX_DEC, codec_rgy_to_dec(codec))) {
            str += " " + CodecToStr(codec);
        }
    }
    str += "\n";
    return char_to_tstring(str);
}
