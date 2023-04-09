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
#ifndef __MPP_UTIL_H__
#define __MPP_UTIL_H__

#include "rgy_util.h"
#include "rgy_err.h"
#include "rgy_opencl.h"
#include "rgy_frame.h"
#include "mpp_frame.h"
#include "rga/rga.h"

#include "mpp_param.h"
#include "convert_csp.h"

class RGYFrameData;

MAP_PAIR_0_1_PROTO(codec, rgy, RGY_CODEC, enc, MppCodingType);
MAP_PAIR_0_1_PROTO(codec, rgy, RGY_CODEC, dec, MppCodingType);
MAP_PAIR_0_1_PROTO(csp, rgy, RGY_CSP, enc, MppFrameFormat);
MAP_PAIR_0_1_PROTO(csp, rgy, RGY_CSP, rkrga, RgaSURF_FORMAT);
MAP_PAIR_0_1_PROTO(picstruct, rgy, RGY_PICSTRUCT, enc, uint32_t);
MAP_PAIR_0_1_PROTO(loglevel, rgy, int, enc, int);
MAP_PAIR_0_1_PROTO(interp, rgy, RGY_VPP_RESIZE_ALGO, rga, IM_SCALE_MODE);

struct MPPCfg {
    MppEncCfg       cfg;
    MppEncPrepCfg   prep;
    MppEncRcCfg     rc;
    MppEncCodecCfg  codec;
    MppEncSliceSplit split;

    MPPCfg();
    RGYFrameInfo frameinfo() const {
        const int outWidth = prep.width;
        const int outHeight = prep.height;
        int bitdepth = 8;
        auto csp = RGY_CSP_NV12;
        const bool yuv444 = false;
        if (bitdepth > 8) {
            csp = (yuv444) ? RGY_CSP_YUV444_16 : RGY_CSP_P010;
        } else {
            csp = (yuv444) ? RGY_CSP_YUV444 : RGY_CSP_NV12;
        }
        return RGYFrameInfo(outWidth, outHeight, csp, bitdepth, RGY_PICSTRUCT_FRAME, RGY_MEM_TYPE_CPU);
    }
    RGY_CODEC rgy_codec() const {
        return codec_enc_to_rgy(codec.coding);
    }
    int codec_profile() const {
        switch (rgy_codec()) {
        case RGY_CODEC_H264: return codec.h264.profile;
        case RGY_CODEC_HEVC: return codec.h265.profile;
        default: break;
        }
        return 0;
    }
    int codec_level() const {
        switch (rgy_codec()) {
        case RGY_CODEC_H264: return codec.h264.level;
        case RGY_CODEC_HEVC: return codec.h265.level;
        default: break;
        }
        return 0;
    }
    int codec_tier() const {
        switch (rgy_codec()) {
        case RGY_CODEC_HEVC: return codec.h265.tier;
        default: break;
        }
        return 0;
    }
};

struct RGYBitstream {
private:
    uint8_t *dataptr;
    size_t  dataLength;
    size_t  dataOffset;
    size_t  maxLength;
    int64_t  dataDts;
    int64_t  dataPts;
    uint32_t dataFlag;
    uint32_t dataAvgQP;
    RGY_FRAMETYPE dataFrametype;
    RGY_PICSTRUCT dataPicstruct;
    int dataFrameIdx;
    int64_t dataDuration;
    RGYFrameData **frameDataList;
    int frameDataNum;

public:
    uint8_t *bufptr() const {
        return dataptr;
    }

    uint8_t *data() const {
        return dataptr + dataOffset;
    }

    uint8_t *release() {
        uint8_t *ptr = dataptr;
        dataptr = nullptr;
        dataOffset = 0;
        dataLength = 0;
        maxLength = 0;
        return ptr;
    }

    uint32_t dataflag() const {
        return dataFlag;
    }

    void setDataflag(uint32_t flag) {
        dataFlag = flag;
    }

    RGY_FRAMETYPE frametype() const {
        return dataFrametype;
    }

    void setFrametype(RGY_FRAMETYPE type) {
        dataFrametype = type;
    }

    RGY_PICSTRUCT picstruct() const {
        return dataPicstruct;
    }

    void setPicstruct(RGY_PICSTRUCT picstruct) {
        dataPicstruct = picstruct;
    }

