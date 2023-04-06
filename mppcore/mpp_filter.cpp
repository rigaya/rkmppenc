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

#include "rgy_input.h"
#include "mpp_param.h"
#include "mpp_filter.h"
#include "rga/rga.h"
#include "rga/im2d.hpp"
#include "rga/im2d_type.h"

// 参考 : https://github.com/airockchip/librga/blob/main/docs/Rockchip_Developer_Guide_RGA_EN.md
        
RGAFilter::RGAFilter() :
    RGYFilter(nullptr),
    m_frameBufRGA() {

}

RGAFilter::~RGAFilter() {
}

RGY_ERR RGAFilter::run_filter(const RGYFrameInfo *pInputFrame, RGYFrameInfo **ppOutputFrames, int *pOutputFrameNum, RGYOpenCLQueue &queue, const std::vector<RGYOpenCLEvent> &wait_events, RGYOpenCLEvent *event) {
    return RGY_ERR_UNSUPPORTED;
}

RGY_ERR RGAFilter::filter_rga(RGYFrameInfo *pInputFrame, RGYFrameInfo **ppOutputFrames, int *pOutputFrameNum, int *sync) {
    if (pInputFrame == nullptr) {
        *pOutputFrameNum = 0;
        ppOutputFrames[0] = nullptr;
    }
    if (m_param
        && m_param->bOutOverwrite //上書きか?
        && pInputFrame != nullptr && pInputFrame->ptr[0] != nullptr //入力が存在するか?
        && ppOutputFrames != nullptr && ppOutputFrames[0] == nullptr) { //出力先がセット可能か?
        ppOutputFrames[0] = pInputFrame;
        *pOutputFrameNum = 1;
    }
    const auto ret = run_filter_rga(pInputFrame, ppOutputFrames, pOutputFrameNum, sync);
    const int nOutFrame = *pOutputFrameNum;
    if (!m_param->bOutOverwrite && nOutFrame > 0) {
        if (m_pathThrough & FILTER_PATHTHROUGH_TIMESTAMP) {
            if (nOutFrame != 1) {
                AddMessage(RGY_LOG_ERROR, _T("timestamp path through can only be applied to 1-in/1-out filter.\n"));
                return RGY_ERR_INVALID_CALL;
            } else {
                ppOutputFrames[0]->timestamp = pInputFrame->timestamp;
                ppOutputFrames[0]->duration = pInputFrame->duration;
                ppOutputFrames[0]->inputFrameId = pInputFrame->inputFrameId;
            }
        }
        for (int i = 0; i < nOutFrame; i++) {
            if (m_pathThrough & FILTER_PATHTHROUGH_FLAGS)     ppOutputFrames[i]->flags = pInputFrame->flags;
            if (m_pathThrough & FILTER_PATHTHROUGH_PICSTRUCT) ppOutputFrames[i]->picstruct = pInputFrame->picstruct;
            if (m_pathThrough & FILTER_PATHTHROUGH_DATA)      ppOutputFrames[i]->dataList  = pInputFrame->dataList;
        }
    }
    return ret;
}

RGY_ERR RGAFilter::filter_iep(RGYFrameInfo *pInputFrame, RGYFrameInfo **ppOutputFrames, int *pOutputFrameNum, unique_event& sync) {
    if (pInputFrame == nullptr) {
        *pOutputFrameNum = 0;
        ppOutputFrames[0] = nullptr;
    }
    if (m_param
        && m_param->bOutOverwrite //上書きか?
        && pInputFrame != nullptr && pInputFrame->ptr[0] != nullptr //入力が存在するか?
        && ppOutputFrames != nullptr && ppOutputFrames[0] == nullptr) { //出力先がセット可能か?
        ppOutputFrames[0] = pInputFrame;
        *pOutputFrameNum = 1;
    }
    const auto ret = run_filter_iep(pInputFrame, ppOutputFrames, pOutputFrameNum, sync);
    const int nOutFrame = *pOutputFrameNum;
    if (!m_param->bOutOverwrite && nOutFrame > 0) {
        if (m_pathThrough & FILTER_PATHTHROUGH_TIMESTAMP) {
            if (nOutFrame != 1) {
                AddMessage(RGY_LOG_ERROR, _T("timestamp path through can only be applied to 1-in/1-out filter.\n"));
                return RGY_ERR_INVALID_CALL;
            } else {
                ppOutputFrames[0]->timestamp = pInputFrame->timestamp;
                ppOutputFrames[0]->duration = pInputFrame->duration;
                ppOutputFrames[0]->inputFrameId = pInputFrame->inputFrameId;
            }
        }
        for (int i = 0; i < nOutFrame; i++) {
            if (m_pathThrough & FILTER_PATHTHROUGH_FLAGS)     ppOutputFrames[i]->flags = pInputFrame->flags;
            if (m_pathThrough & FILTER_PATHTHROUGH_PICSTRUCT) ppOutputFrames[i]->picstruct = pInputFrame->picstruct;
            if (m_pathThrough & FILTER_PATHTHROUGH_DATA)      ppOutputFrames[i]->dataList  = pInputFrame->dataList;
        }
    }
    return ret;
}

