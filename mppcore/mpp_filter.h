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

#pragma once
#include "rgy_version.h"
#include "rgy_err.h"
#include "rgy_util.h"
#include "rgy_filter.h"
#include "mpp_util.h"

#include "iep2_api.h"


class RGAFilter : public RGYFilter {
public:
    RGAFilter();
    virtual ~RGAFilter();
    RGY_ERR filter_rga(RGYFrameInfo *pInputFrame, RGYFrameInfo **ppOutputFrames, int *pOutputFrameNum, int *sync);
protected:
    virtual RGY_ERR run_filter(const RGYFrameInfo *pInputFrame, RGYFrameInfo **ppOutputFrames, int *pOutputFrameNum, RGYOpenCLQueue &queue, const std::vector<RGYOpenCLEvent> &wait_events, RGYOpenCLEvent *event) override;
    virtual RGY_ERR run_filter_rga(const RGYFrameInfo *pInputFrame, RGYFrameInfo **ppOutputFrames, int *pOutputFrameNum, int *sync) = 0;
    virtual RGY_ERR AllocFrameBuf(const RGYFrameInfo &frame, int frames) override;
    virtual RGY_ERR AllocFrameBufRGA(const RGYFrameInfo &frame, int frames);

    std::vector<std::unique_ptr<RGYMPPRGAFrame>> m_frameBufRGA;
};

class RGAFilterResize : public RGAFilter {
public:
    RGAFilterResize();
    virtual ~RGAFilterResize();
    virtual RGY_ERR init(shared_ptr<RGYFilterParam> param, shared_ptr<RGYLog> pPrintMes) override;
protected:
    virtual RGY_ERR run_filter_rga(const RGYFrameInfo *pInputFrame, RGYFrameInfo **ppOutputFrames, int *pOutputFrameNum, int *sync) override;
    RGY_ERR checkParams(const RGYFilterParam *param);  
    virtual void close() override;

    rga_buffer_handle_t getRGABufferHandle(const RGYFrameInfo *frame);
};

class RGYFilterParamDeinterlaceIEP : public RGYFilterParam {
public:
    RGY_PICSTRUCT picstruct;
    IEPDeinterlaceMode mode;
    int threadCsp;
    RGYParamThread threadParamCsp;
    RGYFilterParamDeinterlaceIEP() : picstruct(RGY_PICSTRUCT_AUTO), mode(IEPDeinterlaceMode::DISABLED), threadCsp(0), threadParamCsp() {};
    virtual ~RGYFilterParamDeinterlaceIEP() {};
};

class RGYConvertCSP;

class RGAFilterDeinterlaceIEP : public RGAFilter {
public:
    RGAFilterDeinterlaceIEP();
    virtual ~RGAFilterDeinterlaceIEP();
    virtual RGY_ERR init(shared_ptr<RGYFilterParam> param, shared_ptr<RGYLog> pPrintMes) override;
protected:
    struct IepBufferInfo {
        MppBuffer mpp;
        IepImg img;
        RGYFrameInfo info;

        IepBufferInfo() : mpp(nullptr), img(), info() {};
    };
    virtual RGY_ERR run_filter_rga(const RGYFrameInfo *pInputFrame, RGYFrameInfo **ppOutputFrames, int *pOutputFrameNum, int *sync) override;
    RGY_ERR checkParams(const RGYFilterParam *param);
    RGY_ERR allocateMppBuffer(const RGYFrameInfo& frameInfo);
    RGY_ERR setInputFrame(const RGYFrameInfo *pInputFrame);
    RGY_ERR setOutputFrame(RGYFrameInfo *pOutputFrame, const IepBufferInfo *bufDst);
    RGY_ERR setImage(IepBufferInfo *buffer, const IepCmd cmd);
    RGY_ERR runFilter(std::vector<IepBufferInfo*> dst, const std::vector<IepBufferInfo*> src);
    virtual void close() override;

    IepBufferInfo *getBufSrc(const int64_t idx) { return &m_mppBufSrc[idx % m_mppBufSrc.size()]; }
    IepBufferInfo *getBufDst(const int64_t idx) { return &m_mppBufDst[idx % m_mppBufDst.size()]; }

    std::unique_ptr<iep_com_ctx, decltype(&put_iep_ctx)> m_iepCtx;
    IEP2_DIL_MODE m_dilMode;
    bool m_isTFF;
    RGYFrameInfo m_mppBufInfo;
    std::vector<IepBufferInfo> m_mppBufSrc;
    std::vector<IepBufferInfo> m_mppBufDst;
    MppBufferGroup m_frameGrp;
    std::unique_ptr<RGYConvertCSP> m_convertIn;
    std::unique_ptr<RGYConvertCSP> m_convertOut;
    int64_t m_frameCountIn;
    int64_t m_frameCountOut;
    int64_t m_prevOutFrameDuration;
};
