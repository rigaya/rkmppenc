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
#include <thread>
#include <mutex>
#include "rgy_version.h"
#include "rgy_err.h"
#include "rgy_util.h"
#include "rgy_event.h"
#include "rgy_filter.h"
#include "mpp_util.h"

#include "iep2_api.h"


class RGAFilter {
public:
    RGAFilter();
    virtual ~RGAFilter();
    virtual RGY_ERR init(shared_ptr<RGYFilterParam> param, shared_ptr<RGYLog> pPrintMes) = 0;
    RGY_ERR filter_rga(RGYFrameMpp *pInputFrame, RGYFrameMpp **ppOutputFrames, int *pOutputFrameNum, int *sync);
    RGY_ERR filter_iep(RGYFrameMpp *pInputFrame, RGYFrameMpp **ppOutputFrames, int *pOutputFrameNum, unique_event& sync);
    virtual void close() = 0;
    const tstring& name() const {
        return m_name;
    }
    const tstring& GetInputMessage() const {
        return m_infoStr;
    }
    const RGYFilterParam *GetFilterParam() const {
        return m_param.get();
    }
protected:
    virtual RGY_ERR run_filter_rga(RGYFrameMpp *pInputFrame, RGYFrameMpp **ppOutputFrames, int *pOutputFrameNum, int *sync) {
        return RGY_ERR_UNSUPPORTED;
    }
    virtual RGY_ERR run_filter_iep(RGYFrameMpp *pInputFrame, RGYFrameMpp **ppOutputFrames, int *pOutputFrameNum, unique_event& sync) {
        return RGY_ERR_UNSUPPORTED;
    }

    rga_buffer_handle_t getRGABufferHandle(RGYFrameMpp *frame);

    void AddMessage(RGYLogLevel log_level, const tstring &str) {
        if (m_pLog == nullptr || log_level < m_pLog->getLogLevel(RGY_LOGT_VPP)) {
            return;
        }
        auto lines = split(str, _T("\n"));
        for (const auto &line : lines) {
            if (line[0] != _T('\0')) {
                m_pLog->write(log_level, RGY_LOGT_VPP, (m_name + _T(": ") + line + _T("\n")).c_str());
            }
        }
    }
    void AddMessage(RGYLogLevel log_level, const TCHAR *format, ...) {
        if (m_pLog == nullptr || log_level < m_pLog->getLogLevel(RGY_LOGT_VPP)) {
            return;
        }

        va_list args;
        va_start(args, format);
        int len = _vsctprintf(format, args) + 1; // _vscprintf doesn't count terminating '\0'
        tstring buffer;
        buffer.resize(len, _T('\0'));
        _vstprintf_s(&buffer[0], len, format, args);
        va_end(args);
        AddMessage(log_level, buffer);
    }
    void setFilterInfo(const tstring &info) {
        m_infoStr = info;
        AddMessage(RGY_LOG_DEBUG, info);
    }

    tstring m_name;
    tstring m_infoStr;
    shared_ptr<RGYLog> m_pLog;  //ログ出力
    shared_ptr<RGYFilterParam> m_param;
    FILTER_PATHTHROUGH_FRAMEINFO m_pathThrough;
};
class RGAFilterCrop : public RGAFilter {
public:
    RGAFilterCrop();
    virtual ~RGAFilterCrop();
    virtual RGY_ERR init(shared_ptr<RGYFilterParam> param, shared_ptr<RGYLog> pPrintMes) override;
protected:
    virtual RGY_ERR run_filter_rga(RGYFrameMpp *pInputFrame, RGYFrameMpp **ppOutputFrames, int *pOutputFrameNum, int *sync) override;
    virtual RGY_ERR run_filter_iep(RGYFrameMpp *pInputFrame, RGYFrameMpp **ppOutputFrames, int *pOutputFrameNum, unique_event& sync) override;
    RGY_ERR checkParams(const RGYFilterParam *param);  
    virtual void close() override;
};