RGY_ERR RGAFilter::AllocFrameBuf(const RGYFrameInfo &frame, int frames) {
    AddMessage(RGY_LOG_ERROR, _T("AllocFrameBuf not supported on RGA filters.\n"));
    return RGY_ERR_UNSUPPORTED;
}

RGY_ERR RGAFilter::AllocFrameBufRGA(const RGYFrameInfo &frame, int frames) {
    if ((int)m_frameBufRGA.size() == frames
        && !cmpFrameInfoCspResolution(&m_frameBufRGA[0]->frame, &frame)) {
        //すべて確保されているか確認
        bool allocated = true;
        for (size_t i = 0; i < m_frameBufRGA.size(); i++) {
            for (int iplane = 0; iplane < RGY_CSP_PLANES[m_frameBufRGA[i]->frame.csp]; iplane++) {
                if (m_frameBufRGA[i]->frame.ptr[iplane] == nullptr) {
                    allocated = false;
                    break;
                }
            }
        }
        if (allocated) {
            return RGY_ERR_NONE;
        }
    }
    m_frameBufRGA.clear();

    for (int i = 0; i < frames; i++) {
        auto uptr = std::make_unique<RGYMPPRGAFrame>();
        if (auto err = uptr->allocate(frame); err != RGY_ERR_NONE) {
            m_frameBufRGA.clear();
            return err;
        }
        m_frameBufRGA.push_back(std::move(uptr));
    }
    return RGY_ERR_NONE;
}

RGAFilterResize::RGAFilterResize() :
    RGAFilter() {

}

RGAFilterResize::~RGAFilterResize() {
    close();
}

void RGAFilterResize::close() {

}

RGY_ERR RGAFilterResize::checkParams(const RGYFilterParam *param) {
    auto prm = dynamic_cast<const RGYFilterParamResize*>(param);
    if (prm == nullptr) {
        AddMessage(RGY_LOG_ERROR, _T("Invalid param.\n"));
        return RGY_ERR_INVALID_PARAM;
    }
    //パラメータチェック
    if (prm->frameOut.height <= 0 || prm->frameOut.width <= 0) {
        AddMessage(RGY_LOG_ERROR, _T("Invalid parameter.\n"));
        return RGY_ERR_INVALID_PARAM;
    }
    if (prm->interp != RGY_VPP_RESIZE_AUTO
        && interp_rgy_to_rga(prm->interp) < INTER_NEAREST) {
        AddMessage(RGY_LOG_ERROR, _T("Invalid parameter for algo: %s.\n"), get_cx_desc(list_vpp_resize, prm->interp));
        return RGY_ERR_INVALID_PARAM;
    }
    return RGY_ERR_NONE;
}

RGY_ERR RGAFilterResize::init(shared_ptr<RGYFilterParam> param, shared_ptr<RGYLog> pPrintMes) {
    m_pLog = pPrintMes;

    auto err = checkParams(param.get());
    if (err != RGY_ERR_NONE) {
        return err;
    }

    auto prm = dynamic_cast<RGYFilterParamResize*>(param.get());
    if (!prm) {
        AddMessage(RGY_LOG_ERROR, _T("Invalid parameter type.\n"));
        return RGY_ERR_INVALID_PARAM;
    }
    if (prm->interp == RGY_VPP_RESIZE_AUTO) {
        prm->interp = RGY_VPP_RESIZE_RGA_BICUBIC; // 自動選択
    }
    err = AllocFrameBufRGA(prm->frameOut, 1);
    if (err != RGY_ERR_NONE) {
        AddMessage(RGY_LOG_ERROR, _T("failed to allocate memory: %s.\n"), get_err_mes(err));
        return RGY_ERR_MEMORY_ALLOC;
    }
    for (int i = 0; i < 4; i++) {
        param->frameOut.pitch[i] = m_frameBufRGA[0]->frame.pitch[i];
    }
    m_infoStr = strsprintf(_T("resize(%s): %dx%d -> %dx%d"),
        get_chr_from_value(list_vpp_resize, prm->interp),
        prm->frameIn.width, prm->frameIn.height,
        prm->frameOut.width, prm->frameOut.height);

    //コピーを保存
    m_param = param;
    return err;
}

rga_buffer_handle_t RGAFilterResize::getRGABufferHandle(const RGYFrameInfo *frame) {
    return importbuffer_virtualaddr((void *)frame->ptr[0], frame->pitch[0] * frame->height * RGY_CSP_PLANES[frame->csp]);
}

