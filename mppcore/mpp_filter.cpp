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

#include "rgy_input.h"
#include "mpp_param.h"
#include "mpp_filter.h"
#include "rga/rga.h"
#include "rga/im2d.hpp"
#include "rga/im2d_type.h"

// 参考 : https://github.com/airockchip/librga/blob/main/docs/Rockchip_Developer_Guide_RGA_EN.md
        
RGAFilter::RGAFilter() :
    m_name(),
    m_infoStr(),
    m_pLog(),
    m_param(),
    m_pathThrough(FILTER_PATHTHROUGH_ALL) {

}

RGAFilter::~RGAFilter() {
}

RGY_ERR RGAFilter::filter_rga(RGYFrameMpp *pInputFrame, RGYFrameMpp **ppOutputFrames, int *pOutputFrameNum, int *sync) {
    if (pInputFrame == nullptr) {
        *pOutputFrameNum = 0;
    }
    if (m_param
        && m_param->bOutOverwrite //上書きか?
        && pInputFrame != nullptr //入力が存在するか?
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
                ppOutputFrames[0]->setTimestamp(pInputFrame->timestamp());
                ppOutputFrames[0]->setDuration(pInputFrame->duration());
                ppOutputFrames[0]->setInputFrameId(pInputFrame->inputFrameId());
            }
        }
        for (int i = 0; i < nOutFrame; i++) {
            if (m_pathThrough & FILTER_PATHTHROUGH_FLAGS)     ppOutputFrames[i]->setFlags(pInputFrame->flags());
            if (m_pathThrough & FILTER_PATHTHROUGH_PICSTRUCT) ppOutputFrames[i]->setPicstruct(pInputFrame->picstruct());
            if (m_pathThrough & FILTER_PATHTHROUGH_DATA)      ppOutputFrames[i]->setDataList(pInputFrame->dataList());
        }
    }
    return ret;
}

RGY_ERR RGAFilter::filter_iep(RGYFrameMpp *pInputFrame, RGYFrameMpp **ppOutputFrames, int *pOutputFrameNum, unique_event& sync) {
    if (pInputFrame == nullptr) {
        *pOutputFrameNum = 0;
    }
    if (m_param
        && m_param->bOutOverwrite //上書きか?
        && pInputFrame != nullptr //入力が存在するか?
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
                ppOutputFrames[0]->setTimestamp(pInputFrame->timestamp());
                ppOutputFrames[0]->setDuration(pInputFrame->duration());
                ppOutputFrames[0]->setInputFrameId(pInputFrame->inputFrameId());
            }
        }
        for (int i = 0; i < nOutFrame; i++) {
            if (m_pathThrough & FILTER_PATHTHROUGH_FLAGS)     ppOutputFrames[i]->setFlags(pInputFrame->flags());
            if (m_pathThrough & FILTER_PATHTHROUGH_PICSTRUCT) ppOutputFrames[i]->setPicstruct(pInputFrame->picstruct());
            if (m_pathThrough & FILTER_PATHTHROUGH_DATA)      ppOutputFrames[i]->setDataList(pInputFrame->dataList());
        }
    }
    return ret;
}

rga_buffer_handle_t RGAFilter::getRGABufferHandle(RGYFrameMpp *frame) {
    const auto fd = mpp_buffer_get_fd(mpp_frame_get_buffer(frame->mpp()));
    return importbuffer_fd(fd, frame->pitch(0) * frame->height() * RGY_CSP_PLANES[frame->csp()]);
}

RGAFilterCrop::RGAFilterCrop() :
    RGAFilter() {
    m_name = _T("Crop(rga)");
}

RGAFilterCrop::~RGAFilterCrop() {
    close();
}

void RGAFilterCrop::close() {

}