class RGAFilterCspConv : public RGAFilter {
public:
    RGAFilterCspConv();
    virtual ~RGAFilterCspConv();
    virtual RGY_ERR init(shared_ptr<RGYFilterParam> param, shared_ptr<RGYLog> pPrintMes) override;
protected:
    virtual RGY_ERR run_filter_rga(RGYFrameMpp *pInputFrame, RGYFrameMpp **ppOutputFrames, int *pOutputFrameNum, int *sync) override;
    virtual RGY_ERR run_filter_iep(RGYFrameMpp *pInputFrame, RGYFrameMpp **ppOutputFrames, int *pOutputFrameNum, unique_event& sync) override;
    RGY_ERR checkParams(const RGYFilterParam *param);  
    virtual void close() override;

    int m_cvt_mode;
};

class RGAFilterResize : public RGAFilter {
public:
    RGAFilterResize();
    virtual ~RGAFilterResize();
    virtual RGY_ERR init(shared_ptr<RGYFilterParam> param, shared_ptr<RGYLog> pPrintMes) override;
protected:
    virtual RGY_ERR run_filter_rga(RGYFrameMpp *pInputFrame, RGYFrameMpp **ppOutputFrames, int *pOutputFrameNum, int *sync) override;
    virtual RGY_ERR run_filter_iep(RGYFrameMpp *pInputFrame, RGYFrameMpp **ppOutputFrames, int *pOutputFrameNum, unique_event& sync) override;
    RGY_ERR checkParams(const RGYFilterParam *param);  
    virtual void close() override;
};

class RGYFilterParamDeinterlaceIEP : public RGYFilterParam {
public:
    RGY_PICSTRUCT picstruct;
    IEPDeinterlaceMode mode;
    RGYFilterParamDeinterlaceIEP() : picstruct(RGY_PICSTRUCT_AUTO), mode(IEPDeinterlaceMode::DISABLED) {};
    virtual ~RGYFilterParamDeinterlaceIEP() {};
};

class RGYConvertCSP;

class RGAFilterDeinterlaceIEP : public RGAFilter {
public:
    RGAFilterDeinterlaceIEP();
    virtual ~RGAFilterDeinterlaceIEP();
    virtual RGY_ERR init(shared_ptr<RGYFilterParam> param, shared_ptr<RGYLog> pPrintMes) override;
protected:
    struct IepBufferOutInfo {
        RGYFrameMpp *mpp;
        HANDLE handle;
        int64_t outFrame;
    };
    virtual RGY_ERR run_filter_rga(RGYFrameMpp *pInputFrame, RGYFrameMpp **ppOutputFrames, int *pOutputFrameNum, int *sync) override;
    virtual RGY_ERR run_filter_iep(RGYFrameMpp *pInputFrame, RGYFrameMpp **ppOutputFrames, int *pOutputFrameNum, unique_event& sync) override;
    RGY_ERR checkParams(const RGYFilterParam *param);
    RGY_ERR setInputFrame(const RGYFrameMpp *pInputFrame);
    RGY_ERR setImage(RGYFrameMpp *frame, const IepCmd cmd);
    RGY_ERR runFilter(std::vector<RGYFrameMpp*> dst, const std::vector<RGYFrameMpp*> src);
    RGY_ERR workerThreadFunc();
    virtual void close() override;

    std::unique_ptr<RGYFrameMpp>& getBufSrc(const int64_t idx) { return m_mppBufSrc[idx % m_mppBufSrc.size()]; }
 
    std::unique_ptr<iep_com_ctx, decltype(&put_iep_ctx)> m_iepCtx;
    IEPDeinterlaceMode m_mode;
    RGYFrameInfo m_mppBufInfo;
    std::vector<std::unique_ptr<RGYFrameMpp>> m_mppBufSrc;
    std::deque<IepBufferOutInfo> m_mppBufDst;
    std::mutex m_mtxBufDst;
    int64_t m_frameCountIn;
    int64_t m_frameCountOut;
    int64_t m_prevOutFrameDuration;

    std::unique_ptr<std::thread> m_threadWorker;
    unique_event m_eventThreadStart; // main -> worker
    unique_event m_eventThreadFin;   // worker -> main
    bool m_threadAbort;
};