RGY_ERR RGAFilterResize::run_filter_rga(const RGYFrameInfo *pInputFrame, RGYFrameInfo **ppOutputFrames, int *pOutputFrameNum, int *sync) {
    RGY_ERR sts = RGY_ERR_NONE;
    if (pInputFrame->ptr[0] == nullptr) {
        return sts;
    }
    auto prm = dynamic_cast<RGYFilterParamResize*>(m_param.get());
    if (!prm) {
        AddMessage(RGY_LOG_ERROR, _T("Invalid parameter type.\n"));
        return RGY_ERR_INVALID_PARAM;
    }

    *pOutputFrameNum = 1;
    if (ppOutputFrames[0] == nullptr) {
        auto outFrame = m_frameBufRGA[0].get();
        ppOutputFrames[0] = &outFrame->frame;
    }
    RGYFrameInfo *const pOutFrame = ppOutputFrames[0];
    ppOutputFrames[0]->picstruct = pInputFrame->picstruct;

    if (pInputFrame->mem_type != RGY_MEM_TYPE_CPU) {
        AddMessage(RGY_LOG_ERROR, _T("Invalid input memory type: %s.\n"), get_memtype_str(pInputFrame->mem_type));
        return RGY_ERR_INVALID_FORMAT;
    }

    if (pOutFrame->mem_type != RGY_MEM_TYPE_CPU) {
        AddMessage(RGY_LOG_ERROR, _T("Invalid output memory type: %s.\n"), get_memtype_str(pOutFrame->mem_type));
        return RGY_ERR_INVALID_FORMAT;
    }

    if (pInputFrame->csp != RGY_CSP_NV12) {
        AddMessage(RGY_LOG_ERROR, _T("Invalid input memory format: %s.\n"), RGY_CSP_NAMES[pInputFrame->csp]);
        return RGY_ERR_INVALID_FORMAT;
    }

    if (pOutFrame->csp != RGY_CSP_NV12) {
        AddMessage(RGY_LOG_ERROR, _T("Invalid output memory format: %s.\n"), RGY_CSP_NAMES[pOutFrame->csp]);
        return RGY_ERR_INVALID_FORMAT;
    }

    rga_buffer_handle_t src_handle = getRGABufferHandle(pInputFrame);
    rga_buffer_handle_t dst_handle = getRGABufferHandle(pOutFrame);

    rga_buffer_t src = wrapbuffer_handle_t(src_handle, pInputFrame->width, pInputFrame->height, pInputFrame->pitch[0], pInputFrame->height, csp_rgy_to_rkrga(pInputFrame->csp));
    rga_buffer_t dst = wrapbuffer_handle_t(dst_handle, pOutFrame->width, pOutFrame->height, pOutFrame->pitch[0], pOutFrame->height, csp_rgy_to_rkrga(pOutFrame->csp));
    if (src.width == 0 || dst.width == 0) {
        AddMessage(RGY_LOG_ERROR, _T("Invalid in/out memory.\n"));
        return RGY_ERR_INVALID_FORMAT;
    }

    sts = err_to_rgy(imcheck(src, dst, {}, {}));
    if (sts != RGY_ERR_NONE) {
        AddMessage(RGY_LOG_ERROR, _T("Check error! %s"), get_err_mes(sts));
        return sts;
    }

    const int acquire_fence_fd = *sync;
    *sync = -1;
    const auto im2dinterp = interp_rgy_to_rga(prm->interp);
    sts = err_to_rgy(imresize(src, dst, 0.0, 0.0, im2dinterp, acquire_fence_fd, sync));
    if (sts != RGY_ERR_NONE) {
        AddMessage(RGY_LOG_ERROR, _T("Failed to run imresize: %s"), get_err_mes(sts));
        return sts;
    }
    //if (src_handle) {
    //    releasebuffer_handle(src_handle);
    //}
    //if (dst_handle) {
    //    releasebuffer_handle(dst_handle);
    //}
    return sts;
}

RGY_ERR RGAFilterResize::run_filter_iep(const RGYFrameInfo *pInputFrame, RGYFrameInfo **ppOutputFrames, int *pOutputFrameNum, unique_event& sync) {
    return RGY_ERR_UNSUPPORTED;
}

RGAFilterDeinterlaceIEP::RGAFilterDeinterlaceIEP() :
    RGAFilter(),
    m_iepCtx(std::unique_ptr<iep_com_ctx, decltype(&rockchip_iep2_api_release_ctx)>(nullptr, rockchip_iep2_api_release_ctx)),
    m_dilMode(IEP2_DIL_MODE_DISABLE),
    m_isTFF(true),
    m_mppBufInfo(),
    m_mppBufSrc(),
    m_mppBufDst(),
    m_frameGrp(nullptr),
    m_convertIn(),
    m_convertOut(),
    m_maxAsyncCount(0),
    m_frameCountIn(0),
    m_frameCountOut(0),
    m_prevOutFrameDuration(0),
    m_timeElapsed(0.0),
    m_threadWorker(),
    m_eventThreadStart(unique_event(nullptr, CloseEvent)),
    m_eventThreadFin(unique_event(nullptr, CloseEvent)),
    m_eventThreadFinSync(),
    m_threadNextOutBuf(),
    m_threadAbort(false) {
    m_name = _T("deint(iep)");
}

RGAFilterDeinterlaceIEP::~RGAFilterDeinterlaceIEP() {
    close();
}

void RGAFilterDeinterlaceIEP::close() {
    if (m_threadWorker->joinable()) {
        m_threadAbort = true;
        SetEvent(m_eventThreadStart.get());
        m_threadWorker->join();
    }
    m_eventThreadStart.reset();
    m_eventThreadFin.reset();
    m_threadAbort = false;
    AddMessage(RGY_LOG_INFO, _T("iep filter run time: %.3f ms\n"), m_timeElapsed * 0.001 / m_frameCountOut);

    for (auto& buf : m_mppBufSrc) {
        if (buf.mpp) {
            mpp_buffer_put(buf.mpp);
            buf.mpp = nullptr;
        }
    }
    m_mppBufSrc.clear();
    for (auto& buf : m_mppBufDst) {
        if (buf.mpp) {
            mpp_buffer_put(buf.mpp);
            buf.mpp = nullptr;
        }
    }
    m_mppBufDst.clear();
    if (m_frameGrp) {
        mpp_buffer_group_put(m_frameGrp);
        m_frameGrp = nullptr;
    }
    if (m_iepCtx) {
        m_iepCtx->ops->deinit(m_iepCtx->priv);
        m_iepCtx.reset();
    }
}