RGY_ERR RGAFilterCrop::checkParams(const RGYFilterParam *param) {
    auto prm = dynamic_cast<const RGYFilterParamCrop*>(param);
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

RGY_ERR RGAFilterCrop::init(shared_ptr<RGYFilterParam> param, shared_ptr<RGYLog> pPrintMes) {
    m_pLog = pPrintMes;

    auto err = checkParams(param.get());
    if (err != RGY_ERR_NONE) {
        return err;
    }
    auto prm = std::dynamic_pointer_cast<RGYFilterParamCrop>(param);
    if (!prm) {
        AddMessage(RGY_LOG_ERROR, _T("Invalid parameter type.\n"));
        return RGY_ERR_INVALID_PARAM;
    }

    prm->frameOut.height = prm->frameIn.height - prm->crop.e.bottom - prm->crop.e.up;
    prm->frameOut.width = prm->frameIn.width - prm->crop.e.left - prm->crop.e.right;
    setFilterInfo(strsprintf(_T("crop(rga): %d,%d,%d,%d"),
        prm->crop.e.left, prm->crop.e.up, prm->crop.e.right, prm->crop.e.bottom));

    //コピーを保存
    m_param = param;
    return err;
}

RGY_ERR RGAFilterCrop::run_filter_rga(RGYFrameMpp *pInputFrame, RGYFrameMpp **ppOutputFrames, int *pOutputFrameNum, int *sync) {
    RGY_ERR sts = RGY_ERR_NONE;
    if (pInputFrame == nullptr) {
        return sts;
    }

    *pOutputFrameNum = 1;
    RGYFrameMpp *const pOutFrame = ppOutputFrames[0];
    auto prm = std::dynamic_pointer_cast<RGYFilterParamCrop>(m_param);
    if (!prm) {
        AddMessage(RGY_LOG_ERROR, _T("Invalid parameter type.\n"));
        return RGY_ERR_INVALID_PARAM;
    }

    if (csp_rgy_to_rkrga(pInputFrame->csp()) == RK_FORMAT_UNKNOWN) {
        AddMessage(RGY_LOG_ERROR, _T("Invalid input memory format: %s.\n"), RGY_CSP_NAMES[pInputFrame->csp()]);
        return RGY_ERR_INVALID_FORMAT;
    }

    if (csp_rgy_to_rkrga(pOutFrame->csp()) == RK_FORMAT_UNKNOWN) {
        AddMessage(RGY_LOG_ERROR, _T("Invalid output memory format: %s.\n"), RGY_CSP_NAMES[pOutFrame->csp()]);
        return RGY_ERR_INVALID_FORMAT;
    }

    rga_buffer_handle_t src_handle = getRGABufferHandle(pInputFrame);
    rga_buffer_handle_t dst_handle = getRGABufferHandle(pOutFrame);

    // この指定方法では、うまく動作しない
    rga_buffer_t src = wrapbuffer_handle_t(src_handle, pInputFrame->width(), pInputFrame->height(),
        pInputFrame->pitch(0), mpp_frame_get_ver_stride(pInputFrame->mpp()), csp_rgy_to_rkrga(pInputFrame->csp()));
    rga_buffer_t dst = wrapbuffer_handle_t(dst_handle, pOutFrame->width(), pOutFrame->height(),
        pOutFrame->pitch(0), mpp_frame_get_ver_stride(pOutFrame->mpp()), csp_rgy_to_rkrga(pOutFrame->csp()));

    // こちらを使用すべき
    //rga_buffer_t src = wrapbuffer_handle(src_handle, pInputFrame->width(), pInputFrame->height(), csp_rgy_to_rkrga(pInputFrame->csp()));
    //rga_buffer_t dst = wrapbuffer_handle(dst_handle, pOutFrame->width(), pOutFrame->height(), csp_rgy_to_rkrga(pOutFrame->csp()));
    if (src.width == 0 || dst.width == 0) {
        AddMessage(RGY_LOG_ERROR, _T("Invalid in/out memory.\n"));
        return RGY_ERR_INVALID_FORMAT;
    }

    sts = err_to_rgy(imcheck(src, dst, {}, {}));
    if (sts != RGY_ERR_NONE) {
        AddMessage(RGY_LOG_ERROR, _T("Check error! %s"), get_err_mes(sts));
        return sts;
    }
    im_rect crop_rect;
    crop_rect.x = prm->crop.e.left;
    crop_rect.y = prm->crop.e.up;
    crop_rect.width = prm->frameOut.width;
    crop_rect.height = prm->frameOut.height;

    const int acquire_fence_fd = *sync;
    *sync = 0;
    sts = err_to_rgy(imcrop(src, dst, crop_rect, 0, sync));
    if (sts != RGY_ERR_NONE) {
        AddMessage(RGY_LOG_ERROR, _T("Failed to run imcrop: %s"), get_err_mes(sts));
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

RGY_ERR RGAFilterCrop::run_filter_iep(RGYFrameMpp *pInputFrame, RGYFrameMpp **ppOutputFrames, int *pOutputFrameNum, unique_event& sync) {
    return RGY_ERR_UNSUPPORTED;
}

RGAFilterCspConv::RGAFilterCspConv() :
    RGAFilter(),
    m_cvt_mode(IM_COLOR_SPACE_DEFAULT) {
    m_name = _T("cspconv(rga)");
}

RGAFilterCspConv::~RGAFilterCspConv() {
    close();
}

void RGAFilterCspConv::close() {

}

RGY_ERR RGAFilterCspConv::checkParams(const RGYFilterParam *param) {
    auto prm = dynamic_cast<const RGYFilterParamCrop*>(param);
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

RGY_ERR RGAFilterCspConv::init(shared_ptr<RGYFilterParam> param, shared_ptr<RGYLog> pPrintMes) {
    m_pLog = pPrintMes;

    auto err = checkParams(param.get());
    if (err != RGY_ERR_NONE) {
        return err;
    }
    auto prm = std::dynamic_pointer_cast<RGYFilterParamCrop>(param);
    if (!prm) {
        AddMessage(RGY_LOG_ERROR, _T("Invalid parameter type.\n"));
        return RGY_ERR_INVALID_PARAM;
    }
    if (rgy_chromafmt_is_rgb(RGY_CSP_CHROMA_FORMAT[param->frameIn.csp]) != rgy_chromafmt_is_rgb(RGY_CSP_CHROMA_FORMAT[param->frameOut.csp])) {
        if (rgy_chromafmt_is_rgb(RGY_CSP_CHROMA_FORMAT[param->frameIn.csp])) {
            switch (prm->matrix) {
            case RGY_MATRIX_BT470_BG:
            case RGY_MATRIX_ST170_M:
                m_cvt_mode = IM_RGB_TO_YUV_BT601_LIMIT;
                break;
            default:
                m_cvt_mode = IM_RGB_TO_YUV_BT709_LIMIT;
                break;
            }
        } else {
            switch (prm->matrix) {
            case RGY_MATRIX_BT470_BG:
            case RGY_MATRIX_ST170_M:
                m_cvt_mode = IM_YUV_TO_RGB_BT601_LIMIT;
                break;
            default:
                m_cvt_mode = IM_YUV_TO_RGB_BT709_LIMIT;
                break;
            }
        }
    }

    setFilterInfo(strsprintf(_T("cspconv(rga): %s -> %s"),
        RGY_CSP_NAMES[param->frameIn.csp], RGY_CSP_NAMES[param->frameOut.csp]));

    //コピーを保存
    m_param = param;
    return err;
}

RGY_ERR RGAFilterCspConv::run_filter_rga(RGYFrameMpp *pInputFrame, RGYFrameMpp **ppOutputFrames, int *pOutputFrameNum, int *sync) {
    RGY_ERR sts = RGY_ERR_NONE;
    if (pInputFrame == nullptr) {
        return sts;
    }

    *pOutputFrameNum = 1;
    RGYFrameMpp *const pOutFrame = ppOutputFrames[0];

    if (csp_rgy_to_rkrga(pInputFrame->csp()) == RK_FORMAT_UNKNOWN) {
        AddMessage(RGY_LOG_ERROR, _T("Invalid input memory format: %s.\n"), RGY_CSP_NAMES[pInputFrame->csp()]);
        return RGY_ERR_INVALID_FORMAT;
    }

    if (csp_rgy_to_rkrga(pOutFrame->csp()) == RK_FORMAT_UNKNOWN) {
        AddMessage(RGY_LOG_ERROR, _T("Invalid output memory format: %s.\n"), RGY_CSP_NAMES[pOutFrame->csp()]);
        return RGY_ERR_INVALID_FORMAT;
    }

    rga_buffer_handle_t src_handle = getRGABufferHandle(pInputFrame);
    rga_buffer_handle_t dst_handle = getRGABufferHandle(pOutFrame);

    // この指定方法では、うまく動作しない
    //rga_buffer_t src = wrapbuffer_handle_t(src_handle, pInputFrame->width(), pInputFrame->height(),
    //    pInputFrame->pitch(0), mpp_frame_get_ver_stride(pInputFrame->mpp()), csp_rgy_to_rkrga(pInputFrame->csp()));
    //rga_buffer_t dst = wrapbuffer_handle_t(dst_handle, pOutFrame->width(), pOutFrame->height(),
    //    pOutFrame->pitch(0), mpp_frame_get_ver_stride(pOutFrame->mpp()), csp_rgy_to_rkrga(pOutFrame->csp()));

    // こちらを使用すべき
    rga_buffer_t src = wrapbuffer_handle(src_handle, pInputFrame->width(), pInputFrame->height(), csp_rgy_to_rkrga(pInputFrame->csp()));
    rga_buffer_t dst = wrapbuffer_handle(dst_handle, pOutFrame->width(), pOutFrame->height(), csp_rgy_to_rkrga(pOutFrame->csp()));
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
    *sync = 0;
    sts = err_to_rgy(imcvtcolor(src, dst, csp_rgy_to_rkrga(pInputFrame->csp()), csp_rgy_to_rkrga(pOutFrame->csp()), m_cvt_mode, acquire_fence_fd, sync));
    if (sts != RGY_ERR_NONE) {
        AddMessage(RGY_LOG_ERROR, _T("Failed to run imcvtcolor: %s"), get_err_mes(sts));
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

RGY_ERR RGAFilterCspConv::run_filter_iep(RGYFrameMpp *pInputFrame, RGYFrameMpp **ppOutputFrames, int *pOutputFrameNum, unique_event& sync) {
    return RGY_ERR_UNSUPPORTED;
}

RGAFilterResize::RGAFilterResize() :
    RGAFilter() {
    m_name = _T("resize(rga)");
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

    setFilterInfo(strsprintf(_T("resize(%s): %dx%d -> %dx%d"),
        get_chr_from_value(list_vpp_resize, prm->interp),
        prm->frameIn.width, prm->frameIn.height,
        prm->frameOut.width, prm->frameOut.height));

    //コピーを保存
    m_param = param;
    return err;
}

RGY_ERR RGAFilterResize::run_filter_rga(RGYFrameMpp *pInputFrame, RGYFrameMpp **ppOutputFrames, int *pOutputFrameNum, int *sync) {
    RGY_ERR sts = RGY_ERR_NONE;
    if (pInputFrame == nullptr) {
        return sts;
    }
    auto prm = dynamic_cast<RGYFilterParamResize*>(m_param.get());
    if (!prm) {
        AddMessage(RGY_LOG_ERROR, _T("Invalid parameter type.\n"));
        return RGY_ERR_INVALID_PARAM;
    }

    *pOutputFrameNum = 1;
    RGYFrameMpp *const pOutFrame = ppOutputFrames[0];

    if (pInputFrame->csp() != RGY_CSP_NV12) {
        AddMessage(RGY_LOG_ERROR, _T("Invalid input memory format: %s.\n"), RGY_CSP_NAMES[pInputFrame->csp()]);
        return RGY_ERR_INVALID_FORMAT;
    }

    if (pOutFrame->csp() != RGY_CSP_NV12) {
        AddMessage(RGY_LOG_ERROR, _T("Invalid output memory format: %s.\n"), RGY_CSP_NAMES[pOutFrame->csp()]);
        return RGY_ERR_INVALID_FORMAT;
    }

    rga_buffer_handle_t src_handle = getRGABufferHandle(pInputFrame);
    rga_buffer_handle_t dst_handle = getRGABufferHandle(pOutFrame);

    rga_buffer_t src = wrapbuffer_handle_t(src_handle, pInputFrame->width(), pInputFrame->height(),
        pInputFrame->pitch(0), mpp_frame_get_ver_stride(pInputFrame->mpp()), csp_rgy_to_rkrga(pInputFrame->csp()));
    rga_buffer_t dst = wrapbuffer_handle_t(dst_handle, pOutFrame->width(), pOutFrame->height(),
        pOutFrame->pitch(0), mpp_frame_get_ver_stride(pOutFrame->mpp()), csp_rgy_to_rkrga(pOutFrame->csp()));
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
    sts = err_to_rgy(imresize(src, dst, 0.0, 0.0, im2dinterp, 0, sync));
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

RGY_ERR RGAFilterResize::run_filter_iep(RGYFrameMpp *pInputFrame, RGYFrameMpp **ppOutputFrames, int *pOutputFrameNum, unique_event& sync) {
    return RGY_ERR_UNSUPPORTED;
}

int RGAFilterDeinterlaceIEP::maxAsyncCount = 6;

RGAFilterDeinterlaceIEP::RGAFilterDeinterlaceIEP() :
    RGAFilter(),
    m_iepCtx(std::unique_ptr<iep_com_ctx, decltype(&rockchip_iep2_api_release_ctx)>(nullptr, rockchip_iep2_api_release_ctx)),
    m_dilMode(IEP2_DIL_MODE_DISABLE),
    m_isTFF(true),
    m_mppBufInfo(),
    m_mppBufSrc(),
    m_mppBufDst(),
    m_mtxBufDst(),
    m_frameCountIn(0),
    m_frameCountOut(0),
    m_prevOutFrameDuration(0),
    m_threadWorker(),
    m_eventThreadStart(unique_event(nullptr, CloseEvent)),
    m_eventThreadFin(unique_event(nullptr, CloseEvent)),
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

    for (auto& buf : m_mppBufSrc) {
        buf.reset();
    }
    m_mppBufSrc.clear();
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
        if (m_iepCtx->ver < 2) {
            AddMessage(RGY_LOG_ERROR, _T("Requires iep ver 2 or later: detected version: %d.\n"), m_iepCtx->ver);
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
    
    switch (prm->mode) {
    case IEPDeinterlaceMode::NORMAL_I5:
        m_dilMode = (m_isTFF) ? IEP2_DIL_MODE_I5O1T : IEP2_DIL_MODE_I5O1B;
        m_mppBufSrc.resize(3+RGAFilterDeinterlaceIEP::maxAsyncCount);
        break;
    case IEPDeinterlaceMode::NORMAL_I2:
        m_dilMode = (m_isTFF) ? IEP2_DIL_MODE_I1O1T : IEP2_DIL_MODE_I1O1B;
        m_mppBufSrc.resize(1+RGAFilterDeinterlaceIEP::maxAsyncCount);
        break;
    case IEPDeinterlaceMode::BOB_I5:
        m_dilMode = IEP2_DIL_MODE_I5O2;
        m_mppBufSrc.resize(3+RGAFilterDeinterlaceIEP::maxAsyncCount);
        break;
    case IEPDeinterlaceMode::BOB_I2:
        m_dilMode = IEP2_DIL_MODE_I2O2;
        m_mppBufSrc.resize(1+RGAFilterDeinterlaceIEP::maxAsyncCount);
        break;
    default:
        AddMessage(RGY_LOG_ERROR, _T("Unknown deinterlace mode.\n"));
        return RGY_ERR_UNSUPPORTED;
    }
    AddMessage(RGY_LOG_DEBUG, _T("Selected deinterlace mode: %s %s.\n"), getDILModeName(m_dilMode), m_isTFF ? _T("TFF") : _T("BFF"));

    if ( prm->mode == IEPDeinterlaceMode::BOB_I5
      || prm->mode == IEPDeinterlaceMode::BOB_I2) {
        prm->baseFps *= 2;
    }
    prm->frameOut.picstruct = RGY_PICSTRUCT_FRAME;

    m_eventThreadStart = CreateEventUnique(nullptr, false, false, nullptr);
    AddMessage(RGY_LOG_DEBUG, _T("Create m_eventThreadStart.\n"));
    
    m_eventThreadFin = CreateEventUnique(nullptr, false, false, nullptr);
    AddMessage(RGY_LOG_DEBUG, _T("Create m_eventThreadFin.\n"));
    
    m_threadWorker = std::make_unique<std::thread>(&RGAFilterDeinterlaceIEP::workerThreadFunc, this);
    AddMessage(RGY_LOG_DEBUG, _T("Start worker thread.\n"));

    m_pathThrough &= ~(FILTER_PATHTHROUGH_PICSTRUCT | FILTER_PATHTHROUGH_TIMESTAMP | FILTER_PATHTHROUGH_FLAGS | FILTER_PATHTHROUGH_DATA);
    setFilterInfo(strsprintf(_T("deinterlace(%s)"),
        get_chr_from_value(list_iep_deinterlace, (int)prm->mode)));

    //コピーを保存
    m_param = param;
    return err;
}

RGY_ERR RGAFilterDeinterlaceIEP::setInputFrame(const RGYFrameMpp *pInputFrame) {
    getBufSrc(m_frameCountIn) = pInputFrame->createCopy();
    m_frameCountIn++;
    return RGY_ERR_NONE;
}

RGY_ERR RGAFilterDeinterlaceIEP::run_filter_rga(RGYFrameMpp *pInputFrame, RGYFrameMpp **ppOutputFrames, int *pOutputFrameNum, int *sync) {
    return RGY_ERR_UNSUPPORTED;
}

RGY_ERR RGAFilterDeinterlaceIEP::run_filter_iep(RGYFrameMpp *pInputFrame, RGYFrameMpp **ppOutputFrames, int *pOutputFrameNum, unique_event& sync) {
    RGY_ERR sts = RGY_ERR_NONE;
    auto prm = dynamic_cast<RGYFilterParamDeinterlaceIEP*>(m_param.get());
    if (!prm) {
        AddMessage(RGY_LOG_ERROR, _T("Invalid parameter type.\n"));
        return RGY_ERR_INVALID_PARAM;
    }

    if (pInputFrame) {
        if (pInputFrame->csp() != RGY_CSP_NV12) {
            AddMessage(RGY_LOG_ERROR, _T("Invalid input memory format: %s.\n"), RGY_CSP_NAMES[pInputFrame->csp()]);
            return RGY_ERR_INVALID_FORMAT;
        }

        if (ppOutputFrames[0]->csp() != RGY_CSP_NV12) {
            AddMessage(RGY_LOG_ERROR, _T("Invalid output memory format: %s.\n"), RGY_CSP_NAMES[ppOutputFrames[0]->csp()]);
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
    if (pInputFrame == nullptr) {
        if (m_frameCountIn == m_frameCountOut) {
            bool bufEmpty = false;
            {
                std::lock_guard<std::mutex> lock(m_mtxBufDst);
                bufEmpty = m_mppBufDst.empty();
            }
            while (!bufEmpty) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                {
                    std::lock_guard<std::mutex> lock(m_mtxBufDst);
                    bufEmpty = m_mppBufDst.empty();
                }
            }
            return RGY_ERR_NONE;
        }
    }

    sync = CreateEventUnique(nullptr, false, false, nullptr);

    // 前回のIEPの実行終了を待つ
    

    *pOutputFrameNum = 1;
    if (m_dilMode == IEP2_DIL_MODE_I5O2
     || m_dilMode == IEP2_DIL_MODE_I2O2) {
        *pOutputFrameNum = 2;
    }
    for (;;) {
        { // m_mppBufDstにアクセスするのでロック
            std::lock_guard<std::mutex> lock(m_mtxBufDst);
            const int bufsize = m_mppBufDst.size();
            if (bufsize < RGAFilterDeinterlaceIEP::maxAsyncCount) {
                for (int i = 0; i < *pOutputFrameNum; i++) {
                    if (ppOutputFrames[i] == nullptr) {
                        AddMessage(RGY_LOG_ERROR, _T("No output buffer for %d frame.\n"), i);
                        return RGY_ERR_UNSUPPORTED;
                    }
                    const auto timestamp = getBufSrc(m_frameCountOut)->timestamp();
                    ppOutputFrames[i]->setTimestamp(timestamp);
                    ppOutputFrames[i]->setInputFrameId(getBufSrc(m_frameCountOut)->inputFrameId());
                    ppOutputFrames[i]->setFlags(getBufSrc(m_frameCountOut)->flags());
                    ppOutputFrames[i]->setDataList(getBufSrc(m_frameCountOut)->dataList());
                    ppOutputFrames[i]->setPicstruct(RGY_PICSTRUCT_FRAME);
                    if (m_frameCountOut < m_frameCountIn-1) {
                        const auto duration = getBufSrc(m_frameCountOut+1)->timestamp() - getBufSrc(m_frameCountOut)->timestamp();
                        if (i > 0) { //bob化の場合
                            ppOutputFrames[i]->setTimestamp(timestamp + duration / 2);
                            ppOutputFrames[i]->setDuration(duration - ppOutputFrames[0]->duration());
                        } else {
                            ppOutputFrames[i]->setDuration(duration / 2);
                        }
                    } else {
                        ppOutputFrames[i]->setDuration(m_prevOutFrameDuration);
                        if (i > 0) { //bob化の場合
                            ppOutputFrames[i]->setTimestamp(timestamp + m_prevOutFrameDuration);
                        }
                    }
                    m_prevOutFrameDuration = ppOutputFrames[i]->duration();
                    // 処理スレッドに出力先を設定
                    m_mppBufDst.push_back(IepBufferOutInfo{ ppOutputFrames[i], (i == 0) ? sync.get() : nullptr, m_frameCountOut });
                }
                m_frameCountOut++;
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    // 処理スレッドに処理開始を通知
    SetEvent(m_eventThreadStart.get());
    return sts;
}

RGY_ERR RGAFilterDeinterlaceIEP::workerThreadFunc() {
    RGY_ERR sts = RGY_ERR_NONE;
    // メインスレッドからの開始通知待ち
    //SetEvent(m_eventThreadFin.get());
    WaitForSingleObject(m_eventThreadStart.get(), INFINITE);

    while (!m_threadAbort) {
        bool bufEmpty = false;
        int64_t frameCountOut = 0;
        HANDLE eventThreadFinSync = nullptr;
        std::vector<RGYFrameMpp*> dst;
        {
            std::lock_guard<std::mutex> lock(m_mtxBufDst);
            if (!(bufEmpty = m_mppBufDst.empty())) {
                dst.push_back(m_mppBufDst.front().mpp);
                eventThreadFinSync = m_mppBufDst.front().handle;
                frameCountOut = m_mppBufDst.front().outFrame;
                m_mppBufDst.pop_front();
                if (m_dilMode == IEP2_DIL_MODE_I5O2
                ||  m_dilMode == IEP2_DIL_MODE_I2O2) {
                    if (m_mppBufDst.empty()) {
                        AddMessage(RGY_LOG_ERROR, _T("No dst buf for second frame!\n"));
                        return RGY_ERR_UNKNOWN;
                    }
                    dst.push_back(m_mppBufDst.front().mpp);
                    m_mppBufDst.pop_front();
                }
            }
        }
        if (dst.size() > 0) {
            if (m_dilMode == IEP2_DIL_MODE_I5O2
            || m_dilMode == IEP2_DIL_MODE_I5O1T
            || m_dilMode == IEP2_DIL_MODE_I5O1B) {
                sts = runFilter(dst, { getBufSrc(std::max<int64_t>(0, frameCountOut-1)).get(),
                                       getBufSrc(frameCountOut + 0).get(),
                                       getBufSrc(std::min(frameCountOut + 1, m_frameCountIn-1)).get() });
                if (sts != RGY_ERR_NONE) {
                    return sts;
                }
            } else if (m_dilMode == IEP2_DIL_MODE_I2O2
                    || m_dilMode == IEP2_DIL_MODE_I1O1T
                    || m_dilMode == IEP2_DIL_MODE_I1O1B) {
                sts = runFilter(dst, { getBufSrc(frameCountOut).get() });
                if (sts != RGY_ERR_NONE) {
                    return sts;
                }
            } else {
                AddMessage(RGY_LOG_ERROR, _T("Unknown dil mode: %d.\n"), m_dilMode);
                return RGY_ERR_UNSUPPORTED;
            }

            // メインスレッドに終了を通知
            //SetEvent(m_eventThreadFin.get());
            // フレームに終了を通知
            SetEvent(eventThreadFinSync);
        }
        if (bufEmpty) {
            // メインスレッドからの開始通知待ち
            WaitForSingleObject(m_eventThreadStart.get(), INFINITE);
        }
    }
    AddMessage(RGY_LOG_DEBUG, _T("worker thread terminate.\n"));
    return sts;
}

RGY_ERR RGAFilterDeinterlaceIEP::setImage(RGYFrameMpp *frame, const IepCmd cmd) {
    auto fd = mpp_buffer_get_fd(mpp_frame_get_buffer(frame->mpp()));
    RK_S32 y_size = frame->pitch(0) * mpp_frame_get_ver_stride(frame->mpp());
    IepImg img;
    img.format = IEP_FORMAT_YCbCr_420_SP;
    img.act_w = frame->width();
    img.act_h = frame->height();
    img.vir_w = frame->pitch(0);
    img.vir_h = mpp_frame_get_ver_stride(frame->mpp());
    img.x_off = 0;
    img.y_off = 0;
    img.mem_addr = fd;
    img.uv_addr = fd + (y_size << 10);
    img.v_addr = fd + ((y_size + y_size / 4) << 10);
    AddMessage(RGY_LOG_TRACE, _T("SetImage %d: fd %d, buffermpp %p\n"), cmd, fd, frame->mpp());
    MPP_RET ret = m_iepCtx->ops->control(m_iepCtx->priv, cmd, &img);
    return err_to_rgy(ret);
}

RGY_ERR RGAFilterDeinterlaceIEP::runFilter(std::vector<RGYFrameMpp*> dst, const std::vector<RGYFrameMpp*> src) {
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
    params.param.com.width = src[0]->width();
    params.param.com.height = src[0]->height();
    params.param.com.hor_stride = src[0]->pitch(0);
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
