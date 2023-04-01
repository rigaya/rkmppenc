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

RGY_ERR RGAFilter::filter(RGYFrameInfo *pInputFrame, RGYFrameInfo **ppOutputFrames, int *pOutputFrameNum) {
    RGYOpenCLQueue dummy;
    return RGYFilter::filter(pInputFrame, ppOutputFrames, pOutputFrameNum, dummy, {}, nullptr);
}
RGY_ERR RGAFilter::run_filter(const RGYFrameInfo *pInputFrame, RGYFrameInfo **ppOutputFrames, int *pOutputFrameNum, RGYOpenCLQueue &queue, const std::vector<RGYOpenCLEvent> &wait_events, RGYOpenCLEvent *event) {
    return run_filter_rga(pInputFrame, ppOutputFrames, pOutputFrameNum);
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

RGY_ERR RGAFilterResize::run_filter_rga(const RGYFrameInfo *pInputFrame, RGYFrameInfo **ppOutputFrames, int *pOutputFrameNum) {
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

    const auto im2dinterp = interp_rgy_to_rga(prm->interp);
    sts = err_to_rgy(imresize_t(src, dst, 0.0, 0.0, im2dinterp, 1));
    if (sts != RGY_ERR_NONE) {
        AddMessage(RGY_LOG_ERROR, _T("Failed to run imresize: %s"), get_err_mes(sts));
        return sts;
    }
    if (src_handle) {
        releasebuffer_handle(src_handle);
    }
    if (dst_handle) {
        releasebuffer_handle(dst_handle);
    }
    return sts;
}