RGY_ERR RGAFilterDeinterlaceIEP::checkParams(const RGYFilterParam *param) {
    auto prm = dynamic_cast<const RGYFilterParamDeinterlaceIEP*>(param);
    if (prm == nullptr) {
        AddMessage(RGY_LOG_ERROR, _T("Invalid param.\n"));
        return RGY_ERR_INVALID_PARAM;
    }
    //パラメータチェック
    if (prm->frameOut.height <= 0 || prm->frameOut.width <= 0) {
        AddMessage(RGY_LOG_ERROR, _T("Invalid parameter.\n"));
        return RGY_ERR_INVALID_PARAM;
    }
    return RGY_ERR_NONE;
}

static const TCHAR *getDILModeName(const IEP2_DIL_MODE mode) {
    switch (mode) {
        #define DIL_MODE_NAME(x) case x: return _T(#x);
        DIL_MODE_NAME(IEP2_DIL_MODE_I5O2);
        DIL_MODE_NAME(IEP2_DIL_MODE_I5O1T);
        DIL_MODE_NAME(IEP2_DIL_MODE_I5O1B);
        DIL_MODE_NAME(IEP2_DIL_MODE_I2O2);
        DIL_MODE_NAME(IEP2_DIL_MODE_I1O1T);
        DIL_MODE_NAME(IEP2_DIL_MODE_I1O1B);
        default: return _T("Unknown");
        #undef DIL_MODE_NAME
    }
}

RGY_ERR RGAFilterDeinterlaceIEP::init(shared_ptr<RGYFilterParam> param, shared_ptr<RGYLog> pPrintMes) {
    m_pLog = pPrintMes;

    auto err = checkParams(param.get());
    if (err != RGY_ERR_NONE) {
        return err;
    }

    auto iep = get_iep_ctx();

    if (!m_iepCtx) {
        m_iepCtx = std::unique_ptr<iep_com_ctx, decltype(&put_iep_ctx)>(get_iep_ctx(), put_iep_ctx);
        err = err_to_rgy(m_iepCtx->ops->init(&m_iepCtx->priv));
        if (err != RGY_ERR_NONE) {
            AddMessage(RGY_LOG_ERROR, _T("Failed to init iep context.\n"));
            return RGY_ERR_INVALID_PARAM;
        }
        AddMessage(RGY_LOG_DEBUG, _T("Init iep context done: ver %d.\n"), m_iepCtx->ver);
    }

    auto prm = dynamic_cast<RGYFilterParamDeinterlaceIEP*>(param.get());
    if (!prm) {
        AddMessage(RGY_LOG_ERROR, _T("Invalid parameter type.\n"));
        return RGY_ERR_INVALID_PARAM;
    }
    m_isTFF = (prm->picstruct & RGY_PICSTRUCT_TFF) != 0;
    m_frameCountIn = 0;
    m_frameCountOut = 0;

    m_maxAsyncCount = 1;
    
    switch (prm->mode) {
    case IEPDeinterlaceMode::NORMAL_I5:
        m_dilMode = (m_isTFF) ? IEP2_DIL_MODE_I5O1T : IEP2_DIL_MODE_I5O1B;
        m_mppBufSrc.resize(3+m_maxAsyncCount);
        m_mppBufDst.resize(1);
        break;
    case IEPDeinterlaceMode::NORMAL_I2:
        m_dilMode = (m_isTFF) ? IEP2_DIL_MODE_I1O1T : IEP2_DIL_MODE_I1O1B;
        m_mppBufSrc.resize(1+m_maxAsyncCount);
        m_mppBufDst.resize(1);
        break;
    case IEPDeinterlaceMode::BOB_I5:
        m_dilMode = IEP2_DIL_MODE_I5O2;
        m_mppBufSrc.resize(3+m_maxAsyncCount);
        m_mppBufDst.resize(2);
        break;
    case IEPDeinterlaceMode::BOB_I2:
        m_dilMode = IEP2_DIL_MODE_I2O2;
        m_mppBufSrc.resize(1+m_maxAsyncCount);
        m_mppBufDst.resize(2);
        break;
    default:
        AddMessage(RGY_LOG_ERROR, _T("Unknown deinterlace mode.\n"));
        return RGY_ERR_UNSUPPORTED;
    }
    AddMessage(RGY_LOG_DEBUG, _T("Selected deinterlace mode: %s %s.\n"), getDILModeName(m_dilMode), m_isTFF ? _T("TFF") : _T("BFF"));
    
    err = allocateMppBuffer(param->frameIn);
    if (err != RGY_ERR_NONE) {
        AddMessage(RGY_LOG_ERROR, _T("failed to allocate memory: %s.\n"), get_err_mes(err));
        return err;
    }
    AddMessage(RGY_LOG_DEBUG, _T("Allocated memory for deinterlace.\n"));

    const auto& mppFrameBufSrcInfo = m_mppBufSrc.front().info;

    m_convertIn = std::make_unique<RGYConvertCSP>(prm->threadCsp, prm->threadParamCsp);
    if (m_convertIn->getFunc() == nullptr) {
        if (auto func = m_convertIn->getFunc(param->frameIn.csp, m_mppBufInfo.csp, false, RGY_SIMD::SIMD_ALL); func == nullptr) {
            AddMessage(RGY_LOG_ERROR, _T("Failed to find conversion(in) for %s -> %s.\n"),
                RGY_CSP_NAMES[param->frameIn.csp], RGY_CSP_NAMES[m_mppBufInfo.csp]);
            return RGY_ERR_UNSUPPORTED;
        } else {
            AddMessage(RGY_LOG_DEBUG, _T("Selected conversion(in) for %s -> %s [%s].\n"),
                RGY_CSP_NAMES[func->csp_from], RGY_CSP_NAMES[func->csp_to], get_simd_str(func->simd));
        }
    }
    m_convertOut = std::make_unique<RGYConvertCSP>(prm->threadCsp, prm->threadParamCsp);
    if (m_convertOut->getFunc() == nullptr) {
        if (auto func = m_convertOut->getFunc(m_mppBufInfo.csp, param->frameOut.csp, false, RGY_SIMD::SIMD_ALL); func == nullptr) {
            AddMessage(RGY_LOG_ERROR, _T("Failed to find conversion(out) for %s -> %s.\n"),
                RGY_CSP_NAMES[m_mppBufInfo.csp], RGY_CSP_NAMES[param->frameOut.csp]);
            return RGY_ERR_UNSUPPORTED;
        } else {
            AddMessage(RGY_LOG_DEBUG, _T("Selected conversion(out) for %s -> %s [%s].\n"),
                RGY_CSP_NAMES[func->csp_from], RGY_CSP_NAMES[func->csp_to], get_simd_str(func->simd));
        }
    }
    m_eventThreadStart = CreateEventUnique(nullptr, false, false, nullptr);
    AddMessage(RGY_LOG_DEBUG, _T("Create m_eventThreadStart.\n"));
    
    m_eventThreadFin = CreateEventUnique(nullptr, false, false, nullptr);
    AddMessage(RGY_LOG_DEBUG, _T("Create m_eventThreadFin.\n"));
    
    m_threadWorker = std::make_unique<std::thread>(&RGAFilterDeinterlaceIEP::workerThreadFunc, this);
    AddMessage(RGY_LOG_DEBUG, _T("Start worker thread.\n"));

    m_pathThrough &= ~(FILTER_PATHTHROUGH_PICSTRUCT | FILTER_PATHTHROUGH_TIMESTAMP | FILTER_PATHTHROUGH_FLAGS | FILTER_PATHTHROUGH_DATA);
    m_infoStr = strsprintf(_T("deinterlace(%s)"),
        get_chr_from_value(list_iep_deinterlace, (int)prm->mode));

    //コピーを保存
    m_param = param;
    return err;
}