    int64_t duration() const {
        return dataDuration;
    }

    void setDuration(int64_t duration) {
        dataDuration = duration;
    }

    int frameIdx() const {
        return dataFrameIdx;
    }

    void setFrameIdx(int frameIdx) {
        dataFrameIdx = frameIdx;
    }

    size_t size() const {
        return dataLength;
    }

    void setSize(size_t size) {
        dataLength = size;
    }

    size_t offset() const {
        return dataOffset;
    }

    void addOffset(size_t add) {
        dataOffset += add;
    }

    void setOffset(size_t offset) {
        dataOffset = offset;
    }

    size_t bufsize() const {
        return maxLength;
    }

    int64_t pts() const {
        return dataPts;
    }

    void setPts(int64_t pts) {
        dataPts = pts;
    }

    int64_t dts() const {
        return dataDts;
    }

    void setDts(int64_t dts) {
        dataDts = dts;
    }

    uint32_t avgQP() {
        return dataAvgQP;
    }

    void setAvgQP(uint32_t avgQP) {
        dataAvgQP = avgQP;
    }

    void clear() {
        if (dataptr && maxLength) {
            _aligned_free(dataptr);
        }
        dataptr = nullptr;
        dataLength = 0;
        dataOffset = 0;
        maxLength = 0;
    }

    RGY_ERR init(size_t nSize) {
        clear();

        if (nSize > 0) {
            if (nullptr == (dataptr = (uint8_t *)_aligned_malloc(nSize, 32))) {
                return RGY_ERR_NULL_PTR;
            }

            maxLength = nSize;
        }
        return RGY_ERR_NONE;
    }

    void trim() {
        if (dataOffset > 0 && dataLength > 0) {
            memmove(dataptr, dataptr + dataOffset, dataLength);
            dataOffset = 0;
        }
    }

    RGY_ERR copy(const uint8_t *setData, size_t setSize) {
        if (setData == nullptr || setSize == 0) {
            return RGY_ERR_MORE_BITSTREAM;
        }
        if (maxLength < setSize) {
            clear();
            auto sts = init(setSize);
            if (sts != RGY_ERR_NONE) {
                return sts;
            }
        }
        dataLength = setSize;
        dataOffset = 0;
        memcpy(dataptr, setData, setSize);
        return RGY_ERR_NONE;
    }

    RGY_ERR copy(const uint8_t *setData, size_t setSize, int64_t pts, int64_t dts, int64_t duration) {
        auto sts = copy(setData, setSize);
        if (sts != RGY_ERR_NONE) {
            return sts;
        }
        dataDts = dts;
        dataPts = pts;
        dataDuration = duration;
        return RGY_ERR_NONE;
    }

    RGY_ERR ref(uint8_t *refData, size_t refSize) {
        clear();
        dataptr = refData;
        dataLength = refSize;
        dataOffset = 0;
        maxLength = 0;
        return RGY_ERR_NONE;
    }

    RGY_ERR copy(const uint8_t *setData, size_t setSize, int64_t dts, int64_t pts) {
        auto sts = copy(setData, setSize);
        if (sts != RGY_ERR_NONE) {
            return sts;
        }
        dataDts = dts;
        dataPts = pts;
        return RGY_ERR_NONE;
    }

    RGY_ERR ref(uint8_t *refData, size_t refSize, int64_t pts, int64_t dts, int64_t duration) {
        auto sts = ref(refData, refSize);
        if (sts != RGY_ERR_NONE) {
            return sts;
        }
        dataDts = dts;
        dataPts = pts;
        dataDuration = duration;
        return RGY_ERR_NONE;
    }

    RGY_ERR copy(const RGYBitstream *pBitstream) {
        auto sts = copy(pBitstream->data(), pBitstream->size());
        if (sts != RGY_ERR_NONE) {
            return sts;
        }
        return RGY_ERR_NONE;
    }

    RGY_ERR changeSize(size_t nNewSize) {
        uint8_t *pData = (uint8_t *)_aligned_malloc(nNewSize, 32);
        if (pData == nullptr) {
            return RGY_ERR_NULL_PTR;
        }

        auto nDataLen = dataLength;
        if (dataLength) {
            memcpy(pData, dataptr + dataOffset, (std::min)(nDataLen, nNewSize));
        }
        clear();

        dataptr       = pData;
        dataOffset = 0;
        dataLength = nDataLen;
        maxLength  = nNewSize;

        return RGY_ERR_NONE;
    }