RGY_ERR RGAFilterDeinterlaceIEP::allocateMppBuffer(const RGYFrameInfo& frameInfo) {
    if (frameInfo.csp != RGY_CSP_YV12 && frameInfo.csp != RGY_CSP_NV12) {
        AddMessage(RGY_LOG_ERROR, _T("Unsupported colorpace: %s.\n"), RGY_CSP_NAMES[frameInfo.csp]);
        return RGY_ERR_UNSUPPORTED;
    }
    m_mppBufInfo = frameInfo;
    m_mppBufInfo.csp = RGY_CSP_NV12;
    m_mppBufInfo.pitch[0] = ALIGN(frameInfo.width, 64);
    for (int i = 1; i < RGY_CSP_PLANES[m_mppBufInfo.csp]; i++) {
        m_mppBufInfo.pitch[i] = m_mppBufInfo.pitch[0];
    }
    const int planeSize = m_mppBufInfo.pitch[0] * frameInfo.height;
    int frameSize = 0;
    switch (m_mppBufInfo.csp) {
        case RGY_CSP_NV12: {
            frameSize = planeSize * 3 / 2;
        } break;
        default:
            AddMessage(RGY_LOG_ERROR, _T("Unspported colorspace: %s.\n"), RGY_CSP_NAMES[frameInfo.csp]);
            return RGY_ERR_UNSUPPORTED;
    }

    auto ret = err_to_rgy(mpp_buffer_group_get_internal(&m_frameGrp, MPP_BUFFER_TYPE_ION));
    if (ret != RGY_ERR_NONE) {
        AddMessage(RGY_LOG_ERROR, _T("failed to get mpp buffer group : %s\n"), get_err_mes(ret));
        return ret;
    }

    for (auto& buf : m_mppBufSrc) {
        ret = err_to_rgy(mpp_buffer_get(m_frameGrp, &buf.mpp, frameSize));
        if (ret != RGY_ERR_NONE) {
            AddMessage(RGY_LOG_ERROR, _T("failed to get buffer for input frame : %s\n"), get_err_mes(ret));
            return ret;
        }
        buf.info = setMPPBufferInfo(m_mppBufInfo.csp, m_mppBufInfo.width, m_mppBufInfo.height,
            m_mppBufInfo.pitch[0], m_mppBufInfo.height, buf.mpp);
        buf.img.format = IEP_FORMAT_YCbCr_420_SP;
    }
    for (auto& buf : m_mppBufDst) {
        ret = err_to_rgy(mpp_buffer_get(m_frameGrp, &buf.mpp, frameSize));
        if (ret != RGY_ERR_NONE) {
            AddMessage(RGY_LOG_ERROR, _T("failed to get buffer for output frame : %s\n"), get_err_mes(ret));
            return ret;
        }
        buf.info = setMPPBufferInfo(m_mppBufInfo.csp, m_mppBufInfo.width, m_mppBufInfo.height,
            m_mppBufInfo.pitch[0], m_mppBufInfo.height, buf.mpp);
        buf.img.format = IEP_FORMAT_YCbCr_420_SP;
    }
    return RGY_ERR_NONE;
}

RGY_ERR RGAFilterDeinterlaceIEP::setInputFrame(const RGYFrameInfo *pInputFrame) {
    auto& mppinfo = getBufSrc(m_frameCountIn)->info;
    
    auto crop = initCrop();
    m_convertIn->run((mppinfo.picstruct & RGY_PICSTRUCT_INTERLACED) ? 1 : 0,
        (void **)mppinfo.ptr, (const void **)pInputFrame->ptr,
        pInputFrame->width, pInputFrame->pitch[0], pInputFrame->pitch[1], mppinfo.pitch[0],
        pInputFrame->height, mppinfo.height, crop.c);
    copyFrameProp(&mppinfo, pInputFrame);
    m_frameCountIn++;
    return RGY_ERR_NONE;
}

RGY_ERR RGAFilterDeinterlaceIEP::setOutputFrame(RGYFrameInfo *pOutputFrame, const IepBufferInfo *bufDst) {
    auto& mppinfo = bufDst->info;
    auto crop = initCrop();
    m_convertOut->run(0,
        (void **)pOutputFrame->ptr, (const void **)mppinfo.ptr,
        mppinfo.width, mppinfo.pitch[0], mppinfo.pitch[1], pOutputFrame->pitch[0],
        mppinfo.height, pOutputFrame->height, crop.c);
    copyFrameProp(pOutputFrame, &mppinfo);
    return RGY_ERR_NONE;
}

RGY_ERR RGAFilterDeinterlaceIEP::run_filter_rga(const RGYFrameInfo *pInputFrame, RGYFrameInfo **ppOutputFrames, int *pOutputFrameNum, int *sync) {
    return RGY_ERR_UNSUPPORTED;
}

RGY_ERR RGAFilterDeinterlaceIEP::run_filter_iep(const RGYFrameInfo *pInputFrame, RGYFrameInfo **ppOutputFrames, int *pOutputFrameNum, unique_event& sync) {
    RGY_ERR sts = RGY_ERR_NONE;
    auto prm = dynamic_cast<RGYFilterParamDeinterlaceIEP*>(m_param.get());
    if (!prm) {
        AddMessage(RGY_LOG_ERROR, _T("Invalid parameter type.\n"));
        return RGY_ERR_INVALID_PARAM;
    }

    RGYFrameInfo *const pOutFrame = ppOutputFrames[0];
    ppOutputFrames[0]->picstruct = RGY_PICSTRUCT_FRAME;

    if (pInputFrame && pInputFrame->ptr[0]) {
        if (pInputFrame->mem_type != RGY_MEM_TYPE_CPU) {
            AddMessage(RGY_LOG_ERROR, _T("Invalid input memory type: %s.\n"), get_memtype_str(pInputFrame->mem_type));
            return RGY_ERR_INVALID_FORMAT;
        }

        if (pOutFrame->mem_type != RGY_MEM_TYPE_CPU) {
            AddMessage(RGY_LOG_ERROR, _T("Invalid output memory type: %s.\n"), get_memtype_str(pOutFrame->mem_type));
            return RGY_ERR_INVALID_FORMAT;
        }

        if (pInputFrame->csp != RGY_CSP_NV12) {
            AddMessage(RGY_LOG_ERROR, _T("Invalid input memory format: %s.\n"), RGY_CSP_NAMES[pInputFrame->csp]);
            return RGY_ERR_INVALID_FORMAT;
        }

        if (pOutFrame->csp != RGY_CSP_NV12) {
            AddMessage(RGY_LOG_ERROR, _T("Invalid output memory format: %s.\n"), RGY_CSP_NAMES[pOutFrame->csp]);
            return RGY_ERR_INVALID_FORMAT;
        }

        sts = setInputFrame(pInputFrame);
        if (sts != RGY_ERR_NONE) {
            AddMessage(RGY_LOG_ERROR, _T("Failed to set input frame: %s.\n"), get_err_mes(sts));
            return sts;
        }
    }
    if (m_frameCountIn < 2
        && (m_dilMode == IEP2_DIL_MODE_I5O2
         || m_dilMode == IEP2_DIL_MODE_I5O1T
         || m_dilMode == IEP2_DIL_MODE_I5O1B
         || m_dilMode == IEP2_DIL_MODE_I2O2)) {
        *pOutputFrameNum = 0;
        return RGY_ERR_NONE; // 出力なし
    }
    if (pInputFrame->ptr[0] == nullptr) {
        if (m_frameCountIn == m_frameCountOut) {
            return RGY_ERR_NONE;
        }
    }

    sync = CreateEventUnique(nullptr, false, false, nullptr);

    // 前回のIEPの実行終了を待つ
    WaitForSingleObject(m_eventThreadFin.get(), INFINITE);

    *pOutputFrameNum = 1;
    if (m_dilMode == IEP2_DIL_MODE_I5O2
     || m_dilMode == IEP2_DIL_MODE_I2O2) {
        *pOutputFrameNum = 2;
    }
    for (int i = 0; i < *pOutputFrameNum; i++) {
        if (ppOutputFrames[i] == nullptr) {
            AddMessage(RGY_LOG_ERROR, _T("No output buffer for %d frame.\n"), i);
            return RGY_ERR_UNSUPPORTED;
        }
        copyFrameProp(ppOutputFrames[i], &getBufSrc(m_frameCountOut)->info);
        ppOutputFrames[i]->picstruct = RGY_PICSTRUCT_FRAME;
        if (m_frameCountOut < m_frameCountIn-1) {
            const auto duration = getBufSrc(m_frameCountOut+1)->info.timestamp - getBufSrc(m_frameCountOut)->info.timestamp;
            if (i > 0) { //bob化の場合
                ppOutputFrames[i]->timestamp += duration / 2;
                ppOutputFrames[i]->duration = duration - ppOutputFrames[0]->duration;
            } else {
                ppOutputFrames[i]->duration = duration / 2;
            }
        } else {
            ppOutputFrames[i]->duration = m_prevOutFrameDuration;
            if (i > 0) { //bob化の場合
                ppOutputFrames[i]->timestamp += m_prevOutFrameDuration;
            }
        }
        m_prevOutFrameDuration = ppOutputFrames[i]->duration;
        // 処理スレッドに出力先を設定
        m_threadNextOutBuf[i] = (*ppOutputFrames[i]);
    }
    m_eventThreadFinSync = sync.get();
    // 処理スレッドに処理開始を通知
    SetEvent(m_eventThreadStart.get());
    return sts;
}