    RGY_ERR append(const uint8_t *appendData, size_t appendSize) {
        if (appendData && appendSize > 0) {
            const auto new_data_length = appendSize + dataLength;
            if (maxLength < new_data_length) {
                auto sts = changeSize(new_data_length);
                if (sts != RGY_ERR_NONE) {
                    return sts;
                }
            }

            if (maxLength < new_data_length + dataOffset) {
                memmove(dataptr, dataptr + dataOffset, dataLength);
                dataOffset = 0;
            }
            memcpy(dataptr + dataLength + dataOffset, appendData, appendSize);
            dataLength = new_data_length;
        }
        return RGY_ERR_NONE;
    }

    RGY_ERR append(RGYBitstream *pBitstream) {
        return append(pBitstream->data(), pBitstream->size());
    }
    void addFrameData(RGYFrameData *frameData);
    void clearFrameDataList();
    std::vector<RGYFrameData *> getFrameDataList();
};

static inline RGYBitstream RGYBitstreamInit() {
    RGYBitstream bitstream;
    memset(&bitstream, 0, sizeof(bitstream));
    return bitstream;
}

#ifndef __CUDACC__
static_assert(std::is_pod<RGYBitstream>::value == true, "RGYBitstream should be POD type.");
#endif

int mpp_frame_pitch(RGY_CSP csp, const int width);
int mpp_frame_size(const RGYFrameInfo &frame);

template<typename T>
struct RGYMPPDeleter {
    RGYMPPDeleter() : deleter(nullptr) {};
    RGYMPPDeleter(std::function<MPP_RET(T*)> deleter) : deleter(deleter) {};
    void operator()(T p) { /* fprintf(stderr, "RGYMPPDeleter: %p\n", p);*/ deleter(&p); }
    std::function<MPP_RET(T*)> deleter;
};

using unique_mppframe = std::unique_ptr<std::remove_pointer<MppFrame>::type, RGYMPPDeleter<MppFrame>>;

static unique_mppframe createMPPFrameEmpty() {
    return unique_mppframe(nullptr, RGYMPPDeleter<MppFrame>(mpp_frame_deinit));
}

static unique_mppframe createMPPFrame() {
    MppFrame frame = nullptr;
    if (mpp_frame_init(&frame) != MPP_OK) {
        frame = nullptr;
    }
    return unique_mppframe(frame, RGYMPPDeleter<MppFrame>(mpp_frame_deinit));
}

static RGYFrameInfo setMPPBufferInfo(const RGY_CSP csp, 
    const int width, const int height, const int x_stride, const int y_stride, MppBuffer buffer) {
    uint8_t *base = (uint8_t *)mpp_buffer_get_ptr(buffer);
    RGYFrameInfo info;
    info.csp = csp;
    info.width = width;
    info.height = height;
    info.ptr[0] = base;
    info.pitch[0] = x_stride;
    switch (csp) {
    case RGY_CSP_NV12:
    case RGY_CSP_NV16:
    case RGY_CSP_NV24:
    case RGY_CSP_P010:
    case RGY_CSP_P210:
        info.ptr[1] = base + x_stride * y_stride;
        info.pitch[1] = x_stride;
        break;
    case RGY_CSP_YV12:
        info.ptr[1] = base + (x_stride >> 1) * y_stride;
        info.ptr[2] = base + (x_stride >> 1) * (info.height >> 1);
        info.pitch[1] = x_stride >> 1;
        info.pitch[2] = x_stride >> 1;
        break;
    case RGY_CSP_YUV444:
    case RGY_CSP_YUV444_09:
    case RGY_CSP_YUV444_10:
    case RGY_CSP_YUV444_12:
    case RGY_CSP_YUV444_14:
    case RGY_CSP_YUV444_16:
    case RGY_CSP_RGB:
    case RGY_CSP_GBR:
        info.ptr[1] = base + x_stride * y_stride;
        info.ptr[2] = base + x_stride * y_stride * 2;
        info.pitch[1] = x_stride;
        info.pitch[2] = x_stride;
    default:
        break;
    }
    return info;
}