RGY_ERR RGAFilterDeinterlaceIEP::workerThreadFunc() {
    RGY_ERR sts = RGY_ERR_NONE;
    // メインスレッドからの開始通知待ち
    SetEvent(m_eventThreadFin.get());
    WaitForSingleObject(m_eventThreadStart.get(), INFINITE);

    while (!m_threadAbort) {
        std::vector<IepBufferInfo*> dst = { &m_mppBufDst[0] };
        if (m_dilMode == IEP2_DIL_MODE_I5O2
        ||  m_dilMode == IEP2_DIL_MODE_I2O2) {
            dst.push_back(&m_mppBufDst[1]);
        }
        if (m_dilMode == IEP2_DIL_MODE_I5O2
         || m_dilMode == IEP2_DIL_MODE_I5O1T
         || m_dilMode == IEP2_DIL_MODE_I5O1B) {
            sts = runFilter(dst, { getBufSrc(std::max<int64_t>(0, m_frameCountOut-1)),
                                   getBufSrc(m_frameCountOut + 0),
                                   getBufSrc(std::min(m_frameCountOut + 1, m_frameCountIn-1)) });
            if (sts != RGY_ERR_NONE) {
                return sts;
            }
        } else if (m_dilMode == IEP2_DIL_MODE_I2O2
                || m_dilMode == IEP2_DIL_MODE_I1O1T
                || m_dilMode == IEP2_DIL_MODE_I1O1B) {
            sts = runFilter(dst, { getBufSrc(m_frameCountOut) });
            if (sts != RGY_ERR_NONE) {
                return sts;
            }
        } else {
            AddMessage(RGY_LOG_ERROR, _T("Unknown dil mode: %d.\n"), m_dilMode);
            return RGY_ERR_UNSUPPORTED;
        }
    auto start = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i < dst.size(); i++) {
            sts = setOutputFrame(&m_threadNextOutBuf[i], dst[i]);
            if (sts != RGY_ERR_NONE) {
                return sts;
            }
        }
    auto fin = std::chrono::high_resolution_clock::now();
    m_timeElapsed += std::chrono::duration_cast<std::chrono::microseconds>(fin-start).count();

        m_frameCountOut++;
        // メインスレッドに終了を通知
        SetEvent(m_eventThreadFin.get());
        // フレームに終了を通知
        SetEvent(m_eventThreadFinSync);
        // メインスレッドからの開始通知待ち
        WaitForSingleObject(m_eventThreadStart.get(), INFINITE);
    }
    AddMessage(RGY_LOG_DEBUG, _T("worker thread terminate.\n"));
    return sts;
}

RGY_ERR RGAFilterDeinterlaceIEP::setImage(IepBufferInfo *buffer, const IepCmd cmd) {
    auto fd = mpp_buffer_get_fd(buffer->mpp);
    RK_S32 y_size = buffer->info.pitch[0] * buffer->info.height;
    buffer->img.act_w = buffer->info.width;
    buffer->img.act_h = buffer->info.height;
    buffer->img.vir_w = buffer->info.pitch[0];
    buffer->img.vir_h = buffer->info.height;
    buffer->img.x_off = 0;
    buffer->img.y_off = 0;
    buffer->img.mem_addr = fd;
    buffer->img.uv_addr = fd + (y_size << 10);
    buffer->img.v_addr = fd + ((y_size + y_size / 4) << 10);
    AddMessage(RGY_LOG_TRACE, _T("SetImage %d: fd %d, buffermpp %p\n"), cmd, fd, buffer->mpp);
    MPP_RET ret = m_iepCtx->ops->control(m_iepCtx->priv, cmd, &buffer->img);
    return err_to_rgy(ret);
}

RGY_ERR RGAFilterDeinterlaceIEP::runFilter(std::vector<IepBufferInfo*> dst, const std::vector<IepBufferInfo*> src) {
#define CHECK_SETIMG(x) { if (auto err = (x); err != RGY_ERR_NONE) { AddMessage(RGY_LOG_ERROR, _T("Failed to set image: %s.\n"), get_err_mes(err)); return err; }}
    if (dst.size() == 1) {
        CHECK_SETIMG(setImage(dst[0], IEP_CMD_SET_DST));
        CHECK_SETIMG(setImage(dst[0], IEP_CMD_SET_DEI_DST1));
    } else if (dst.size() == 2) {
        CHECK_SETIMG(setImage(dst[0], IEP_CMD_SET_DST));
        CHECK_SETIMG(setImage(dst[1], IEP_CMD_SET_DEI_DST1));
    } else {
        AddMessage(RGY_LOG_ERROR, _T("Unknwon dst frame count: %d.\n"), dst.size());
        return RGY_ERR_UNKNOWN;
    }
    if (src.size() == 1) {
        CHECK_SETIMG(setImage(src[0], IEP_CMD_SET_SRC));      // curr
        CHECK_SETIMG(setImage(src[0], IEP_CMD_SET_DEI_SRC1)); // next
        CHECK_SETIMG(setImage(src[0], IEP_CMD_SET_DEI_SRC2)); // prev
    } else if (src.size() == 3) {
        CHECK_SETIMG(setImage(src[1], IEP_CMD_SET_SRC));      // curr
        CHECK_SETIMG(setImage(src[2], IEP_CMD_SET_DEI_SRC1)); // next
        CHECK_SETIMG(setImage(src[0], IEP_CMD_SET_DEI_SRC2)); // prev
    } else {
        AddMessage(RGY_LOG_ERROR, _T("Unknwon src frame count: %d.\n"), src.size());
        return RGY_ERR_UNKNOWN;
    }
#undef CHECK_SETIMG

    struct iep2_api_params params;
    params.ptype = IEP2_PARAM_TYPE_MODE;
    params.param.mode.dil_mode = m_dilMode;
    params.param.mode.out_mode = IEP2_OUT_MODE_LINE;
    params.param.mode.dil_order = m_isTFF ? IEP2_FIELD_ORDER_TFF : IEP2_FIELD_ORDER_BFF;
    params.param.mode.ff_mode = IEP2_FF_MODE_FRAME;
    auto err = err_to_rgy(m_iepCtx->ops->control(m_iepCtx->priv, IEP_CMD_SET_DEI_CFG, &params));
    if (err != RGY_ERR_NONE) {
        AddMessage(RGY_LOG_ERROR, _T("Failed to set deinterlace mode: %s.\n"), get_err_mes(err));
        return err;
    }
    AddMessage(RGY_LOG_TRACE, _T("Set deinterlace mode.\n"));

    params.ptype = IEP2_PARAM_TYPE_COM;
    params.param.com.sfmt = IEP2_FMT_YUV420; // nv12
    params.param.com.dfmt = IEP2_FMT_YUV420; // nv12
    params.param.com.sswap = IEP2_YUV_SWAP_SP_UV; // nv12
    params.param.com.dswap = IEP2_YUV_SWAP_SP_UV; // nv12
    params.param.com.width = src[0]->info.width;
    params.param.com.height = src[0]->info.height;
    params.param.com.hor_stride = src[0]->info.pitch[0];
    err = err_to_rgy(m_iepCtx->ops->control(m_iepCtx->priv, IEP_CMD_SET_DEI_CFG, &params));
    if (err != RGY_ERR_NONE) {
        AddMessage(RGY_LOG_ERROR, _T("Failed to set deinterlace params: %s.\n"), get_err_mes(err));
        return err;
    }
    AddMessage(RGY_LOG_TRACE, _T("Set deinterlace params.\n"));

    struct iep2_api_info dei_info;
    err = err_to_rgy(m_iepCtx->ops->control(m_iepCtx->priv, IEP_CMD_RUN_SYNC, &dei_info));
    if (err != RGY_ERR_NONE) {
        AddMessage(RGY_LOG_ERROR, _T("Failed to run filter: %s.\n"), get_err_mes(err));
        return err;
    }
    AddMessage(RGY_LOG_TRACE, _T("Deinterlace result: dil_order %d, frm_mode %d.\n"), dei_info.dil_order, dei_info.frm_mode ? 1 : 0);

    const int out_order = dei_info.dil_order == IEP2_FIELD_ORDER_BFF ? 1 : 0;
    if (out_order && dst.size() == 2) {
        std::swap(dst[0], dst[1]);
    }
    return RGY_ERR_NONE;
}