static RGYFrameInfo infoMPP(MppFrame mppframe) {
    const int x_stride = mpp_frame_get_hor_stride(mppframe);
    const int y_stride = mpp_frame_get_ver_stride(mppframe);
    const MppFrameFormat fmt = mpp_frame_get_fmt(mppframe);

    RGYFrameInfo info = setMPPBufferInfo(csp_enc_to_rgy(fmt), mpp_frame_get_width(mppframe), mpp_frame_get_height(mppframe),
        x_stride, y_stride, mpp_frame_get_buffer(mppframe));

    //auto meta = mpp_frame_get_meta(mppframe);
    //RK_S64 duration;
    //mpp_meta_get_s64(meta, KEY_USER_DATA, &duration);
    
    info.timestamp = mpp_frame_get_pts(mppframe);
    info.duration = 0;
    info.picstruct = picstruct_enc_to_rgy(mpp_frame_get_mode(mppframe) & MPP_FRAME_FLAG_FIELD_ORDER_MASK);
    info.flags = RGY_FRAME_FLAG_NONE;
    info.mem_type = RGY_MEM_TYPE_MPP;
    info.inputFrameId = mpp_frame_get_poc(mppframe);
    info.flags = RGY_FRAME_FLAG_NONE;
    return info;
}

struct RGYFrameMpp : public RGYFrame {
public:
    RGYFrameMpp();
    RGYFrameMpp(const RGYFrameInfo &frame, MppBufferGroup frmGroup);
    RGYFrameMpp(MppFrame mppframe, uint64_t duration__, RGY_FRAME_FLAGS flags__ = RGY_FRAME_FLAG_NONE, std::vector<std::shared_ptr<RGYFrameData>> dataList = {});
    virtual ~RGYFrameMpp();
    virtual RGY_ERR allocate(const RGYFrameInfo &frame, MppBufferGroup frmGroup);

    MppFrame mpp() { return mppframe.get(); }
    MppFrame releaseMpp() { return mppframe.release();}
    RGYFrameInfo getInfoCopy() const { return getInfo(); }

    virtual bool isempty() const { return !mppframe; }
    virtual void setTimestamp(uint64_t timestamp) override { mpp_frame_set_pts(mppframe.get(), timestamp); }
    virtual void setDuration(uint64_t duration) override { duration_ = duration; }
    virtual void setPicstruct(RGY_PICSTRUCT picstruct) override { mpp_frame_set_mode(mppframe.get(), picstruct_rgy_to_enc(picstruct)); }
    virtual void setInputFrameId(int id) override { mpp_frame_set_poc(mppframe.get(), id); }
    virtual void setFlags(RGY_FRAME_FLAGS frameflags) override { flags_ = frameflags; }
    virtual void clearDataList() override { frameDataList.clear(); }
    virtual const std::vector<std::shared_ptr<RGYFrameData>>& dataList() const override { return frameDataList; }
    virtual std::vector<std::shared_ptr<RGYFrameData>>& dataList() override { return frameDataList; }
    virtual void setDataList(const std::vector<std::shared_ptr<RGYFrameData>>& dataList) override { frameDataList = dataList; }

    std::unique_ptr<RGYFrameMpp> createCopy() const;
protected:
    RGYFrameMpp(const RGYFrameMpp &) = delete;
    void operator =(const RGYFrameMpp &) = delete;

    virtual RGYFrameInfo getInfo() const override {
        auto info = infoMPP(mppframe.get());
        info.duration = duration_;
        info.flags = flags_;
        info.dataList = frameDataList;
        return info;
    }

    unique_mppframe mppframe;
    uint64_t duration_;
    RGY_FRAME_FLAGS flags_;
    std::vector<std::shared_ptr<RGYFrameData>> frameDataList;
};

VideoInfo videooutputinfo(
    const MPPCfg &prm,
    rgy_rational<int>& sar,
    RGY_PICSTRUCT picstruct,
    const VideoVUIInfo& vui);

int64_t rational_rescale(int64_t v, rgy_rational<int> from, rgy_rational<int> to);

#endif //#ifndef __MPP_UTIL_H__
