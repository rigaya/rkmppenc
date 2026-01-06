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
#ifndef __MPP_PIPELINE_H__
#define __MPP_PIPELINE_H__

#include <thread>
#include <future>
#include <atomic>
#include <deque>
#include <set>
#include <unordered_map>

#include "rgy_version.h"
#include "rgy_err.h"
#include "rgy_util.h"
#include "rgy_log.h"
#include "rgy_input.h"
#include "rgy_input_avcodec.h"
#include "rgy_input_sm.h"
#include "rgy_output.h"
#include "rgy_output_avcodec.h"
#include "rgy_opencl.h"
#include "rgy_filter.h"
#include "rgy_filter_ssim.h"
#include "rgy_thread.h"
#include "rgy_timecode.h"
#include "rgy_device.h"
#include "mpp_device.h"
#include "mpp_param.h"
#include "mpp_filter.h"
#include "rk_mpi.h"

static const int RGY_WAIT_INTERVAL = 60000;

struct MPPContext {
    MppCtx ctx;
    MppApi *mpi;

    MPPContext();
    ~MPPContext();
    RGY_ERR create();
    RGY_ERR init(MppCtxType type, MppCodingType codectype);
};

enum RGYRunState {
    RGY_STATE_STOPPED,
    RGY_STATE_RUNNING,
    RGY_STATE_ERROR,
    RGY_STATE_ABORT,
    RGY_STATE_EOF
};
struct VppVilterBlock {
    VppFilterType type;
    std::vector<std::unique_ptr<RGYFilter>> vppcl;
    std::vector<std::unique_ptr<RGAFilter>> vpprga;

    VppVilterBlock(std::vector<std::unique_ptr<RGYFilter>>& filter) : type(VppFilterType::FILTER_OPENCL), vppcl(std::move(filter)), vpprga() {};
    VppVilterBlock(std::vector<std::unique_ptr<RGAFilter>>& filter, VppFilterType type_) : type(type_), vppcl(), vpprga(std::move(filter)) {};
};

enum class PipelineTaskOutputType {
    UNKNOWN,
    SURFACE,
    BITSTREAM
};

enum class PipelineTaskSurfaceType {
    UNKNOWN,
    CL,
    MPP
};

class PipelineTaskSurface {
private:
    RGYFrame *surf;
    std::atomic<int> *ref;
public:
    PipelineTaskSurface() : surf(nullptr), ref(nullptr) {};
    PipelineTaskSurface(std::pair<RGYFrame *, std::atomic<int> *> surf_) : PipelineTaskSurface(surf_.first, surf_.second) {};
    PipelineTaskSurface(RGYFrame *surf_, std::atomic<int> *ref_) : surf(surf_), ref(ref_) { if (surf) (*ref)++; };
    PipelineTaskSurface(const PipelineTaskSurface& obj) : surf(obj.surf), ref(obj.ref) { if (surf) (*ref)++; }
    PipelineTaskSurface &operator=(const PipelineTaskSurface &obj) {
        if (this != &obj) { // 自身の代入チェック
            surf = obj.surf;
            ref = obj.ref;
            if (surf) (*ref)++;
        }
        return *this;
    }
    ~PipelineTaskSurface() { reset(); }
    void reset() { if (surf) (*ref)--; surf = nullptr; ref = nullptr; }
    bool operator !() const {
        return frame() == nullptr;
    }
    bool operator !=(const PipelineTaskSurface& obj) const { return frame() != obj.frame(); }
    bool operator ==(const PipelineTaskSurface& obj) const { return frame() == obj.frame(); }
    bool operator !=(std::nullptr_t) const { return frame() != nullptr; }
    bool operator ==(std::nullptr_t) const { return frame() == nullptr; }
    const RGYFrameMpp *mpp() const { return dynamic_cast<const RGYFrameMpp*>(surf); }
    RGYFrameMpp *mpp() { return dynamic_cast<RGYFrameMpp*>(surf); }
    const RGYCLFrame *cl() const { return dynamic_cast<const RGYCLFrame*>(surf); }
    RGYCLFrame *cl() { return dynamic_cast<RGYCLFrame*>(surf); }
    const RGYFrame *frame() const { return surf; }
    RGYFrame *frame() { return surf; }
};

// アプリ用の独自参照カウンタと組み合わせたクラス
class PipelineTaskSurfaces {
private:
    class PipelineTaskSurfacesPair {
    private:
        std::unique_ptr<RGYFrame> surf_;
        std::atomic<int> ref;
    public:
        PipelineTaskSurfacesPair(std::unique_ptr<RGYFrame> s) : surf_(std::move(s)), ref(0) {};
        
        bool isFree() const { return ref == 0; } // 使用されていないフレームかを返す
        PipelineTaskSurface getRef() { return PipelineTaskSurface(surf_.get(), &ref); };
        const RGYFrame *surf() const { return surf_.get(); }
        RGYFrame *surf() { return surf_.get(); }
        PipelineTaskSurfaceType type() const {
            if (!surf_) return PipelineTaskSurfaceType::UNKNOWN;
            if (dynamic_cast<const RGYCLFrame*>(surf_.get())) return PipelineTaskSurfaceType::CL;
            if (dynamic_cast<const RGYFrameMpp*>(surf_.get())) return PipelineTaskSurfaceType::MPP;
            return PipelineTaskSurfaceType::UNKNOWN;
        }
    };
    std::vector<std::unique_ptr<PipelineTaskSurfacesPair>> m_surfaces; // フレームと参照カウンタ
public:
    PipelineTaskSurfaces() : m_surfaces() {};
    ~PipelineTaskSurfaces() {}

    void clear() {
        m_surfaces.clear();
    }
    void setSurfaces(std::vector<std::unique_ptr<RGYFrame>>& surfs) {
        clear();
        m_surfaces.resize(surfs.size());
        for (size_t i = 0; i < m_surfaces.size(); i++) {
            m_surfaces[i] = std::make_unique<PipelineTaskSurfacesPair>(std::move(surfs[i]));
        }
    }
    PipelineTaskSurface addSurface(std::unique_ptr<RGYFrame>& surf) {
        deleteFreedSurface();
        m_surfaces.push_back(std::move(std::unique_ptr<PipelineTaskSurfacesPair>(new PipelineTaskSurfacesPair(std::move(surf)))));
        return m_surfaces.back()->getRef();
    }
    PipelineTaskSurface addSurface(std::unique_ptr<RGYFrameMpp>& surf) {
        deleteFreedSurface();
        m_surfaces.push_back(std::move(std::unique_ptr<PipelineTaskSurfacesPair>(new PipelineTaskSurfacesPair(std::move(surf)))));
        return m_surfaces.back()->getRef();
    }

    PipelineTaskSurface getFreeSurf() {
        for (auto& s : m_surfaces) {
            if (s->isFree()) {
                return s->getRef();
            }
        }
        return PipelineTaskSurface();
    }
    PipelineTaskSurface get(RGYFrame *surf) {
        auto s = findSurf(surf);
        if (s != nullptr) {
            return s->getRef();
        }
        return PipelineTaskSurface();
    }
    size_t bufCount() const { return m_surfaces.size(); }

    bool isAllFree() const {
        for (const auto& s : m_surfaces) {
            if (!s->isFree()) {
                return false;
            }
        }
        return true;
    }
    PipelineTaskSurfaceType type() const {
        if (m_surfaces.size() == 0) return PipelineTaskSurfaceType::UNKNOWN;
        return m_surfaces.front()->type();
    }
protected:
    void deleteFreedSurface() {
        for (auto it = m_surfaces.begin(); it != m_surfaces.end();) {
            if ((*it)->isFree()) {
                it = m_surfaces.erase(it);
            } else {
                it++;
            }
        }
    }

    PipelineTaskSurfacesPair *findSurf(RGYFrame *surf) {
        for (auto& s : m_surfaces) {
            if (s->surf() == surf) {
                return s.get();
            }
        }
        return nullptr;
    }
};

class PipelineTaskOutputDataCustom {
    int type;
public:
    PipelineTaskOutputDataCustom() {};
    virtual ~PipelineTaskOutputDataCustom() {};
};

class PipelineTaskOutputDataCheckPts : public PipelineTaskOutputDataCustom {
private:
    int64_t timestamp;
public:
    PipelineTaskOutputDataCheckPts() : PipelineTaskOutputDataCustom() {};
    PipelineTaskOutputDataCheckPts(int64_t timestampOverride) : PipelineTaskOutputDataCustom(), timestamp(timestampOverride) {};
    virtual ~PipelineTaskOutputDataCheckPts() {};
    int64_t timestampOverride() const { return timestamp; }
};

class PipelineTaskOutput {
protected:
    PipelineTaskOutputType m_type;
    std::unique_ptr<PipelineTaskOutputDataCustom> m_customData;
public:
    PipelineTaskOutput() : m_type(PipelineTaskOutputType::UNKNOWN), m_customData() {};
    PipelineTaskOutput(PipelineTaskOutputType type) : m_type(type), m_customData() {};
    PipelineTaskOutput(PipelineTaskOutputType type, std::unique_ptr<PipelineTaskOutputDataCustom>& customData) : m_type(type), m_customData(std::move(customData)) {};
    RGY_ERR waitsync(uint32_t wait = RGY_WAIT_INTERVAL) {
        return RGY_ERR_NONE;
    }
    virtual void depend_clear() {};
    PipelineTaskOutputType type() const { return m_type; }
    const PipelineTaskOutputDataCustom *customdata() const { return m_customData.get(); }
    virtual RGY_ERR write([[maybe_unused]] RGYOutput *writer, [[maybe_unused]] RGYOpenCLQueue *clqueue, [[maybe_unused]] RGYFilterSsim *videoQualityMetric) {
        return RGY_ERR_UNSUPPORTED;
    }
    virtual ~PipelineTaskOutput() {};
};

class PipelineTaskOutputSurf : public PipelineTaskOutput {
protected:
    PipelineTaskSurface m_surf;
    std::unique_ptr<PipelineTaskOutput> m_dependencyFrame;
    std::vector<RGYOpenCLEvent> m_clevents;
    unique_event m_event;
    int m_release_fence_rga;
public:
    PipelineTaskOutputSurf(PipelineTaskSurface surf) :
        PipelineTaskOutput(PipelineTaskOutputType::SURFACE), m_surf(surf), m_dependencyFrame(), m_clevents(), m_event(unique_event(nullptr, CloseEvent)), m_release_fence_rga(0) {
    };
    PipelineTaskOutputSurf(PipelineTaskSurface surf, std::unique_ptr<PipelineTaskOutputDataCustom>& customData) :
        PipelineTaskOutput(PipelineTaskOutputType::SURFACE, customData), m_surf(surf), m_dependencyFrame(), m_clevents(), m_event(unique_event(nullptr, CloseEvent)), m_release_fence_rga(0) {
    };
    PipelineTaskOutputSurf(PipelineTaskSurface surf, std::unique_ptr<PipelineTaskOutput>& dependencyFrame) :
        PipelineTaskOutput(PipelineTaskOutputType::SURFACE),
        m_surf(surf), m_dependencyFrame(std::move(dependencyFrame)), m_clevents(), m_event(unique_event(nullptr, CloseEvent)), m_release_fence_rga(0) {
    };
    PipelineTaskOutputSurf(PipelineTaskSurface surf, std::unique_ptr<PipelineTaskOutput>& dependencyFrame, RGYOpenCLEvent& clevent) :
        PipelineTaskOutput(PipelineTaskOutputType::SURFACE),
        m_surf(surf), m_dependencyFrame(std::move(dependencyFrame)), m_clevents(), m_event(unique_event(nullptr, CloseEvent)), m_release_fence_rga(0) {
        m_clevents.push_back(clevent);
    };
    virtual ~PipelineTaskOutputSurf() {
        depend_clear();
        m_surf.reset();
    };

    PipelineTaskSurface& surf() { return m_surf; }

    void addEvent(unique_event& event) {
        m_event = std::move(event);
    }

    void addClEvent(RGYOpenCLEvent& clevent) {
        m_clevents.push_back(clevent);
    }

    void setRGASync(int sync) {
        m_release_fence_rga = sync;
    }

    virtual void depend_clear() override {
        RGYOpenCLEvent::wait(m_clevents);
        m_clevents.clear();
        m_dependencyFrame.reset();
        if (m_event) {
            WaitForSingleObject(m_event.get(), INFINITE);
            m_event.reset();
        }
        if (m_release_fence_rga) {
            imsync(m_release_fence_rga);
            m_release_fence_rga = 0;
        }
    }

    RGY_ERR writeSys(RGYOutput *writer) {
        auto err = writer->WriteNextFrame(m_surf.frame());
        return err;
    }

    RGY_ERR writeCL(RGYOutput *writer, RGYOpenCLQueue *clqueue) {
        if (clqueue == nullptr) {
            return RGY_ERR_NULL_PTR;
        }
        auto clframe = m_surf.cl();
        auto err = clframe->queueMapBuffer(*clqueue, CL_MAP_READ); // CPUが読み込むためにmapする
        if (err != RGY_ERR_NONE) {
            return err;
        }
        clframe->mapWait();
        err = writer->WriteNextFrame(clframe->mappedHost());
        clframe->unmapBuffer();
        return err;
    }

    virtual RGY_ERR write([[maybe_unused]] RGYOutput *writer, [[maybe_unused]] RGYOpenCLQueue *clqueue, [[maybe_unused]] RGYFilterSsim *videoQualityMetric) override {
        if (!writer || writer->getOutType() == OUT_TYPE_NONE) {
            return RGY_ERR_NOT_INITIALIZED;
        }
        if (writer->getOutType() != OUT_TYPE_SURFACE) {
            return RGY_ERR_INVALID_OPERATION;
        }
        auto err = (m_surf.cl() != nullptr) ? writeCL(writer, clqueue) : writeSys(writer);
        return err;
    }
};

class PipelineTaskOutputBitstream : public PipelineTaskOutput {
protected:
    std::shared_ptr<RGYBitstream> m_bs;
public:
    PipelineTaskOutputBitstream(std::shared_ptr<RGYBitstream> bs) : PipelineTaskOutput(PipelineTaskOutputType::BITSTREAM), m_bs(bs) {};
    virtual ~PipelineTaskOutputBitstream() {};

    std::shared_ptr<RGYBitstream>& bitstream() { return m_bs; }

    virtual RGY_ERR write([[maybe_unused]] RGYOutput *writer, [[maybe_unused]] RGYOpenCLQueue *clqueue, [[maybe_unused]] RGYFilterSsim *videoQualityMetric) override {
        if (!writer || writer->getOutType() == OUT_TYPE_NONE) {
            return RGY_ERR_NOT_INITIALIZED;
        }
        if (writer->getOutType() != OUT_TYPE_BITSTREAM) {
            return RGY_ERR_INVALID_OPERATION;
        }
        if (videoQualityMetric) {
            if (!videoQualityMetric->decodeStarted()) {
                videoQualityMetric->initDecode(m_bs.get());
            }
            videoQualityMetric->addBitstream(m_bs.get());
        }
        return writer->WriteNextFrame(m_bs.get());
    }
};

enum class PipelineTaskType {
    UNKNOWN,
    MPPDEC,
    MPPIEP,
    MPPVPP,
    MPPENC,
    INPUT,
    INPUTCL,
    CHECKPTS,
    TRIM,
    AUDIO,
    OUTPUTRAW,
    OPENCL,
    VIDEOMETRIC,
};

static const TCHAR *getPipelineTaskTypeName(PipelineTaskType type) {
    switch (type) {
    case PipelineTaskType::MPPIEP:      return _T("MPPIEP");
    case PipelineTaskType::MPPVPP:      return _T("MPPVPP");
    case PipelineTaskType::MPPDEC:      return _T("MPPDEC");
    case PipelineTaskType::MPPENC:      return _T("MPPENC");
    case PipelineTaskType::INPUT:       return _T("INPUT");
    case PipelineTaskType::INPUTCL:     return _T("INPUTCL");
    case PipelineTaskType::CHECKPTS:    return _T("CHECKPTS");
    case PipelineTaskType::TRIM:        return _T("TRIM");
    case PipelineTaskType::OPENCL:      return _T("OPENCL");
    case PipelineTaskType::AUDIO:       return _T("AUDIO");
    case PipelineTaskType::VIDEOMETRIC: return _T("VIDEOMETRIC");
    case PipelineTaskType::OUTPUTRAW:   return _T("OUTRAW");
    default: return _T("UNKNOWN");
    }
}

// Alllocするときの優先度 値が高い方が優先
static const int getPipelineTaskAllocPriority(PipelineTaskType type) {
    switch (type) {
    case PipelineTaskType::MPPENC:    return 4;
    case PipelineTaskType::MPPDEC:    return 3;
    case PipelineTaskType::MPPIEP:    return 2;
    case PipelineTaskType::MPPVPP:    return 1;
    case PipelineTaskType::INPUT:
    case PipelineTaskType::INPUTCL:
    case PipelineTaskType::CHECKPTS:
    case PipelineTaskType::TRIM:
    case PipelineTaskType::OPENCL:
    case PipelineTaskType::AUDIO:
    case PipelineTaskType::OUTPUTRAW:
    case PipelineTaskType::VIDEOMETRIC:
    default: return 0;
    }
}

class PipelineTask {
protected:
    PipelineTaskType m_type;
    std::deque<std::unique_ptr<PipelineTaskOutput>> m_outQeueue;
    PipelineTaskSurfaces m_workSurfs;
    int m_inFrames;
    int m_outFrames;
    int m_outMaxQueueSize;
    MppBufferGroup m_frameGrp;
    std::shared_ptr<RGYLog> m_log;
public:
    PipelineTask() : m_type(PipelineTaskType::UNKNOWN), m_outQeueue(), m_workSurfs(), m_inFrames(0), m_outFrames(0), m_outMaxQueueSize(0), m_log() {};
    PipelineTask(PipelineTaskType type, int outMaxQueueSize, std::shared_ptr<RGYLog> log) :
        m_type(type), m_outQeueue(), m_workSurfs(), m_inFrames(0), m_outFrames(0), m_outMaxQueueSize(outMaxQueueSize), m_frameGrp(nullptr), m_log(log) {
    };
    virtual ~PipelineTask() {
        m_outQeueue.clear();
        m_workSurfs.clear();
        if (m_frameGrp) {
            mpp_buffer_group_put(m_frameGrp);
            m_frameGrp = nullptr;
        }
    }
    virtual bool isPassThrough() const { return false; }
    virtual tstring print() const { return getPipelineTaskTypeName(m_type); }
    virtual std::optional<std::pair<RGYFrameInfo, int>> requiredSurfIn() = 0;
    virtual std::optional<std::pair<RGYFrameInfo, int>> requiredSurfOut() = 0;
    virtual RGY_ERR sendFrame(std::unique_ptr<PipelineTaskOutput>& frame) = 0;
    virtual bool abort() { return false; }; // 中断指示を受け取ったらtrueを返す
    virtual RGY_ERR getOutputFrameInfo(RGYFrameInfo& info) { info = RGYFrameInfo(); return RGY_ERR_NONE; }
    virtual std::vector<std::unique_ptr<PipelineTaskOutput>> getOutput(const bool sync) {
        std::vector<std::unique_ptr<PipelineTaskOutput>> output;
        while ((int)m_outQeueue.size() > m_outMaxQueueSize) {
            auto out = std::move(m_outQeueue.front());
            m_outQeueue.pop_front();
            if (sync) {
                out->waitsync();
            }
            out->depend_clear();
            m_outFrames++;
            output.push_back(std::move(out));
        }
        return output;
    }
    bool isAMFTask() const { return isAMFTask(m_type); }
    bool isAMFTask(const PipelineTaskType task) const {
        return task == PipelineTaskType::MPPDEC
            || task == PipelineTaskType::MPPVPP
            || task == PipelineTaskType::MPPENC;
    }
    // mfx関連とそうでないtaskのやり取りでロックが必要
    bool requireSync(const PipelineTaskType nextTaskType) const {
        return true;
    }
    int workSurfacesAllocPriority() const {
        return getPipelineTaskAllocPriority(m_type);
    }
    size_t workSurfacesCount() const {
        return m_workSurfs.bufCount();
    }

    void PrintMes(RGYLogLevel log_level, const TCHAR *format, ...) {
        if (m_log.get() == nullptr) {
            if (log_level <= RGY_LOG_INFO) {
                return;
            }
        } else if (log_level < m_log->getLogLevel(RGY_LOGT_CORE)) {
            return;
        }

        va_list args;
        va_start(args, format);

        int len = _vsctprintf(format, args) + 1; // _vscprintf doesn't count terminating '\0'
        vector<TCHAR> buffer(len, 0);
        _vstprintf_s(buffer.data(), len, format, args);
        va_end(args);

        tstring mes = getPipelineTaskTypeName(m_type) + tstring(_T(": ")) + buffer.data();

        if (m_log.get() != nullptr) {
            m_log->write(log_level, RGY_LOGT_CORE, mes.c_str());
        } else {
            _ftprintf(stderr, _T("%s"), mes.c_str());
        }
    }
protected:
    RGY_ERR workSurfacesClear() {
        if (m_outQeueue.size() != 0) {
            return RGY_ERR_UNSUPPORTED;
        }
        if (!m_workSurfs.isAllFree()) {
            return RGY_ERR_UNSUPPORTED;
        }
        return RGY_ERR_NONE;
    }
public:
    RGY_ERR workSurfacesAllocCL(const int numFrames, const RGYFrameInfo &frame, RGYOpenCLContext *cl) {
        auto sts = workSurfacesClear();
        if (sts != RGY_ERR_NONE) {
            PrintMes(RGY_LOG_ERROR, _T("allocWorkSurfaces:   Failed to clear old surfaces: %s.\n"), get_err_mes(sts));
            return sts;
        }
        PrintMes(RGY_LOG_DEBUG, _T("allocWorkSurfaces:   cleared old surfaces: %s.\n"), get_err_mes(sts));

        // OpenCLフレームの確保
        std::vector<std::unique_ptr<RGYFrame>> frames(numFrames);
        for (size_t i = 0; i < frames.size(); i++) {
            //CPUとのやり取りが効率化できるよう、CL_MEM_ALLOC_HOST_PTR を指定する
            //これでmap/unmapで可能な場合コピーが発生しない
            frames[i] = cl->createFrameBuffer(frame, CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR);
        }
        PrintMes(RGY_LOG_DEBUG, _T("allocWorkSurfaces:   allocated %d frames.\n"), numFrames);
        m_workSurfs.setSurfaces(frames);
        return RGY_ERR_NONE;
    }
#if 0
    RGY_ERR workSurfacesAllocMpp(const int numFrames, const RGYFrameInfo &frame) {
        auto sts = workSurfacesClear();
        if (sts != RGY_ERR_NONE) {
            PrintMes(RGY_LOG_ERROR, _T("allocWorkSurfaces:   Failed to clear old surfaces: %s.\n"), get_err_mes(sts));
            return sts;
        }
        PrintMes(RGY_LOG_DEBUG, _T("allocWorkSurfaces:   cleared old surfaces: %s.\n"), get_err_mes(sts));

        if (!m_frameGrp) {
            sts = err_to_rgy(mpp_buffer_group_get_internal(&m_frameGrp, MPP_BUFFER_TYPE_DRM));
            if (sts != RGY_ERR_NONE) {
                PrintMes(RGY_LOG_ERROR, _T("failed to get mpp buffer group : %s\n"), get_err_mes(sts));
                return sts;
            }
        }

        std::vector<std::unique_ptr<RGYFrame>> frames(numFrames);
        for (size_t i = 0; i < frames.size(); i++) {
            frames[i] = std::make_unique<RGYFrameMpp>(frame, m_frameGrp);
            if (frames[i]->isempty()) {
                return sts;
            }
        }
        m_workSurfs.setSurfaces(frames);
        return RGY_ERR_NONE;
    }
#endif
    PipelineTaskSurfaceType workSurfaceType() const {
        if (m_workSurfs.bufCount() == 0) {
            return PipelineTaskSurfaceType::UNKNOWN;
        }
        return m_workSurfs.type();
    }
    // 使用中でないフレームを探してきて、参照カウンタを加算したものを返す
    // 破棄時にアプリ側の参照カウンタを減算するようにshared_ptrで設定してある
    PipelineTaskSurface getWorkSurf() {
        if (m_workSurfs.bufCount() == 0) {
            PrintMes(RGY_LOG_ERROR, _T("getWorkSurf:   No buffer allocated!\n"));
            return PipelineTaskSurface();
        }
        for (int i = 0; i < RGY_WAIT_INTERVAL; i++) {
            PipelineTaskSurface s = m_workSurfs.getFreeSurf();
            if (s != nullptr) {
                return s;
            }
            sleep_hybrid(i);
        }
        PrintMes(RGY_LOG_ERROR, _T("getWorkSurf:   Failed to get work surface, all %d frames used.\n"), m_workSurfs.bufCount());
        return PipelineTaskSurface();
    }

    std::unique_ptr<RGYFrameMpp> getNewWorkSurfMpp(const RGYFrameInfo &frame, const int x_stride = 0, const int y_stride = 0) {
        if (!m_frameGrp) {
            auto sts = err_to_rgy(mpp_buffer_group_get_internal(&m_frameGrp, MPP_BUFFER_TYPE_DRM));
            if (sts != RGY_ERR_NONE) {
                PrintMes(RGY_LOG_ERROR, _T("failed to get mpp buffer group : %s\n"), get_err_mes(sts));
                return std::make_unique<RGYFrameMpp>();
            }
            mpp_buffer_group_limit_config(m_frameGrp, mpp_frame_size(frame, x_stride, y_stride), 32);
        }
        return std::make_unique<RGYFrameMpp>(frame, m_frameGrp, x_stride, y_stride);
    }

    void setOutputMaxQueueSize(int size) { m_outMaxQueueSize = size; }

    PipelineTaskType taskType() const { return m_type; }
    int inputFrames() const { return m_inFrames; }
    int outputFrames() const { return m_outFrames; }
    int outputMaxQueueSize() const { return m_outMaxQueueSize; }
};

class PipelineTaskInput : public PipelineTask {
    RGYInput *m_input;
    std::shared_ptr<RGYOpenCLContext> m_cl;
    bool m_abort;
public:
    PipelineTaskInput(int outMaxQueueSize, RGYInput *input, std::shared_ptr<RGYOpenCLContext> cl, std::shared_ptr<RGYLog> log)
        : PipelineTask(PipelineTaskType::INPUT, outMaxQueueSize, log), m_input(input), m_cl(cl), m_abort(false) {

    };
    virtual ~PipelineTaskInput() {};
    virtual std::optional<std::pair<RGYFrameInfo, int>> requiredSurfIn() override { return std::nullopt; };
    virtual std::optional<std::pair<RGYFrameInfo, int>> requiredSurfOut() override {
        const auto inputFrameInfo = m_input->GetInputFrameInfo();
        RGYFrameInfo info(inputFrameInfo.srcWidth, inputFrameInfo.srcHeight, inputFrameInfo.csp, inputFrameInfo.bitdepth, inputFrameInfo.picstruct, RGY_MEM_TYPE_CPU);
        return std::make_pair(info, m_outMaxQueueSize);
    };
    virtual bool abort() { m_abort = true; return true; }; // 中断指示を受け取ったらtrueを返す
    RGY_ERR LoadNextFrameCL() {
        auto surfWork = getWorkSurf();
        if (surfWork == nullptr) {
            PrintMes(RGY_LOG_ERROR, _T("failed to get work surface for input.\n"));
            return RGY_ERR_NOT_ENOUGH_BUFFER;
        }
        auto clframe = surfWork.cl();
        auto err = clframe->queueMapBuffer(m_cl->queue(), CL_MAP_WRITE); // CPUが書き込むためにMapする
        if (err != RGY_ERR_NONE) {
            PrintMes(RGY_LOG_ERROR, _T("Failed to map buffer: %s.\n"), get_err_mes(err));
            return err;
        }
        clframe->mapWait(); //すぐ終わるはず
        auto mappedframe = clframe->mappedHost();
        err = m_input->LoadNextFrame(mappedframe);
        if (err != RGY_ERR_NONE) {
            //Unlockする必要があるので、ここに入ってもすぐにreturnしてはいけない
            if (err == RGY_ERR_MORE_DATA) { // EOF
                err = RGY_ERR_MORE_BITSTREAM; // EOF を PipelineTaskMFXDecode のreturnコードに合わせる
            } else {
                PrintMes(RGY_LOG_ERROR, _T("Error in reader: %s.\n"), get_err_mes(err));
            }
        }
        clframe->setPropertyFrom(mappedframe);
        auto clerr = clframe->unmapBuffer();
        if (clerr != RGY_ERR_NONE) {
            PrintMes(RGY_LOG_ERROR, _T("Failed to unmap buffer: %s.\n"), get_err_mes(err));
            if (err == RGY_ERR_NONE) {
                err = clerr;
            }
        }
        if (err == RGY_ERR_NONE) {
            surfWork.frame()->setDuration(mappedframe->duration());
            surfWork.frame()->setTimestamp(mappedframe->timestamp());
            surfWork.frame()->setInputFrameId(mappedframe->inputFrameId());
            surfWork.frame()->setPicstruct(mappedframe->picstruct());
            surfWork.frame()->setFlags(mappedframe->flags());
            surfWork.frame()->setDataList(mappedframe->dataList());
            surfWork.frame()->setInputFrameId(m_inFrames++);
            m_outQeueue.push_back(std::make_unique<PipelineTaskOutputSurf>(surfWork));
        }
        return err;
    }
    RGY_ERR LoadNextFrameSys() {
        const auto inputFrameInfo = m_input->GetInputFrameInfo();
        RGYFrameInfo info(inputFrameInfo.srcWidth, inputFrameInfo.srcHeight, inputFrameInfo.csp, inputFrameInfo.bitdepth, inputFrameInfo.picstruct, RGY_MEM_TYPE_MPP);
        auto surfWork = getNewWorkSurfMpp(info);
        if (surfWork == nullptr) {
            PrintMes(RGY_LOG_ERROR, _T("failed to get work surface for input.\n"));
            return RGY_ERR_NOT_ENOUGH_BUFFER;
        }
        auto err = m_input->LoadNextFrame(surfWork.get());
        if (err == RGY_ERR_MORE_DATA) {// EOF
            err = RGY_ERR_MORE_BITSTREAM; // EOF を PipelineTaskMFXDecode のreturnコードに合わせる
        } else if (err != RGY_ERR_NONE) {
            PrintMes(RGY_LOG_ERROR, _T("Error in reader: %s.\n"), get_err_mes(err));
        } else {
            surfWork->setInputFrameId(m_inFrames++);
            m_outQeueue.push_back(std::make_unique<PipelineTaskOutputSurf>(m_workSurfs.addSurface(surfWork)));
        }
        return err;
    }
    virtual RGY_ERR sendFrame([[maybe_unused]] std::unique_ptr<PipelineTaskOutput>& frame) override {
        if (m_abort) {
            return RGY_ERR_MORE_BITSTREAM; // EOF を PipelineTaskMFXDecode のreturnコードに合わせる
        }
        if (workSurfaceType() == PipelineTaskSurfaceType::CL) {
            return LoadNextFrameCL();
        }
        return LoadNextFrameSys();
    }
};

class PipelineTaskMPPDecode : public PipelineTask {
protected:
    struct FrameData {
        int64_t timestamp;
        int64_t duration;
        RGY_FRAME_FLAGS flags;

        FrameData() : timestamp(AV_NOPTS_VALUE), duration(0), flags(RGY_FRAME_FLAG_NONE) {};
        FrameData(int64_t pts, int64_t duration_, RGY_FRAME_FLAGS f) : timestamp(pts), duration(duration_), flags(f) {};
    };
    MPPContext *m_dec;
    RGYInput *m_input;
    RGYBitstream m_decInputBitstream;
    MppBufferGroup m_frameGrp;
    bool m_adjustTimestamp;
    int64_t m_firstBitstreamTimestamp; // bitstreamの最初のtimestamp
    int64_t m_firstFrameTimestamp; // frameの最初のtimestamp
    std::deque<int64_t> m_queueTimestamp; // timestampへのキュー
    std::vector<int64_t> m_queueTimestampWrap; // wrapした場合のtimestamp
    RGYQueueMPMP<RGYFrameDataMetadata*> m_queueHDR10plusMetadata;
    RGYQueueMPMP<FrameData> m_dataFlag;
    bool m_decInBitStreamEOS; // デコーダへの入力Bitstream側でEOSを検知
    bool m_decOutFrameEOS;    // デコーダからの出力Frame側でEOSを検知
    bool m_abort;
    int m_decOutFrames;
public:
    PipelineTaskMPPDecode(MPPContext *dec, int outMaxQueueSize, RGYInput *input, bool adjustTimestamp, std::shared_ptr<RGYLog> log)
        : PipelineTask(PipelineTaskType::MPPDEC, outMaxQueueSize, log), m_dec(dec), m_input(input),
        m_decInputBitstream(RGYBitstreamInit()), m_frameGrp(), m_adjustTimestamp(adjustTimestamp),
        m_firstBitstreamTimestamp(-1), m_firstFrameTimestamp(-1), m_queueTimestamp(), m_queueTimestampWrap(), m_queueHDR10plusMetadata(), m_dataFlag(),
        m_decInBitStreamEOS(false), m_decOutFrameEOS(false), m_abort(false), m_decOutFrames(0) {
        m_queueHDR10plusMetadata.init(256);
        m_dataFlag.init();
    };
    virtual ~PipelineTaskMPPDecode() {
        if (m_frameGrp) {
            mpp_buffer_group_put(m_frameGrp);
            m_frameGrp = nullptr;
        }
        m_queueHDR10plusMetadata.close([](RGYFrameDataMetadata **ptr) { if (*ptr) { delete *ptr; *ptr = nullptr; }; });
        m_decInputBitstream.clear();
    };
    void setDec(MPPContext *dec) { m_dec = dec; };
    virtual bool abort() { m_abort = true; return true; }; // 中断指示を受け取ったらtrueを返す

    virtual std::optional<std::pair<RGYFrameInfo, int>> requiredSurfIn() override { return std::nullopt; };
    virtual std::optional<std::pair<RGYFrameInfo, int>> requiredSurfOut() override {
        const auto inputFrameInfo = m_input->GetInputFrameInfo();
        RGYFrameInfo info(inputFrameInfo.srcWidth, inputFrameInfo.srcHeight, inputFrameInfo.csp, inputFrameInfo.bitdepth, inputFrameInfo.picstruct, RGY_MEM_TYPE_MPP);
        return std::make_pair(info, m_outMaxQueueSize);
    };

    // あとで取得すために入力したフレームを設定しておく
    void addBitstreamTimestamp(const int64_t pts) {
        if (!m_adjustTimestamp) {
            return;
        }
        if (m_firstBitstreamTimestamp < 0) {
            m_firstBitstreamTimestamp = pts;
        }
        if (m_queueTimestamp.empty() && m_queueTimestampWrap.size() > 0) {
            std::sort(m_queueTimestampWrap.begin(), m_queueTimestampWrap.end());
            m_queueTimestamp.insert(m_queueTimestamp.end(), m_queueTimestampWrap.begin(), m_queueTimestampWrap.end());
            m_queueTimestampWrap.clear();
        }
        if (m_queueTimestamp.empty()) {
            m_queueTimestamp.push_back(pts);
            return;
        }
        const auto inputFrameInfo = m_input->GetInputFrameInfo();
        const auto fpsInv = av_make_q(inputFrameInfo.fpsD, inputFrameInfo.fpsN);
        const auto inputTimebase = av_make_q(m_input->getInputTimebase());
        const auto frameDurationx16 = av_rescale_q(16, fpsInv, inputTimebase);
        auto lastPts = m_queueTimestamp.back();
        if (std::abs(pts - lastPts) > frameDurationx16) { // 大きく離れている場合は別のキューに入れる
            m_queueTimestampWrap.push_back(pts);
            return;
        }
        m_queueTimestamp.push_back(pts);
        std::sort(m_queueTimestamp.begin(), m_queueTimestamp.end());
        return;
    }

    void cleanTimestampQueue(const int64_t pts) {
        while (!m_queueTimestamp.empty() && m_queueTimestamp.front() <= pts) {
            m_queueTimestamp.pop_front();
        }
    }

    // MPEG2デコーダのようにまともなtimestampを返さないときに入力bitstreamの情報を使って修正する
    int64_t adjustFrameTimestamp(int64_t pts) {
        if (!m_adjustTimestamp || m_queueTimestamp.empty()) {
            return pts;
        }
        if (m_adjustTimestamp) {
            if (m_firstFrameTimestamp < 0) { // 最初のフレーム
                m_firstFrameTimestamp = pts;
                const auto pos = std::find(m_queueTimestamp.begin(), m_queueTimestamp.end(), pts);
                if (pos == m_queueTimestamp.end()) { //知らないtimestamp
                    pts = m_firstBitstreamTimestamp; // 入力bitstreamの最初のtimestampで代用
                }
            } else {
                pts = m_queueTimestamp.front();
                m_queueTimestamp.pop_front();
            }
        }
        cleanTimestampQueue(pts);
        return pts;
    }

    virtual RGY_ERR getOutputFrame(int& trycount) {
        MppFrame mppframe = nullptr;
        auto ret = err_to_rgy(m_dec->mpi->decode_get_frame(m_dec->ctx, &mppframe));
        if (ret == RGY_ERR_MPP_ERR_TIMEOUT) { // デコード待ち
            ret = RGY_ERR_MPP_ERR_TIMEOUT;
            if (trycount >= 10000) {
                PrintMes(RGY_LOG_ERROR, _T("decode_get_frame timeout too much time\n"));
                return RGY_ERR_UNKNOWN;
            }
            return ret;
        } else if (!mppframe) {
            return RGY_ERR_MORE_DATA; // 次のbitstreamが必要
        }
        trycount = 0;

        if (ret != RGY_ERR_NONE) {
            PrintMes(RGY_LOG_ERROR, _T("decode_get_frame failed: %s\n"), get_err_mes(ret));
            return ret;
        }
        const int width = mpp_frame_get_width(mppframe);
        const int height = mpp_frame_get_height(mppframe);
        const int stride_w = mpp_frame_get_hor_stride(mppframe);
        const int stride_h = mpp_frame_get_ver_stride(mppframe);
        const int buf_size = mpp_frame_get_buf_size(mppframe);
        if (mpp_frame_get_info_change(mppframe)) {
            if (m_frameGrp == nullptr) {
                ret = err_to_rgy(mpp_buffer_group_get_internal(&m_frameGrp, MPP_BUFFER_TYPE_ION));
                if (ret != RGY_ERR_NONE) {
                    PrintMes(RGY_LOG_ERROR, _T("Get mpp buffer group failed : %s\n"), get_err_mes(ret));
                    return ret;
                }

                // Set buffer to mpp decoder
                ret = err_to_rgy(m_dec->mpi->control(m_dec->ctx, MPP_DEC_SET_EXT_BUF_GROUP, m_frameGrp));
                if (ret != RGY_ERR_NONE) {
                    PrintMes(RGY_LOG_ERROR, _T("Set buffer group failed : %s\n"), get_err_mes(ret));
                    return ret;
                }
            } else {
                // If old buffer group exist clear it
                ret = err_to_rgy(mpp_buffer_group_clear(m_frameGrp));
                if (ret != RGY_ERR_NONE) {
                    PrintMes(RGY_LOG_ERROR, _T("Clear buffer group failed : %s\n"), get_err_mes(ret));
                    return ret;
                }
            }

            // Use limit config to limit buffer count to 32 with buf_size
            ret = err_to_rgy(mpp_buffer_group_limit_config(m_frameGrp, buf_size, 32));
            if (ret != RGY_ERR_NONE) {
                PrintMes(RGY_LOG_ERROR, _T("Limit buffer group failed : %s\n"), get_err_mes(ret));
                return ret;
            }

            // All buffer group config done.
            // Set info change ready to let decoder continue decoding
            ret = err_to_rgy(m_dec->mpi->control(m_dec->ctx, MPP_DEC_SET_INFO_CHANGE_READY, nullptr));
            if (ret != RGY_ERR_NONE) {
                PrintMes(RGY_LOG_ERROR, _T("Info change ready failed : %s\n"), get_err_mes(ret));
                return ret;
            }
        } else if (mpp_frame_get_discard(mppframe)) {
            PrintMes(RGY_LOG_ERROR, _T("Received a discard frame.\n"));
            ret = RGY_ERR_UNKNOWN;
        } else if (auto e = mpp_frame_get_errinfo(mppframe); e != 0) {
            PrintMes(RGY_LOG_ERROR, _T("Received a errinfo frame: 0x%08x.\n"), e);
            ret = RGY_ERR_UNKNOWN;
        } else {
            //copy
            const bool eos = mpp_frame_get_eos(mppframe);
            ret = setOutputSurf(mppframe);
            if (eos || ret == RGY_ERR_MORE_BITSTREAM) {
                PrintMes(RGY_LOG_DEBUG, _T("Received a eos frame.\n"));
                m_decOutFrameEOS = true;
                ret = RGY_ERR_MORE_BITSTREAM; // EOS
            }
        }
        if (mppframe) {
            mpp_frame_deinit(&mppframe);
        }
        return ret;
    }

    virtual RGY_ERR sendFrame([[maybe_unused]] std::unique_ptr<PipelineTaskOutput>& frame) override {
        auto ret = m_input->LoadNextFrame(nullptr);
        if (ret != RGY_ERR_NONE && ret != RGY_ERR_MORE_DATA && ret != RGY_ERR_MORE_BITSTREAM) {
            PrintMes(RGY_LOG_ERROR, _T("Error in reader: %s.\n"), get_err_mes(ret));
            return ret;
        }
        if (m_abort) {
            ret = RGY_ERR_MORE_BITSTREAM;
            m_decInputBitstream.setSize(0);
            m_decInputBitstream.setOffset(0);
        } else {
            //この関数がMFX_ERR_NONE以外を返せば、入力ビットストリームは終了
            ret = m_input->GetNextBitstream(&m_decInputBitstream);
        }
        if (ret == RGY_ERR_MORE_BITSTREAM) { //入力ビットストリームは終了
            if (m_decInBitStreamEOS) { // すでにeosを送信していた場合
                PrintMes(RGY_LOG_DEBUG, _T("Already sent null packet.\n"));
                return ret;
            }
        } else if (ret != RGY_ERR_NONE) {
            PrintMes(RGY_LOG_ERROR, _T("Error on getting video bitstream: %s.\n"), get_err_mes(ret));
            return ret;
        } else {
            for (auto& frameData : m_decInputBitstream.getFrameDataList()) {
                if (frameData->dataType() == RGY_FRAME_DATA_HDR10PLUS) {
                    auto ptr = dynamic_cast<RGYFrameDataHDR10plus*>(frameData);
                    if (ptr) {
                        m_queueHDR10plusMetadata.push(new RGYFrameDataHDR10plus(*ptr));
                    }
                } else if (frameData->dataType() == RGY_FRAME_DATA_DOVIRPU) {
                    auto ptr = dynamic_cast<RGYFrameDataDOVIRpu*>(frameData);
                    if (ptr) {
                        m_queueHDR10plusMetadata.push(new RGYFrameDataDOVIRpu(*ptr));
                    }
                }
            }
            const auto data = FrameData(m_decInputBitstream.pts(), m_decInputBitstream.duration(), (RGY_FRAME_FLAGS)m_decInputBitstream.dataflag());
            m_dataFlag.push(data);
        }
        MppPacket packet;
        if (m_decInputBitstream.size() > 0) {
            mpp_packet_init(&packet, m_decInputBitstream.data(), m_decInputBitstream.size());
            mpp_packet_set_pts(packet, m_decInputBitstream.pts());
            //auto meta = mpp_packet_get_meta(packet);
            //mpp_meta_set_s64(meta, KEY_USER_DATA, m_decInputBitstream.duration());
            addBitstreamTimestamp(m_decInputBitstream.pts());
            PrintMes(RGY_LOG_TRACE, _T("Send input bitstream: %lld.\n"), m_decInputBitstream.pts());
        } else if (ret == RGY_ERR_MORE_BITSTREAM) {
            mpp_packet_init(&packet, nullptr, 0);
            mpp_packet_set_pts(packet, 0);
            mpp_packet_set_flag(packet, 0);
            mpp_packet_set_eos(packet);
            m_decInBitStreamEOS = true;
            PrintMes(RGY_LOG_DEBUG, _T("Send null packet.\n"));
            ret = RGY_ERR_NONE;
        } else {
            PrintMes(RGY_LOG_ERROR, _T("ERROR: Failed to get input bitstream: %s.\n"), get_err_mes(ret));
            return ret;
        }

        bool pkt_send = false;
        for (int trycount = 0; !pkt_send || ret == RGY_ERR_NONE; trycount++) {
            if (!pkt_send) {
                ret = err_to_rgy(m_dec->mpi->decode_put_packet(m_dec->ctx, packet));
                PrintMes(RGY_LOG_TRACE, _T("decode_put_packet: %s\n"), get_err_mes(ret));
                if (ret == RGY_ERR_NONE) {
                    pkt_send = true;
                    mpp_packet_deinit(&packet);
                }
            }
            ret = getOutputFrame(trycount);
            if (!pkt_send && (ret == RGY_ERR_MPP_ERR_TIMEOUT || ret == RGY_ERR_MORE_DATA)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
        if (pkt_send) { // パケットを送り終わったらbitstreamをクリア
            m_decInputBitstream.setSize(0);
            m_decInputBitstream.setOffset(0);
        }
        if (m_decInBitStreamEOS) { // bitstreamのEOSを送った場合は、最後までフレームを回収する
            ret = RGY_ERR_NONE;
            PrintMes(RGY_LOG_DEBUG, _T("dec loop fin: %s\n"), get_err_mes(ret));
            for (int trycount = 0; !m_decOutFrameEOS; trycount++) {
                ret = getOutputFrame(trycount);
                PrintMes(RGY_LOG_TRACE, _T("getOutputFrameEOS: %s\n"), get_err_mes(ret));
                if (ret != RGY_ERR_NONE
                 && ret != RGY_ERR_MPP_ERR_TIMEOUT
                 && ret != RGY_ERR_MORE_DATA
                 && ret != RGY_ERR_MORE_BITSTREAM) {
                    break; //エラー
                }
            }
        }
        if (ret == RGY_ERR_MORE_BITSTREAM) {
            return ret; // EOS
        } else if (ret != RGY_ERR_NONE && ret != RGY_ERR_MPP_ERR_TIMEOUT && ret != RGY_ERR_MPP_ERR_BUFFER_FULL && ret != RGY_ERR_MORE_DATA) {
            PrintMes(RGY_LOG_ERROR, _T("Failed to load input frame: %s.\n"), get_err_mes(ret));
            return ret;
        }
        return RGY_ERR_NONE;
    }
protected:
    RGY_ERR setOutputSurf(MppFrame& mppframe) {
        { // eosの場合はフレームがないこともある
            void *bufptr = nullptr;
            auto buffer = mpp_frame_get_buffer(mppframe);
            if (buffer) {
                bufptr = mpp_buffer_get_ptr(buffer);
            }
            if (bufptr == nullptr) { 
                return (mpp_frame_get_eos(mppframe)) ? RGY_ERR_MORE_BITSTREAM : RGY_ERR_NULL_PTR;
            }
        }
        const auto mode = mpp_frame_get_mode(mppframe);
        auto timestamp = adjustFrameTimestamp(mpp_frame_get_pts(mppframe));
        uint64_t duration = 0;
        auto flags = RGY_FRAME_FLAG_NONE;
        if (auto frameData = getDataFlag(timestamp); frameData.timestamp != AV_NOPTS_VALUE) {
            if (frameData.flags & RGY_FRAME_FLAG_RFF) {
                flags |= RGY_FRAME_FLAG_RFF;
            }
            duration = frameData.duration;
        }
        if (mode & MPP_FRAME_FLAG_TOP_FIRST) {
            flags |= RGY_FRAME_FLAG_RFF_TFF;
        }
        if (mode & MPP_FRAME_FLAG_BOT_FIRST) {
            flags |= RGY_FRAME_FLAG_RFF_BFF;
        }

        std::unique_ptr<RGYFrame> outSurf = std::make_unique<RGYFrameMpp>(mppframe, duration, flags);
        mppframe = nullptr;
        outSurf->setTimestamp(timestamp);
        outSurf->setInputFrameId(m_decOutFrames++);

        //mppframeのインタレはちゃんと設定されてない場合があるので、auto以外の時は入力設定で上書きする
        const auto inputFrameInfo = m_input->GetInputFrameInfo();
        if (inputFrameInfo.picstruct != RGY_PICSTRUCT_AUTO) {
            outSurf->setPicstruct(inputFrameInfo.picstruct);
        } else if (mode & MPP_FRAME_FLAG_TOP_FIRST) {
            outSurf->setPicstruct(RGY_PICSTRUCT_FRAME_TFF);
        } else if (mode & MPP_FRAME_FLAG_BOT_FIRST) {
            outSurf->setPicstruct(RGY_PICSTRUCT_FRAME_BFF);
        } else {
            outSurf->setPicstruct(RGY_PICSTRUCT_FRAME);
        }
        outSurf->clearDataList();
        if (auto data = getMetadata(RGY_FRAME_DATA_HDR10PLUS, outSurf->timestamp()); data) {
            outSurf->dataList().push_back(data);
        }
        if (auto data = getMetadata(RGY_FRAME_DATA_DOVIRPU, outSurf->timestamp()); data) {
            outSurf->dataList().push_back(data);
        }
        PrintMes(RGY_LOG_TRACE, _T("setOutputSurf: %d: mode 0x%04x, pts %lld: %lld.\n"), m_decOutFrames, mode, outSurf->timestamp(), outSurf->duration());
        m_outQeueue.push_back(std::make_unique<PipelineTaskOutputSurf>(m_workSurfs.addSurface(outSurf)));
        return RGY_ERR_NONE;
    }
    FrameData getDataFlag(const int64_t timestamp) {
        FrameData pts_flag;
        while (m_dataFlag.front_copy_no_lock(&pts_flag)) {
            if (pts_flag.timestamp < timestamp || pts_flag.timestamp == AV_NOPTS_VALUE) {
                m_dataFlag.pop();
            } else {
                break;
            }
        }
        size_t queueSize = m_dataFlag.size();
        for (uint32_t i = 0; i < queueSize; i++) {
            if (m_dataFlag.copy(&pts_flag, i, &queueSize)) {
                if (pts_flag.timestamp == timestamp) {
                    return pts_flag;
                }
            }
        }
        return FrameData();
    }
    std::shared_ptr<RGYFrameData> getMetadata(const RGYFrameDataType datatype, const int64_t timestamp) {
        std::shared_ptr<RGYFrameData> frameData;
        RGYFrameDataMetadata *frameDataPtr = nullptr;
        while (m_queueHDR10plusMetadata.front_copy_no_lock(&frameDataPtr)) {
            if (frameDataPtr->timestamp() < timestamp) {
                m_queueHDR10plusMetadata.pop();
                delete frameDataPtr;
            } else {
                break;
            }
        }
        size_t queueSize = m_queueHDR10plusMetadata.size();
        for (uint32_t i = 0; i < queueSize; i++) {
            if (m_queueHDR10plusMetadata.copy(&frameDataPtr, i, &queueSize)) {
                if (frameDataPtr->timestamp() == timestamp && frameDataPtr->dataType() == datatype) {
                    if (frameDataPtr->dataType() == RGY_FRAME_DATA_HDR10PLUS) {
                        auto ptr = dynamic_cast<RGYFrameDataHDR10plus*>(frameDataPtr);
                        if (ptr) {
                            frameData = std::make_shared<RGYFrameDataHDR10plus>(*ptr);
                        }
                    } else if (frameData->dataType() == RGY_FRAME_DATA_DOVIRPU) {
                        auto ptr = dynamic_cast<RGYFrameDataDOVIRpu*>(frameDataPtr);
                        if (ptr) {
                            frameData = std::make_shared<RGYFrameDataDOVIRpu>(*ptr);
                        }
                    }
                    break;
                }
            }
        }
        return frameData;
    };
};

class PipelineTaskCheckPTS : public PipelineTask {
protected:
    rgy_rational<int> m_srcTimebase;
    rgy_rational<int> m_streamTimebase;
    rgy_rational<int> m_outputTimebase;
    RGYAVSync m_avsync;
    bool m_timestampPassThrough;
    bool m_vpp_rff;
    bool m_vpp_afs_rff_aware;
    int64_t m_outFrameDuration; //(m_outputTimebase基準)
    int64_t m_tsOutFirst;     //(m_outputTimebase基準)
    int64_t m_tsOutEstimated; //(m_outputTimebase基準)
    int64_t m_tsPrev;         //(m_outputTimebase基準)
    uint32_t m_inputFramePosIdx;
    FramePosList *m_framePosList;
    static const int64_t INVALID_PTS = AV_NOPTS_VALUE;
public:
    PipelineTaskCheckPTS(rgy_rational<int> srcTimebase, rgy_rational<int> streamTimebase, rgy_rational<int> outputTimebase, int64_t outFrameDuration, RGYAVSync avsync, bool timestampPassThrough, bool vpp_afs_rff_aware, FramePosList *framePosList, std::shared_ptr<RGYLog> log) :
        PipelineTask(PipelineTaskType::CHECKPTS, /*outMaxQueueSize = */ 0 /*常に0である必要がある*/, log),
        m_srcTimebase(srcTimebase), m_streamTimebase(streamTimebase), m_outputTimebase(outputTimebase), m_avsync(avsync), m_timestampPassThrough(timestampPassThrough), m_vpp_rff(false), m_vpp_afs_rff_aware(vpp_afs_rff_aware), m_outFrameDuration(outFrameDuration),
        m_tsOutFirst(INVALID_PTS), m_tsOutEstimated(0), m_tsPrev(-1), m_inputFramePosIdx(std::numeric_limits<decltype(m_inputFramePosIdx)>::max()), m_framePosList(framePosList) {
    };
    virtual ~PipelineTaskCheckPTS() {};

    virtual bool isPassThrough() const override {
        // そのまま渡すのでpaththrough
        return true;
    }
    static const int MAX_FORCECFR_INSERT_FRAMES = 1024; //事実上の無制限
public:
    virtual std::optional<std::pair<RGYFrameInfo, int>> requiredSurfIn() override { return std::nullopt; };
    virtual std::optional<std::pair<RGYFrameInfo, int>> requiredSurfOut() override { return std::nullopt; };

    virtual RGY_ERR sendFrame(std::unique_ptr<PipelineTaskOutput>& frame) override {
        if (!frame) {
            //PipelineTaskCheckPTSは、getOutputで1フレームずつしか取り出さない
            //そのためm_outQeueueにまだフレームが残っている可能性がある
            return (m_outQeueue.size() > 0) ? RGY_ERR_MORE_SURFACE : RGY_ERR_MORE_DATA;
        }
        int64_t outPtsSource = m_tsOutEstimated; //(m_outputTimebase基準)
        int64_t outDuration = m_outFrameDuration; //入力fpsに従ったduration

        PipelineTaskOutputSurf *taskSurf = dynamic_cast<PipelineTaskOutputSurf *>(frame.get());
        if (taskSurf == nullptr) {
            PrintMes(RGY_LOG_ERROR, _T("Invalid frame type: failed to cast to PipelineTaskOutputSurf.\n"));
            return RGY_ERR_UNSUPPORTED;
        }

        if ((m_srcTimebase.n() > 0 && m_srcTimebase.is_valid())
            && ((m_avsync & (RGY_AVSYNC_VFR | RGY_AVSYNC_FORCE_CFR)) || m_vpp_rff || m_vpp_afs_rff_aware || m_timestampPassThrough)) {
            //CFR仮定ではなく、オリジナルの時間を見る
            const auto srcTimestamp = taskSurf->surf().frame()->timestamp();
            outPtsSource = rational_rescale(srcTimestamp, m_srcTimebase, m_outputTimebase);
            if (taskSurf->surf().frame()->duration() > 0) {
                outDuration = rational_rescale(taskSurf->surf().frame()->duration(), m_srcTimebase, m_outputTimebase);
                taskSurf->surf().frame()->setDuration(outDuration);
            }
        }
        PrintMes(RGY_LOG_TRACE, _T("check_pts(%d/%d): nOutEstimatedPts %lld, outPtsSource %lld, outDuration %d\n"), taskSurf->surf().frame()->inputFrameId(), m_inFrames, m_tsOutEstimated, outPtsSource, outDuration);
        if (m_tsOutFirst == INVALID_PTS) {
            m_tsOutFirst = outPtsSource; //最初のpts
            PrintMes(RGY_LOG_TRACE, _T("check_pts: m_tsOutFirst %lld\n"), outPtsSource);
        }
        //最初のptsを0に修正
        if (!m_timestampPassThrough) {
            //最初のptsを0に修正
            outPtsSource -= m_tsOutFirst;
        }

        if ((m_avsync & RGY_AVSYNC_VFR) || m_vpp_rff || m_vpp_afs_rff_aware) {
            if (m_vpp_rff || m_vpp_afs_rff_aware) {
                if (std::abs(outPtsSource - m_tsOutEstimated) >= 32 * m_outFrameDuration) {
                    PrintMes(RGY_LOG_TRACE, _T("check_pts: detected gap %lld, changing offset.\n"), outPtsSource, std::abs(outPtsSource - m_tsOutEstimated));
                    //timestampに一定以上の差があればそれを無視する
                    m_tsOutFirst += (outPtsSource - m_tsOutEstimated); //今後の位置合わせのための補正
                    outPtsSource = m_tsOutEstimated;
                    PrintMes(RGY_LOG_TRACE, _T("check_pts:   changed to m_tsOutFirst %lld, outPtsSource %lld.\n"), m_tsOutFirst, outPtsSource);
                }
                auto ptsDiff = outPtsSource - m_tsOutEstimated;
                if (ptsDiff <= std::min<int64_t>(-1, -1 * m_outFrameDuration * 7 / 8)) {
                    //間引きが必要
                    PrintMes(RGY_LOG_TRACE, _T("check_pts(%d):   skipping frame (vfr)\n"), taskSurf->surf().frame()->inputFrameId());
                    return RGY_ERR_MORE_SURFACE;
                }
                // 少しのずれはrffによるものとみなし、基準値を修正する
                m_tsOutEstimated = outPtsSource;
            }
            if ((ENCODER_VCEENC || ENCODER_MPP) && m_framePosList) {
                //cuvidデコード時は、timebaseの分子はかならず1なので、streamIn->time_baseとズレているかもしれないのでオリジナルを計算
                const auto orig_pts = rational_rescale(taskSurf->surf().frame()->timestamp(), m_srcTimebase, m_streamTimebase);
                //ptsからフレーム情報を取得する
                const auto framePos = m_framePosList->findpts(orig_pts, &m_inputFramePosIdx);
                PrintMes(RGY_LOG_TRACE, _T("check_pts(%d):   estimetaed orig_pts %lld, framePos %d\n"), taskSurf->surf().frame()->inputFrameId(), orig_pts, framePos.poc);
                if (framePos.poc != FRAMEPOS_POC_INVALID && framePos.duration > 0) {
                    //有効な値ならオリジナルのdurationを使用する
                    outDuration = rational_rescale(framePos.duration, m_streamTimebase, m_outputTimebase);
                    PrintMes(RGY_LOG_TRACE, _T("check_pts(%d):   changing duration to original: %d\n"), taskSurf->surf().frame()->inputFrameId(), outDuration);
                }
            }
        }
        if (m_avsync & RGY_AVSYNC_FORCE_CFR) {
            if (std::abs(outPtsSource - m_tsOutEstimated) >= CHECK_PTS_MAX_INSERT_FRAMES * m_outFrameDuration) {
                //timestampに一定以上の差があればそれを無視する
                m_tsOutFirst += (outPtsSource - m_tsOutEstimated); //今後の位置合わせのための補正
                outPtsSource = m_tsOutEstimated;
                PrintMes(RGY_LOG_WARN, _T("Big Gap was found between 2 frames, avsync might be corrupted.\n"));
                PrintMes(RGY_LOG_TRACE, _T("check_pts:   changed to m_tsOutFirst %lld, outPtsSource %lld.\n"), m_tsOutFirst, outPtsSource);
            }
            auto ptsDiff = outPtsSource - m_tsOutEstimated;
            if (ptsDiff <= std::min<int64_t>(-1, -1 * m_outFrameDuration * 7 / 8)) {
                //間引きが必要
                PrintMes(RGY_LOG_DEBUG, _T("Drop frame: framepts %lld, estimated next %lld, diff %lld [%.1f]\n"), outPtsSource, m_tsOutEstimated, ptsDiff, ptsDiff / (double)m_outFrameDuration);
                return RGY_ERR_MORE_SURFACE;
            }
            while (ptsDiff >= std::max<int64_t>(1, m_outFrameDuration * 7 / 8)) {
                PrintMes(RGY_LOG_DEBUG, _T("Insert frame: framepts %lld, estimated next %lld, diff %lld [%.1f]\n"), outPtsSource, m_tsOutEstimated, ptsDiff, ptsDiff / (double)m_outFrameDuration);
                //水増しが必要
                if (ENCODER_MPP && taskSurf->surf().mpp() != nullptr) {
                    // mppのフレームをエンコーダに投入すると、そちらで破棄されるので、ここで複製しておく
                    // createCopyはバッファ自体は参照なので、何度複製してもそれほどメモリ使用量は増えない
                    std::unique_ptr<RGYFrame> copyFrame = taskSurf->surf().mpp()->createCopy();
                    copyFrame->setTimestamp(m_tsOutEstimated);
                    copyFrame->setInputFrameId(taskSurf->surf().frame()->inputFrameId());
                    copyFrame->setDuration(outDuration);
                    m_outQeueue.push_back(std::make_unique<PipelineTaskOutputSurf>(m_workSurfs.addSurface(copyFrame)));
                } else {
                    // 同じフレームを2回キューに入れる形にして、コピーコストを低減する
                    PipelineTaskSurface surfVppOut = taskSurf->surf();
                    surfVppOut.frame()->setInputFrameId(taskSurf->surf().frame()->inputFrameId());
                    surfVppOut.frame()->setTimestamp(m_tsOutEstimated);
                    if (ENCODER_VCEENC) {
                        surfVppOut.frame()->setDuration(outDuration);
                    }
                    //timestampの上書き情報
                    //surfVppOut内部のmfxSurface1自体は同じデータを指すため、複数のタイムスタンプを持つことができない
                    //この問題をm_outQeueueのPipelineTaskOutput(これは個別)に与えるPipelineTaskOutputDataCheckPtsの値で、
                    //PipelineTaskCheckPTS::getOutput時にtimestampを変更するようにする
                    //そのため、checkptsからgetOutputしたフレームは
                    //(次にPipelineTaskCheckPTS::getOutputを呼ぶより前に)直ちに後続タスクに投入するよう制御する必要がある
                    std::unique_ptr<PipelineTaskOutputDataCustom> timestampOverride(new PipelineTaskOutputDataCheckPts(m_tsOutEstimated));
                    m_outQeueue.push_back(std::make_unique<PipelineTaskOutputSurf>(surfVppOut, timestampOverride));
                }
                m_tsOutEstimated += m_outFrameDuration;
                ptsDiff = outPtsSource - m_tsOutEstimated;
            }
            outPtsSource = m_tsOutEstimated;
        }
        if (m_tsPrev >= outPtsSource) {
            if (m_tsPrev - outPtsSource >= MAX_FORCECFR_INSERT_FRAMES * m_outFrameDuration) {
                PrintMes(RGY_LOG_DEBUG, _T("check_pts: previous pts %lld, current pts %lld, estimated pts %lld, m_tsOutFirst %lld, changing offset.\n"), m_tsPrev, outPtsSource, m_tsOutEstimated, m_tsOutFirst);
                m_tsOutFirst += (outPtsSource - m_tsOutEstimated); //今後の位置合わせのための補正
                outPtsSource = m_tsOutEstimated;
                PrintMes(RGY_LOG_DEBUG, _T("check_pts:   changed to m_tsOutFirst %lld, outPtsSource %lld.\n"), m_tsOutFirst, outPtsSource);
            } else {
                if (m_avsync & RGY_AVSYNC_FORCE_CFR) {
                    //間引きが必要
                    PrintMes(RGY_LOG_WARN, _T("check_pts(%d/%d): timestamp of video frame is smaller than previous frame, skipping frame: previous pts %lld, current pts %lld.\n"),
                        taskSurf->surf().frame()->inputFrameId(), m_inFrames, m_tsPrev, outPtsSource);
                    return RGY_ERR_MORE_SURFACE;
                } else {
                    const auto origPts = outPtsSource;
                    outPtsSource = m_tsPrev + std::max<int64_t>(1, m_outFrameDuration / 4);
                    PrintMes(RGY_LOG_WARN, _T("check_pts(%d/%d): timestamp of video frame is smaller than previous frame, changing pts: %lld -> %lld (previous pts %lld).\n"),
                        taskSurf->surf().frame()->inputFrameId(), m_inFrames, origPts, outPtsSource, m_tsPrev);
                }
            }
        }

        //次のフレームのptsの予想
        m_inFrames++;
        m_tsOutEstimated += outDuration;
        m_tsPrev = outPtsSource;
        PipelineTaskSurface outSurf = taskSurf->surf();
        outSurf.frame()->setInputFrameId(taskSurf->surf().frame()->inputFrameId());
        outSurf.frame()->setTimestamp(outPtsSource);
        outSurf.frame()->setDuration(outDuration);
        std::unique_ptr<PipelineTaskOutputDataCustom> timestampOverride(new PipelineTaskOutputDataCheckPts(outPtsSource));
        m_outQeueue.push_back(std::make_unique<PipelineTaskOutputSurf>(outSurf, timestampOverride));
        return RGY_ERR_NONE;
    }
    //checkptsではtimestampを上書きするため特別に常に1フレームしか取り出さない
    //これは--avsync frocecfrでフレームを参照コピーする際、
    //mfxSurface1自体は同じデータを指すため、複数のタイムスタンプを持つことができないため、
    //1フレームずつgetOutputし、都度タイムスタンプを上書きしてすぐに後続のタスクに投入してタイムスタンプを反映させる必要があるため
    virtual std::vector<std::unique_ptr<PipelineTaskOutput>> getOutput(const bool sync) override {
        std::vector<std::unique_ptr<PipelineTaskOutput>> output;
        if ((int)m_outQeueue.size() > m_outMaxQueueSize) {
            auto out = std::move(m_outQeueue.front());
            m_outQeueue.pop_front();
            if (sync) {
                out->waitsync();
            }
            out->depend_clear();
            if (out->customdata() != nullptr) {
                const auto dataCheckPts = dynamic_cast<const PipelineTaskOutputDataCheckPts *>(out->customdata());
                if (dataCheckPts == nullptr) {
                    PrintMes(RGY_LOG_ERROR, _T("Failed to get timestamp data, timestamp might be inaccurate!\n"));
                } else {
                    PipelineTaskOutputSurf *outSurf = dynamic_cast<PipelineTaskOutputSurf *>(out.get());
                    outSurf->surf().frame()->setTimestamp(dataCheckPts->timestampOverride());
                }
            }
            m_outFrames++;
            output.push_back(std::move(out));
        }
        if (output.size() > 1) {
            PrintMes(RGY_LOG_ERROR, _T("output queue more than 1, invalid!\n"));
        }
        return output;
    }
};

class PipelineTaskTrim : public PipelineTask {
protected:
    const sTrimParam &m_trimParam;
    RGYInput *m_input;
    rgy_rational<int> m_srcTimebase;
public:
    PipelineTaskTrim(const sTrimParam &trimParam, RGYInput *input, const rgy_rational<int>& srcTimebase, int outMaxQueueSize, std::shared_ptr<RGYLog> log) :
        PipelineTask(PipelineTaskType::TRIM, outMaxQueueSize, log),
        m_trimParam(trimParam), m_input(input), m_srcTimebase(srcTimebase) {
    };
    virtual ~PipelineTaskTrim() {};

    virtual bool isPassThrough() const override { return true; }
    virtual std::optional<std::pair<RGYFrameInfo, int>> requiredSurfIn() override { return std::nullopt; };
    virtual std::optional<std::pair<RGYFrameInfo, int>> requiredSurfOut() override { return std::nullopt; };

    virtual RGY_ERR sendFrame(std::unique_ptr<PipelineTaskOutput>& frame) override {
        if (!frame) {
            return RGY_ERR_MORE_DATA;
        }
        m_inFrames++;
        PipelineTaskOutputSurf *taskSurf = dynamic_cast<PipelineTaskOutputSurf *>(frame.get());
        if (!frame_inside_range(taskSurf->surf().frame()->inputFrameId(), m_trimParam.list).first) {
            return RGY_ERR_NONE;
        }
        if (!m_input->checkTimeSeekTo(taskSurf->surf().frame()->timestamp(), m_srcTimebase)) {
            return RGY_ERR_NONE; //seektoにより脱落させるフレーム
        }
        m_outQeueue.push_back(std::make_unique<PipelineTaskOutputSurf>(taskSurf->surf()));
        return RGY_ERR_NONE;
    }
};

class PipelineTaskAudio : public PipelineTask {
protected:
    RGYInput *m_input;
    std::map<int, std::shared_ptr<RGYOutputAvcodec>> m_pWriterForAudioStreams;
    std::map<int, RGYFilter *> m_filterForStreams;
    std::vector<std::shared_ptr<RGYInput>> m_audioReaders;
public:
    PipelineTaskAudio(RGYInput *input, std::vector<std::shared_ptr<RGYInput>>& audioReaders, std::vector<std::shared_ptr<RGYOutput>>& fileWriterListAudio, std::vector<VppVilterBlock>& vpFilters, int outMaxQueueSize, std::shared_ptr<RGYLog> log) :
        PipelineTask(PipelineTaskType::AUDIO, outMaxQueueSize, log),
        m_input(input), m_audioReaders(audioReaders) {
        //streamのindexから必要なwriteへのポインタを返すテーブルを作成
        for (auto writer : fileWriterListAudio) {
            auto pAVCodecWriter = std::dynamic_pointer_cast<RGYOutputAvcodec>(writer);
            if (pAVCodecWriter) {
                auto trackIdList = pAVCodecWriter->GetStreamTrackIdList();
                for (auto trackID : trackIdList) {
                    m_pWriterForAudioStreams[trackID] = pAVCodecWriter;
                }
            }
        }
        //streamのtrackIdからパケットを送信するvppフィルタへのポインタを返すテーブルを作成
        for (auto& filterBlock : vpFilters) {
            if (filterBlock.type == VppFilterType::FILTER_OPENCL) {
                for (auto& filter : filterBlock.vppcl) {
                    const auto targetTrackId = filter->targetTrackIdx();
                    if (targetTrackId != 0) {
                        m_filterForStreams[targetTrackId] = filter.get();
                    }
                }
            }
        }
    };
    virtual ~PipelineTaskAudio() {};
    virtual bool isPassThrough() const override { return true; }

    virtual std::optional<std::pair<RGYFrameInfo, int>> requiredSurfIn() override { return std::nullopt; };
    virtual std::optional<std::pair<RGYFrameInfo, int>> requiredSurfOut() override { return std::nullopt; };


    void flushAudio() {
        PrintMes(RGY_LOG_DEBUG, _T("Clear packets in writer...\n"));
        std::set<RGYOutputAvcodec*> writers;
        for (const auto& [ streamid, writer ] : m_pWriterForAudioStreams) {
            auto pWriter = std::dynamic_pointer_cast<RGYOutputAvcodec>(writer);
            if (pWriter != nullptr) {
                writers.insert(pWriter.get());
            }
        }
        for (const auto& writer : writers) {
            //エンコーダなどにキャッシュされたパケットを書き出す
            writer->WriteNextPacket(nullptr);
        }
    }

    RGY_ERR extractAudio(int inputFrames) {
        RGY_ERR ret = RGY_ERR_NONE;
#if ENABLE_AVSW_READER
        if (m_pWriterForAudioStreams.size() > 0) {
#if ENABLE_SM_READER
            RGYInputSM *pReaderSM = dynamic_cast<RGYInputSM *>(m_input);
            const int droppedInAviutl = (pReaderSM != nullptr) ? pReaderSM->droppedFrames() : 0;
#else
            const int droppedInAviutl = 0;
#endif

            auto packetList = m_input->GetStreamDataPackets(inputFrames + droppedInAviutl);

            //音声ファイルリーダーからのトラックを結合する
            for (const auto& reader : m_audioReaders) {
                vector_cat(packetList, reader->GetStreamDataPackets(inputFrames + droppedInAviutl));
            }
            //パケットを各Writerに分配する
            for (uint32_t i = 0; i < packetList.size(); i++) {
                AVPacket *pkt = packetList[i];
                const int nTrackId = pktFlagGetTrackID(pkt);
                const bool sendToFilter = m_filterForStreams.count(nTrackId) > 0;
                const bool sendToWriter = m_pWriterForAudioStreams.count(nTrackId) > 0;
                if (sendToFilter) {
                    AVPacket *pktToFilter = nullptr;
                    if (sendToWriter) {
                        pktToFilter = av_packet_clone(pkt);
                    } else {
                        std::swap(pktToFilter, pkt);
                    }
                    auto err = m_filterForStreams[nTrackId]->addStreamPacket(pktToFilter);
                    if (err != RGY_ERR_NONE) {
                        return err;
                    }
                }
                if (sendToWriter) {
                    auto pWriter = m_pWriterForAudioStreams[nTrackId];
                    if (pWriter == nullptr) {
                        PrintMes(RGY_LOG_ERROR, _T("Invalid writer found for %s track #%d\n"), char_to_tstring(trackMediaTypeStr(nTrackId)).c_str(), trackID(nTrackId));
                        return RGY_ERR_NOT_FOUND;
                    }
                    auto err = pWriter->WriteNextPacket(pkt);
                    if (err != RGY_ERR_NONE) {
                        return err;
                    }
                    pkt = nullptr;
                }
                if (pkt != nullptr) {
                    PrintMes(RGY_LOG_ERROR, _T("Failed to find writer for %s track #%d\n"), char_to_tstring(trackMediaTypeStr(nTrackId)).c_str(), trackID(nTrackId));
                    return RGY_ERR_NOT_FOUND;
                }
            }
        }
#endif //ENABLE_AVSW_READER
        return ret;
    };

    virtual RGY_ERR sendFrame(std::unique_ptr<PipelineTaskOutput>& frame) override {
        m_inFrames++;
        auto err = extractAudio(m_inFrames);
        if (err != RGY_ERR_NONE) {
            return err;
        }
        if (!frame) {
            flushAudio();
            return RGY_ERR_MORE_DATA;
        }
        PipelineTaskOutputSurf *taskSurf = dynamic_cast<PipelineTaskOutputSurf *>(frame.get());
        m_outQeueue.push_back(std::make_unique<PipelineTaskOutputSurf>(taskSurf->surf()));
        return RGY_ERR_NONE;
    }
};

class PipelineTaskVideoQualityMetric : public PipelineTask {
private:
    std::shared_ptr<RGYOpenCLContext> m_cl;
    RGYFilterSsim *m_videoMetric;
public:
    PipelineTaskVideoQualityMetric(RGYFilterSsim *videoMetric, std::shared_ptr<RGYOpenCLContext> cl, int outMaxQueueSize, std::shared_ptr<RGYLog> log)
        : PipelineTask(PipelineTaskType::VIDEOMETRIC, outMaxQueueSize, log), m_cl(cl), m_videoMetric(videoMetric) {
    };

    virtual bool isPassThrough() const override { return true; }
    virtual std::optional<std::pair<RGYFrameInfo, int>> requiredSurfIn() override { return std::nullopt; };
    virtual std::optional<std::pair<RGYFrameInfo, int>> requiredSurfOut() override { return std::nullopt; };
    virtual RGY_ERR sendFrame(std::unique_ptr<PipelineTaskOutput>& frame) override {
#if 0
        if (!frame) {
            return RGY_ERR_MORE_DATA;
        }
        //明示的に待機が必要
        frame->depend_clear();

        PipelineTaskOutputSurf *taskSurf = dynamic_cast<PipelineTaskOutputSurf *>(frame.get());
        if (taskSurf == nullptr) {
            PrintMes(RGY_LOG_ERROR, _T("Invalid task surface.\n"));
            return RGY_ERR_NULL_PTR;
        }
        RGYFrameInfo inputFrame;
        if (auto surfVppIn = taskSurf->surf().mppsurf(); surfVppIn != nullptr) {
            if (taskSurf->surf().frame()->getInfo().mem_type != RGY_MEM_TYPE_CPU
                && surfVppIn->GetMemoryType() != mpp::AMF_MEMORY_OPENCL) {
                mpp::AMFContext::AMFOpenCLLocker locker(m_context);
#if 0
                auto ar = inAmf->Interop(mpp::AMF_MEMORY_OPENCL);
#else
#if 0
                //dummyのCPUへのメモリコピーを行う
                //こうしないとデコーダからの出力をOpenCLに渡したときに、フレームが壊れる(フレーム順序が入れ替わってガクガクする)
                mpp::AMFDataPtr data;
                surfVppIn->Duplicate(mpp::AMF_MEMORY_HOST, &data);
#endif
                auto ar = surfVppIn->Convert(mpp::AMF_MEMORY_OPENCL);
#endif
                if (ar != AMF_OK) {
                    PrintMes(RGY_LOG_ERROR, _T("Failed to convert plane: %s.\n"), get_err_mes(err_to_rgy(ar)));
                    return err_to_rgy(ar);
                }
            }
            inputFrame = taskSurf->surf().frame()->getInfo();
        } else if (taskSurf->surf().clframe() != nullptr) {
            //OpenCLフレームが出てきた時の場合
            auto clframe = taskSurf->surf().clframe();
            if (clframe == nullptr) {
                PrintMes(RGY_LOG_ERROR, _T("Invalid cl frame.\n"));
                return RGY_ERR_NULL_PTR;
            }
            inputFrame = clframe->frameInfo();
        } else {
            PrintMes(RGY_LOG_ERROR, _T("Invalid input frame.\n"));
            return RGY_ERR_NULL_PTR;
        }
        //フレームを転送
        RGYOpenCLEvent inputReleaseEvent;
        int dummy = 0;
        auto err = m_videoMetric->filter(&inputFrame, nullptr, &dummy, m_cl->queue(), &inputReleaseEvent);
        if (err != RGY_ERR_NONE) {
            PrintMes(RGY_LOG_ERROR, _T("Failed to send frame for video metric calcualtion: %s.\n"), get_err_mes(err));
            return err;
        }
        //eventを入力フレームを使用し終わったことの合図として登録する
        taskSurf->addClEvent(inputReleaseEvent);
        m_outQeueue.push_back(std::move(frame));
#endif
        return RGY_ERR_NONE;
    }
};

class PipelineTaskMPPEncode : public PipelineTask {
protected:
    static const int BUF_COUNT = 16;
    MPPContext *m_encoder;
    RGY_CODEC m_encCodec;
    MPPCfg& m_encParams;
    RGYTimecode *m_timecode;
    RGYTimestamp *m_encTimestamp;
    rgy_rational<int> m_outputTimebase;
    bool m_sentEOSFrame;
    MppBufferGroup m_frameGrp;
    struct MPPBufferPair {
        MppBuffer frame;
        MppBuffer pkt;
    };
    std::array<MPPBufferPair, BUF_COUNT> m_buffer;
    std::deque<MppBuffer> m_queueFrameList;
    RGYListRef<RGYBitstream> m_bitStreamOut;
    const RGYHDR10Plus *m_hdr10plus;
    const DOVIRpu *m_doviRpu;
    std::unique_ptr<RGYConvertCSP> m_convert;
public:
    PipelineTaskMPPEncode(
        MPPContext *enc, RGY_CODEC encCodec, MPPCfg& encParams, int outMaxQueueSize,
        RGYTimecode *timecode, RGYTimestamp *encTimestamp, rgy_rational<int> outputTimebase, const RGYHDR10Plus *hdr10plus, const DOVIRpu *doviRpu,
        int threadCsp, RGYParamThread threadParamCsp, std::shared_ptr<RGYLog> log)
        : PipelineTask(PipelineTaskType::MPPENC, outMaxQueueSize, log),
        m_encoder(enc), m_encCodec(encCodec), m_encParams(encParams), m_timecode(timecode), m_encTimestamp(encTimestamp), m_outputTimebase(outputTimebase),
        m_sentEOSFrame(false), m_frameGrp(nullptr), m_buffer(), m_queueFrameList(),
        m_bitStreamOut(), m_hdr10plus(hdr10plus), m_doviRpu(doviRpu), m_convert(std::make_unique<RGYConvertCSP>(threadCsp, threadParamCsp)) {
        for (auto& buf : m_buffer) {
            buf.frame = nullptr;
            buf.pkt = nullptr;
        }
    };
    virtual ~PipelineTaskMPPEncode() {
        m_outQeueue.clear();
        m_workSurfs.clear();
        m_queueFrameList.clear();
        for (auto& buf : m_buffer) {
            if (buf.frame) {
                mpp_buffer_put(buf.frame);
                buf.frame = nullptr;
            }
            if (buf.pkt) {
                mpp_buffer_put(buf.pkt);
                buf.pkt = nullptr;
            }
        }
        if (m_frameGrp) {
            mpp_buffer_group_put(m_frameGrp);
            m_frameGrp = nullptr;
        }
    };
    void setEnc(MPPContext *encode) { m_encoder = encode; };

    virtual std::optional<std::pair<RGYFrameInfo, int>> requiredSurfIn() override {
        return std::make_pair(m_encParams.frameinfo(), 8);
    }
    virtual std::optional<std::pair<RGYFrameInfo, int>> requiredSurfOut() override { return std::nullopt; };

    RGY_ERR allocateMppBuffer() {
        const int hor_stride = m_encParams.get<RK_S32>("prep:hor_stride");
        const int ver_stride = m_encParams.get<RK_S32>("prep:ver_stride");
        const int planeSize = ALIGN(hor_stride, 64) * ALIGN(ver_stride, 64);
        int frameSize = 0;
        const MppFrameFormat format = (MppFrameFormat)m_encParams.get<RK_S32>("prep:format");
        switch (format & MPP_FRAME_FMT_MASK) {
            case MPP_FMT_YUV420SP:
            case MPP_FMT_YUV420P: {
                frameSize = planeSize * 3 / 2;
            } break;
            default:
                PrintMes(RGY_LOG_ERROR, _T("Unspported colorspace.\n"));
                return RGY_ERR_UNSUPPORTED;
        }

        auto ret = err_to_rgy(mpp_buffer_group_get_internal(&m_frameGrp, MPP_BUFFER_TYPE_DRM));
        if (ret != RGY_ERR_NONE) {
            PrintMes(RGY_LOG_ERROR, _T("failed to get mpp buffer group : %s\n"), get_err_mes(ret));
            return ret;
        }

        for (auto& buf : m_buffer) {
            ret = err_to_rgy(mpp_buffer_get(m_frameGrp, &buf.frame, frameSize));
            if (ret != RGY_ERR_NONE) {
                PrintMes(RGY_LOG_ERROR, _T("failed to get buffer for input frame : %s\n"), get_err_mes(ret));
                return ret;
            }
            ret = err_to_rgy(mpp_buffer_get(m_frameGrp, &buf.pkt, frameSize));
            if (ret != RGY_ERR_NONE) {
                PrintMes(RGY_LOG_ERROR, _T("failed to get buffer for output packet : %s\n"), get_err_mes(ret));
                return ret;
            }
            m_queueFrameList.push_back(buf.frame);
        }
        return RGY_ERR_NONE;
    }

    std::tuple<RGY_ERR, std::shared_ptr<RGYBitstream>> getOutputBitstream() {
        MppPacket packet = nullptr;
        auto err = err_to_rgy(m_encoder->mpi->encode_get_packet(m_encoder->ctx, &packet));
        PrintMes(m_sentEOSFrame ? RGY_LOG_DEBUG : RGY_LOG_TRACE, _T("encode_get_packet: %s, %s.\n"), packet ? _T("yes") : _T("null"), get_err_mes(err));
        if (err == RGY_ERR_MPP_ERR_TIMEOUT || !packet) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            return { RGY_ERR_MORE_SURFACE, nullptr };
        } else if (err != RGY_ERR_NONE) {
            PrintMes(RGY_LOG_ERROR, _T("Failed to get packet from encoder: %s\n"), get_err_mes(err));
            return { err, nullptr };
        }

        auto output = m_bitStreamOut.get([](RGYBitstream *bs) {
            *bs = RGYBitstreamInit();
            return 0;
        });
        if (!output) {
            return { RGY_ERR_NULL_PTR, nullptr };
        }
        const bool eos = mpp_packet_get_eos(packet);
        if (eos) {
            PrintMes(RGY_LOG_DEBUG, _T("Got EOS packet.\n"));
        }
        const auto pktLength = mpp_packet_get_length(packet);
        if (pktLength == 0) {
            return { eos ? RGY_ERR_MORE_DATA : RGY_ERR_NONE, nullptr };
        }
        const auto pts = mpp_packet_get_pts(packet);
        const auto pktval = m_encTimestamp->get(pts);
        output->copy((uint8_t *)mpp_packet_get_pos(packet), pktLength, pts, 0, pktval.duration);

        if (mpp_packet_has_meta(packet)) {
            auto meta = mpp_packet_get_meta(packet);
            RK_S32 avg_qp = -1;
            if (mpp_meta_get_s32(meta, KEY_ENC_AVERAGE_QP, &avg_qp) == MPP_OK) {
                output->setAvgQP(avg_qp);
            }
            
            MppFrame frm = nullptr;
            if (mpp_meta_get_frame(meta, KEY_INPUT_FRAME, &frm) == MPP_OK) {
                if (m_frameGrp) { // ここで管理しているメモリなら、解放せずキューに戻す
                    MppBuffer frm_buf = mpp_frame_get_buffer(frm);
                    if (frm_buf) {
                        m_queueFrameList.push_back(frm_buf);
                    }
                }
                mpp_frame_deinit(&frm);
            }
        }
        mpp_packet_deinit(&packet);
        return { eos ? RGY_ERR_MORE_DATA : RGY_ERR_NONE, output };
    }

    virtual RGY_ERR sendFrame(std::unique_ptr<PipelineTaskOutput>& frame) override {
        if (frame && frame->type() != PipelineTaskOutputType::SURFACE) {
            PrintMes(RGY_LOG_ERROR, _T("Invalid frame type.\n"));
            return RGY_ERR_UNSUPPORTED;
        }

        std::vector<std::shared_ptr<RGYFrameData>> metadatalist;
        if (m_encCodec == RGY_CODEC_HEVC || m_encCodec == RGY_CODEC_AV1) {
            if (frame) {
                metadatalist = dynamic_cast<PipelineTaskOutputSurf *>(frame.get())->surf().frame()->dataList();
            }
            if (m_hdr10plus) {
                // 外部からHDR10+を読み込む場合、metadatalist 内のHDR10+の削除
                for (auto it = metadatalist.begin(); it != metadatalist.end(); ) {
                    if ((*it)->dataType() == RGY_FRAME_DATA_HDR10PLUS) {
                        it = metadatalist.erase(it);
                    } else {
                        it++;
                    }
                }
            }
            if (m_doviRpu) {
                // 外部からdoviを読み込む場合、metadatalist 内のdovi rpuの削除
                for (auto it = metadatalist.begin(); it != metadatalist.end(); ) {
                    if ((*it)->dataType() == RGY_FRAME_DATA_DOVIRPU) {
                        it = metadatalist.erase(it);
                    } else {
                        it++;
                    }
                }
            }
        }
#if 0
        if (!m_gotExtraData) { // 初回のみ取得する
            std::vector<char> extradata(16 * 1024, 0);
            MppPacket packet = nullptr;
            mpp_packet_init(&packet, extradata.data(), extradata.size());
            mpp_packet_set_length(packet, 0);

            auto err = err_to_rgy(m_encoder->mpi->control(m_encoder->ctx, MPP_ENC_GET_HDR_SYNC, packet));
            if (err != RGY_ERR_NONE) {
                PrintMes(RGY_LOG_ERROR, _T("Failed to get header: %s\n"), get_err_mes(err));
                return err;
            }

            void *ptr   = mpp_packet_get_pos(packet);
            size_t len  = mpp_packet_get_length(packet);

            m_extradata.resize(len);
            memcpy(m_extradata.data(), ptr, len);

            mpp_packet_deinit(&packet);
            m_gotExtraData = true;
        }
#endif

        bool mppframeeos = false;
        MppFrame mppframe = nullptr;
        if (!frame || !dynamic_cast<PipelineTaskOutputSurf *>(frame.get())->surf()) { // 終了フラグ
            auto err = err_to_rgy(mpp_frame_init(&mppframe));
            if (err != RGY_ERR_NONE) {
                PrintMes(RGY_LOG_ERROR, _T("Failed to allocate mpp frame: %s\n"), get_err_mes(err));
                return err;
            }
            mpp_frame_set_width(mppframe, m_encParams.get<RK_S32>("prep:width"));
            mpp_frame_set_height(mppframe, m_encParams.get<RK_S32>("prep:height"));
            mpp_frame_set_hor_stride(mppframe, m_encParams.get<RK_S32>("prep:hor_stride"));
            mpp_frame_set_ver_stride(mppframe, m_encParams.get<RK_S32>("prep:ver_stride"));
            mpp_frame_set_fmt(mppframe, (MppFrameFormat)m_encParams.get<RK_S32>("prep:format"));
            mpp_frame_set_buffer(mppframe, nullptr);
            mpp_frame_set_eos(mppframe, 1);
            mppframeeos = true;
            PrintMes(RGY_LOG_DEBUG, _T("Send EOS frame\n"));
        } else {
            auto& surfIn = dynamic_cast<PipelineTaskOutputSurf *>(frame.get())->surf();
            auto surfInFrame = surfIn.frame();
            if (m_timecode) {
                m_timecode->write(surfInFrame->timestamp(), m_outputTimebase);
            }
            m_encTimestamp->add(surfInFrame->timestamp(), surfInFrame->inputFrameId(), m_inFrames, surfInFrame->duration(), surfInFrame->dataList());
            //エンコーダまでたどり着いたフレームについてはdataListを解放
            surfInFrame->clearDataList();

            if (auto inFrameMpp = surfIn.mpp(); inFrameMpp != nullptr) {
                mppframe = inFrameMpp->releaseMpp(); // 中身がMppFrameの場合はそのまま使用する
                //メモリの確保方法が、エンコーダの設定と一致しているかを確認する
                RK_S32 enc_width = m_encParams.get<RK_S32>("prep:width");
                RK_S32 enc_height = m_encParams.get<RK_S32>("prep:height");
                RK_S32 enc_hor_stride = m_encParams.get<RK_S32>("prep:hor_stride");
                RK_S32 enc_ver_stride = m_encParams.get<RK_S32>("prep:ver_stride");
                MppFrameFormat enc_format = (MppFrameFormat)m_encParams.get<RK_S32>("prep:format");
                
                if (mpp_frame_get_width(mppframe)     != (RK_U32)enc_width
                || mpp_frame_get_height(mppframe)     != (RK_U32)enc_height
                || mpp_frame_get_hor_stride(mppframe) != (RK_U32)enc_hor_stride
                || mpp_frame_get_ver_stride(mppframe) != (RK_U32)enc_ver_stride
                || mpp_frame_get_fmt(mppframe)        != enc_format) {
                    m_encParams.set_s32("prep:width", mpp_frame_get_width(mppframe));
                    m_encParams.set_s32("prep:height", mpp_frame_get_height(mppframe));
                    m_encParams.set_s32("prep:hor_stride", mpp_frame_get_hor_stride(mppframe));
                    m_encParams.set_s32("prep:ver_stride", mpp_frame_get_ver_stride(mppframe));
                    m_encParams.set_s32("prep:format", (RK_S32)mpp_frame_get_fmt(mppframe));
                    
                    auto ret = m_encParams.apply(m_encParams.cfg);
                    if (ret != RGY_ERR_NONE) {
                        PrintMes(RGY_LOG_ERROR, _T("Failed to apply prep cfg changes: %s.\n"), get_err_mes(ret));
                        return ret;
                    }
                    ret = err_to_rgy(m_encoder->mpi->control(m_encoder->ctx, MPP_ENC_SET_CFG, m_encParams.cfg));
                    if (ret != RGY_ERR_NONE) {
                        PrintMes(RGY_LOG_ERROR, _T("Failed to reset prep cfg to encoder: %dx%d %s [%d:%d]: %s.\n"), 
                            m_encParams.get<RK_S32>("prep:width"), m_encParams.get<RK_S32>("prep:height"), 
                            RGY_CSP_NAMES[csp_enc_to_rgy((MppFrameFormat)m_encParams.get<RK_S32>("prep:format"))],
                            m_encParams.get<RK_S32>("prep:hor_stride"), m_encParams.get<RK_S32>("prep:ver_stride"), get_err_mes(ret));
                        return ret;
                    }
                    PrintMes(RGY_LOG_DEBUG, _T("Reset prep cfg to encoder: %dx%d %s [%d:%d].\n"),
                        m_encParams.get<RK_S32>("prep:width"), m_encParams.get<RK_S32>("prep:height"),
                        RGY_CSP_NAMES[csp_enc_to_rgy((MppFrameFormat)m_encParams.get<RK_S32>("prep:format"))],
                        m_encParams.get<RK_S32>("prep:hor_stride"), m_encParams.get<RK_S32>("prep:ver_stride"));
                }
            } else {
                auto err = err_to_rgy(mpp_frame_init(&mppframe));
                if (err != RGY_ERR_NONE) {
                    PrintMes(RGY_LOG_ERROR, _T("Failed to allocate mpp frame: %s\n"), get_err_mes(err));
                    return err;
                }
                mpp_frame_set_width(mppframe, m_encParams.get<RK_S32>("prep:width"));
                mpp_frame_set_height(mppframe, m_encParams.get<RK_S32>("prep:height"));
                mpp_frame_set_hor_stride(mppframe, m_encParams.get<RK_S32>("prep:hor_stride"));
                mpp_frame_set_ver_stride(mppframe, m_encParams.get<RK_S32>("prep:ver_stride"));
                mpp_frame_set_fmt(mppframe, (MppFrameFormat)m_encParams.get<RK_S32>("prep:format"));

                if (!m_frameGrp) {
                    auto err = allocateMppBuffer();
                    if (err != RGY_ERR_NONE) {
                        return err;
                    }
                }
                //明示的に待機が必要
                frame->depend_clear();

                if (m_queueFrameList.empty()) {
                    PrintMes(RGY_LOG_ERROR, _T("Not enough buffer!\n"));
                    return RGY_ERR_MPP_ERR_BUFFER_FULL;
                }
                auto framebuf = m_queueFrameList.front();
                m_queueFrameList.pop_front();
                mpp_frame_set_buffer(mppframe, framebuf);
                auto mppinfo = infoMPP(mppframe);
                RGYFrameInfo surfEncInInfo;
                // 入力がOpenCLのフレームの場合、mapされているはずのhost側のバッファを読み取る
                if (auto clframe = dynamic_cast<PipelineTaskOutputSurf *>(frame.get())->surf().cl(); clframe != nullptr) {
                    if (!clframe->isMapped()) {
                        PrintMes(RGY_LOG_ERROR, _T("Failed to get mapped buffer.\n"));
                        return RGY_ERR_UNKNOWN;
                    }
                    surfEncInInfo = clframe->mappedHost()->frameInfo();
                } else {
                    PrintMes(RGY_LOG_ERROR, _T("Unknown frame type!\n"));
                    return RGY_ERR_MPP_ERR_BUFFER_FULL;
                }
                if (m_convert->getFunc() == nullptr) {
                    if (auto func = m_convert->getFunc(surfEncInInfo.csp, mppinfo.csp, false, RGY_SIMD::SIMD_ALL); func == nullptr) {
                        PrintMes(RGY_LOG_ERROR, _T("Failed to find conversion for %s -> %s.\n"),
                            RGY_CSP_NAMES[surfEncInInfo.csp], RGY_CSP_NAMES[mppinfo.csp]);
                        return RGY_ERR_UNSUPPORTED;
                    } else {
                        PrintMes(RGY_LOG_DEBUG, _T("Selected conversion for %s -> %s [%s].\n"),
                            RGY_CSP_NAMES[func->csp_from], RGY_CSP_NAMES[func->csp_to], get_simd_str(func->simd));
                    }
                }
                auto crop = initCrop();
                m_convert->run((mppinfo.picstruct & RGY_PICSTRUCT_INTERLACED) ? 1 : 0,
                    (void **)mppinfo.ptr, (const void **)surfEncInInfo.ptr,
                    surfEncInInfo.width, surfEncInInfo.pitch[0], surfEncInInfo.pitch[1], mppinfo.pitch[0], mppinfo.pitch[1],
                    surfEncInInfo.height, mppinfo.height, crop.c);
                    
                if (auto clframe = dynamic_cast<PipelineTaskOutputSurf *>(frame.get())->surf().cl(); clframe != nullptr) {
                    clframe->unmapBuffer();
                    clframe->resetMappedFrame();
                }
                mpp_frame_set_pts(mppframe, surfEncInInfo.timestamp);
            }
            mpp_frame_set_poc(mppframe, m_inFrames);
            m_inFrames++;
            PrintMes(RGY_LOG_TRACE, _T("m_inFrames %d.\n"), m_inFrames);
        }

        //MppPacket packet = nullptr;
        //mpp_packet_init_with_buffer(&packet, m_pktBuf);
        //mpp_packet_set_length(packet, 0); //It is important to clear output packet length!!

        //auto meta = mpp_frame_get_meta(mppframe);
        //mpp_meta_set_packet(meta, KEY_OUTPUT_PACKET, packet);

        auto err = RGY_ERR_NONE;
        bool sendFrame = false;
        do {
            //エンコーダからの取り出し
            PrintMes(RGY_LOG_TRACE, _T("getOutputBitstream %d.\n"), m_inFrames);
            auto [out_ret, outBs] = getOutputBitstream();
            if (out_ret == RGY_ERR_MORE_SURFACE) {
                err = out_ret; // もっとエンコーダへの投入が必要
            } else if (out_ret == RGY_ERR_NONE || out_ret == RGY_ERR_MORE_DATA) {
                if (outBs && outBs->size() > 0) {
                    m_outQeueue.push_back(std::make_unique<PipelineTaskOutputBitstream>(outBs));
                }
                if (out_ret == RGY_ERR_MORE_DATA) { //EOF
                    err = RGY_ERR_MORE_DATA;
                    break;
                }
            } else {
                err = out_ret; // エラー
                break;
            }

            if (!sendFrame) {
                err = err_to_rgy(m_encoder->mpi->encode_put_frame(m_encoder->ctx, mppframe));
                PrintMes(mppframeeos ? RGY_LOG_DEBUG : RGY_LOG_TRACE, _T("encode_put_frame%s %d: %s.\n"), mppframeeos ? _T("(eos)") : _T(""), m_inFrames, get_err_mes(err));
                if (err == RGY_ERR_NONE) {
                    sendFrame = true;
                    if (mppframeeos) {
                        m_sentEOSFrame = true;
                    }
                } else {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));}
                }
        } while (!sendFrame || err != RGY_ERR_NONE);
        return err;
    }
};

class PipelineTaskRGA : public PipelineTask {
protected:
    std::vector<std::unique_ptr<RGAFilter>>& m_vpFilters;
    std::shared_ptr<RGYOpenCLContext> m_cl;
    std::unique_ptr<RGYConvertCSP> m_convert;
    std::unique_ptr<RGYFrameMpp> m_inputFrameTmp;
    int m_vppOutFrames;
    std::unordered_map<int64_t, std::vector<std::shared_ptr<RGYFrameData>>> m_metadatalist;
    std::deque<std::unique_ptr<PipelineTaskOutput>> m_prevInputFrame; //前回投入されたフレーム、完了通知を待ってから解放するため、参照を保持する
public:
    PipelineTaskRGA(std::vector<std::unique_ptr<RGAFilter>>& vppfilter, std::shared_ptr<RGYOpenCLContext>& cl, int threadCsp, RGYParamThread threadParamCsp, int outMaxQueueSize, std::shared_ptr<RGYLog> log) :
        PipelineTask(PipelineTaskType::MPPVPP, outMaxQueueSize, log), m_vpFilters(vppfilter), m_cl(cl),
        m_convert(std::make_unique<RGYConvertCSP>(threadCsp, threadParamCsp)), m_inputFrameTmp(), m_vppOutFrames(), m_metadatalist(), m_prevInputFrame() {

    };
    virtual ~PipelineTaskRGA() {
        m_prevInputFrame.clear();
    };

    virtual std::optional<std::pair<RGYFrameInfo, int>> requiredSurfIn() override {
        return std::make_pair(m_vpFilters.front()->GetFilterParam()->frameIn, 0);
    };
    virtual std::optional<std::pair<RGYFrameInfo, int>> requiredSurfOut() override {
        return std::make_pair(m_vpFilters.back()->GetFilterParam()->frameOut, 0);
    };
    virtual RGY_ERR sendFrame(std::unique_ptr<PipelineTaskOutput>& frame) override {
        if (m_prevInputFrame.size() > 0) {
            //前回投入したフレームの処理が完了していることを確認したうえで参照を破棄することでロックを解放する
            auto prevframe = std::move(m_prevInputFrame.front());
            m_prevInputFrame.pop_front();
            prevframe->depend_clear();
        }

        std::deque<std::pair<RGYFrameMpp*, uint32_t>> filterframes;
        bool drain = !frame;
        if (!frame) {
            filterframes.push_back(std::make_pair(nullptr, 0u));
        } else {
            auto taskSurf = dynamic_cast<PipelineTaskOutputSurf *>(frame.get());
            if (taskSurf == nullptr) {
                PrintMes(RGY_LOG_ERROR, _T("Invalid task surface.\n"));
                return RGY_ERR_NULL_PTR;
            }
            if (auto surfVppInMpp = taskSurf->surf().mpp(); surfVppInMpp != nullptr) {
                filterframes.push_back(std::make_pair(surfVppInMpp, 0u));
            } else if (auto surfVppInCL = taskSurf->surf().cl(); surfVppInCL != nullptr) {
                // 入力がOpenCLのフレームの場合、mapされているはずのhost側のバッファを読み取る
                if (!surfVppInCL->isMapped()) {
                    PrintMes(RGY_LOG_ERROR, _T("Failed to get mapped buffer.\n"));
                    return RGY_ERR_UNKNOWN;
                }
                const auto mappedHost = surfVppInCL->mappedHost()->frameInfo();
                if (!m_inputFrameTmp) {
                    m_inputFrameTmp = std::make_unique<RGYFrameMpp>();
                    if (!m_frameGrp) {
                        auto sts = err_to_rgy(mpp_buffer_group_get_internal(&m_frameGrp, MPP_BUFFER_TYPE_DRM));
                        if (sts != RGY_ERR_NONE) {
                            PrintMes(RGY_LOG_ERROR, _T("failed to get mpp buffer group : %s\n"), get_err_mes(sts));
                            return sts;
                        }
                    }
                    auto err = m_inputFrameTmp->allocate(mappedHost, m_frameGrp);
                    if (err != RGY_ERR_NONE) {
                        PrintMes(RGY_LOG_ERROR, _T("Failed to allocate input buffer: %s.\n"), get_err_mes(err));
                        return err;
                    }
                }
                if (m_convert->getFunc() == nullptr) {
                    if (auto func = m_convert->getFunc(mappedHost.csp, m_inputFrameTmp->csp(), false, RGY_SIMD::SIMD_ALL); func == nullptr) {
                        PrintMes(RGY_LOG_ERROR, _T("Failed to find conversion for %s -> %s.\n"),
                            RGY_CSP_NAMES[mappedHost.csp], RGY_CSP_NAMES[m_inputFrameTmp->csp()]);
                        return RGY_ERR_UNSUPPORTED;
                    } else {
                        PrintMes(RGY_LOG_DEBUG, _T("Selected conversion for %s -> %s [%s].\n"),
                            RGY_CSP_NAMES[func->csp_from], RGY_CSP_NAMES[func->csp_to], get_simd_str(func->simd));
                    }
                }
                auto crop = initCrop();
                m_convert->run((mappedHost.picstruct & RGY_PICSTRUCT_INTERLACED) ? 1 : 0,
                    (void **)m_inputFrameTmp->ptr().data(), (const void **)mappedHost.ptr,
                    mappedHost.width, mappedHost.pitch[0], mappedHost.pitch[1], m_inputFrameTmp->pitch(RGY_PLANE_Y), m_inputFrameTmp->pitch(RGY_PLANE_C),
                    mappedHost.height, m_inputFrameTmp->height(), crop.c);

                m_inputFrameTmp->setPropertyFrom(surfVppInCL);
                surfVppInCL->unmapBuffer();
                surfVppInCL->resetMappedFrame();
                filterframes.push_back(std::make_pair(m_inputFrameTmp.get(), 0u));
            } else {
                PrintMes(RGY_LOG_ERROR, _T("Invalid task surface (not mpp).\n"));
                return RGY_ERR_NULL_PTR;
            }
            //ここでinput frameの参照を m_prevInputFrame で保持するようにして、OpenCLによるフレームの処理が完了しているかを確認できるようにする
            //これを行わないとこのフレームが再度使われてしまうことになる
            m_prevInputFrame.push_back(std::move(frame));
        }
        int rga_sync = 0;
        std::vector<std::unique_ptr<PipelineTaskOutputSurf>> outputSurfs;
        while (filterframes.size() > 0 || drain) {
            //フィルタリングするならここ
            for (uint32_t ifilter = filterframes.front().second; ifilter < m_vpFilters.size() - 1; ifilter++) {
                int nOutFrames = 0;
                RGYFrameMpp *outInfo[16] = { 0 };
                auto sts_filter = m_vpFilters[ifilter]->filter_rga(filterframes.front().first, (RGYFrameMpp **)&outInfo, &nOutFrames, &rga_sync);
                if (sts_filter != RGY_ERR_NONE) {
                    PrintMes(RGY_LOG_ERROR, _T("Error while running filter \"%s\".\n"), m_vpFilters[ifilter]->name().c_str());
                    return sts_filter;
                }
                if (nOutFrames == 0) {
                    if (drain) {
                        filterframes.front().second++;
                        continue;
                    }
                    return RGY_ERR_NONE;
                }
                drain = false; //途中でフレームが出てきたら、drain完了していない

                // 上書きするタイプのフィルタの場合、pop_front -> push_front は不要
                if (m_vpFilters[ifilter]->GetFilterParam()->bOutOverwrite
                    && filterframes.front().first == outInfo[0]) {
                    // 上書きするタイプのフィルタが複数のフレームを返すのはサポートしない
                    if (nOutFrames > 1) {
                        PrintMes(RGY_LOG_ERROR, _T("bOutOverwrite = true but nOutFrames = %d at filter[%d][%s].\n"),
                            nOutFrames, ifilter, m_vpFilters[ifilter]->name().c_str());
                        return RGY_ERR_UNSUPPORTED;
                    }
                } else {
                    filterframes.pop_front();
                    //最初に出てきたフレームは先頭に追加する
                    for (int jframe = nOutFrames - 1; jframe >= 0; jframe--) {
                        filterframes.push_front(std::make_pair(outInfo[jframe], ifilter + 1));
                    }
                }
            }
            if (drain) {
                return RGY_ERR_MORE_DATA; //最後までdrain = trueなら、drain完了
            }
            
            //最後のフィルタ
            auto &lastFilter = m_vpFilters[m_vpFilters.size() - 1];
            auto surfVppOut = getNewWorkSurfMpp(lastFilter->GetFilterParam()->frameOut);
            if (surfVppOut == nullptr) {
                PrintMes(RGY_LOG_ERROR, _T("failed to get work surface for input.\n"));
                return RGY_ERR_NOT_ENOUGH_BUFFER;
            }
            //エンコードバッファのポインタを渡す
            int nOutFrames = 0;
            RGYFrameMpp *outInfo[1];
            outInfo[0] = surfVppOut.get();
            auto sts_filter = lastFilter->filter_rga(filterframes.front().first, (RGYFrameMpp **)&outInfo, &nOutFrames, &rga_sync);
            if (sts_filter != RGY_ERR_NONE) {
                PrintMes(RGY_LOG_ERROR, _T("Error while running filter \"%s\".\n"), lastFilter->name().c_str());
                return sts_filter;
            }
            filterframes.pop_front();

            auto outputSurf = std::make_unique<PipelineTaskOutputSurf>(m_workSurfs.addSurface(surfVppOut));
            outputSurf->setRGASync(rga_sync);
            outputSurfs.push_back(std::move(outputSurf));
        }
        m_outQeueue.insert(m_outQeueue.end(),
            std::make_move_iterator(outputSurfs.begin()),
            std::make_move_iterator(outputSurfs.end())
        );
        return RGY_ERR_NONE;
    }
};

class PipelineTaskIEP : public PipelineTask {
protected:
    std::vector<std::unique_ptr<RGAFilter>>& m_vpFilters;
    std::shared_ptr<RGYOpenCLContext> m_cl;
    std::unique_ptr<RGYConvertCSP> m_convert;
    RGYFrameInfo m_inputFrameInfo;
    int m_stride_x, m_stride_y;
    int m_vppOutFrames;
    std::unordered_map<int64_t, std::vector<std::shared_ptr<RGYFrameData>>> m_metadatalist;
    std::deque<std::unique_ptr<PipelineTaskOutput>> m_prevInputFrame; //前回投入されたフレーム、完了通知を待ってから解放するため、参照を保持する
public:
    PipelineTaskIEP(std::vector<std::unique_ptr<RGAFilter>>& vppfilter, std::shared_ptr<RGYOpenCLContext>& cl, int threadCsp, RGYParamThread threadParamCsp, int outMaxQueueSize, std::shared_ptr<RGYLog> log) :
        PipelineTask(PipelineTaskType::MPPIEP, outMaxQueueSize, log), m_vpFilters(vppfilter), m_cl(cl),
        m_convert(std::make_unique<RGYConvertCSP>(threadCsp, threadParamCsp)), m_inputFrameInfo(), m_stride_x(0), m_stride_y(0), m_vppOutFrames(), m_metadatalist(), m_prevInputFrame() {

    };
    virtual ~PipelineTaskIEP() {
        m_prevInputFrame.clear();
    };

    virtual std::optional<std::pair<RGYFrameInfo, int>> requiredSurfIn() override {
        return std::make_pair(m_vpFilters.front()->GetFilterParam()->frameIn, 1);
    };
    virtual std::optional<std::pair<RGYFrameInfo, int>> requiredSurfOut() override {
        return std::make_pair(m_vpFilters.back()->GetFilterParam()->frameOut, m_outMaxQueueSize + 4);
    };
    virtual RGY_ERR sendFrame(std::unique_ptr<PipelineTaskOutput>& frame) override {
        if (m_prevInputFrame.size() > 0) {
            //前回投入したフレームの処理が完了していることを確認したうえで参照を破棄することでロックを解放する
            auto prevframe = std::move(m_prevInputFrame.front());
            m_prevInputFrame.pop_front();
            prevframe->depend_clear();
        }

        if (m_vpFilters.size() > 1) {
            PrintMes(RGY_LOG_ERROR, _T("PipelineTaskIEP supports only one filter per block.\n"));
            return RGY_ERR_NULL_PTR;
        }

        RGYCLFrame *surfVppInCL = nullptr;
        std::deque<std::pair<RGYFrameMpp*, uint32_t>> filterframes;
        auto inputFrameTmp = std::make_unique<RGYFrameMpp>();
        bool drain = !frame;
        if (!frame) {
            filterframes.push_back(std::make_pair(nullptr, 0u));
        } else {
            auto taskSurf = dynamic_cast<PipelineTaskOutputSurf *>(frame.get());
            if (taskSurf == nullptr) {
                PrintMes(RGY_LOG_ERROR, _T("Invalid task surface.\n"));
                return RGY_ERR_NULL_PTR;
            }
            if (auto surfVppInMpp = taskSurf->surf().mpp(); surfVppInMpp != nullptr) {
                filterframes.push_back(std::make_pair(surfVppInMpp, 0u));
            } else if (surfVppInCL = taskSurf->surf().cl(); surfVppInCL != nullptr) {
                // 入力がOpenCLのフレームの場合、mapされているはずのhost側のバッファを読み取る
                if (!surfVppInCL->isMapped()) {
                    PrintMes(RGY_LOG_ERROR, _T("Failed to get mapped buffer.\n"));
                    return RGY_ERR_UNKNOWN;
                }
                const auto mappedHost = surfVppInCL->mappedHost()->frameInfo();
                if (!m_frameGrp) {
                    auto sts = err_to_rgy(mpp_buffer_group_get_internal(&m_frameGrp, MPP_BUFFER_TYPE_DRM));
                    if (sts != RGY_ERR_NONE) {
                        PrintMes(RGY_LOG_ERROR, _T("failed to get mpp buffer group : %s\n"), get_err_mes(sts));
                        return sts;
                    }
                }
                auto err = inputFrameTmp->allocate(mappedHost, m_frameGrp);
                if (err != RGY_ERR_NONE) {
                    PrintMes(RGY_LOG_ERROR, _T("Failed to allocate input buffer: %s.\n"), get_err_mes(err));
                    return err;
                }
                if (m_convert->getFunc() == nullptr) {
                    if (auto func = m_convert->getFunc(mappedHost.csp, inputFrameTmp->csp(), false, RGY_SIMD::SIMD_ALL); func == nullptr) {
                        PrintMes(RGY_LOG_ERROR, _T("Failed to find conversion for %s -> %s.\n"),
                            RGY_CSP_NAMES[mappedHost.csp], RGY_CSP_NAMES[inputFrameTmp->csp()]);
                        return RGY_ERR_UNSUPPORTED;
                    } else {
                        PrintMes(RGY_LOG_DEBUG, _T("Selected conversion for %s -> %s [%s].\n"),
                            RGY_CSP_NAMES[func->csp_from], RGY_CSP_NAMES[func->csp_to], get_simd_str(func->simd));
                    }
                }
                auto crop = initCrop();
                m_convert->run((mappedHost.picstruct & RGY_PICSTRUCT_INTERLACED) ? 1 : 0,
                    (void **)inputFrameTmp->ptr().data(), (const void **)mappedHost.ptr,
                    mappedHost.width, mappedHost.pitch[0], mappedHost.pitch[1], inputFrameTmp->pitch(RGY_PLANE_Y), inputFrameTmp->pitch(RGY_PLANE_C),
                    inputFrameTmp->height(), inputFrameTmp->height(), crop.c);

                inputFrameTmp->setPropertyFrom(surfVppInCL);
                surfVppInCL->unmapBuffer();
                surfVppInCL->resetMappedFrame();
                filterframes.push_back(std::make_pair(inputFrameTmp.get(), 0u));
            } else {
                PrintMes(RGY_LOG_ERROR, _T("Invalid task surface (not mpp).\n"));
                return RGY_ERR_NULL_PTR;
            }
            //ここでinput frameの参照を m_prevInputFrame で保持するようにして、OpenCLによるフレームの処理が完了しているかを確認できるようにする
            //これを行わないとこのフレームが再度使われてしまうことになる
            m_prevInputFrame.push_back(std::move(frame));
            m_inputFrameInfo = filterframes.front().first->getInfoCopy();
            m_stride_x = filterframes.front().first->x_stride();
            m_stride_y = filterframes.front().first->y_stride();
        }


        //エンコードバッファのポインタを渡す
        int nOutFrames = 1;
        RGYFrameMpp *ptrOutInfo[2] = { 0 }; // 最大で2フレーム出力がある
        std::vector<std::unique_ptr<RGYFrameMpp>> surfOut;
        for (size_t i = 0; i < _countof(ptrOutInfo); i++) {
            auto surfVppOut = getNewWorkSurfMpp(m_inputFrameInfo, m_stride_x, m_stride_y); // IEPはx_stride, y_strideが入出力で同じでないとおかしなことになる
            if (surfVppOut == nullptr) {
                PrintMes(RGY_LOG_ERROR, _T("failed to get work surface for input.\n"));
                return RGY_ERR_NOT_ENOUGH_BUFFER;
            }
            ptrOutInfo[i] = surfVppOut.get();
            surfOut.push_back(std::move(surfVppOut));
        }
        unique_event sync = unique_event(nullptr, CloseEvent);
        int dummy = 0;
        auto sts_filter = m_vpFilters.front()->filter_iep(filterframes.front().first, (RGYFrameMpp **)&ptrOutInfo, &nOutFrames, sync);
        if (sts_filter != RGY_ERR_NONE) {
            PrintMes(RGY_LOG_ERROR, _T("Error while running filter \"%s\".\n"), m_vpFilters.front()->name().c_str());
            return sts_filter;
        }
        filterframes.pop_front();
        if (drain && nOutFrames == 0) {
            return RGY_ERR_MORE_DATA; //最後までdrain = trueなら、drain完了
        }

        //WaitForSingleObject(sync.get(), INFINITE);

        for (int i = 0; i < nOutFrames; i++) {
            auto outSurf = std::make_unique<PipelineTaskOutputSurf>(m_workSurfs.addSurface(surfOut[i]));
            outSurf->addEvent(sync);
            m_outQeueue.push_back(std::move(outSurf));
        }
        return RGY_ERR_NONE;
    }
};

class PipelineTaskOpenCL : public PipelineTask {
protected:
    std::shared_ptr<RGYOpenCLContext> m_cl;
    std::vector<std::unique_ptr<RGYFilter>>& m_vpFilters;
    std::deque<std::unique_ptr<PipelineTaskOutput>> m_prevInputFrame; //前回投入されたフレーム、完了通知を待ってから解放するため、参照を保持する
    RGYFilterSsim *m_videoMetric;
    std::unique_ptr<RGYCLFrame> m_clFrameInput;
    std::unique_ptr<RGYCLFrame> m_clFrameOutput;
public:
    PipelineTaskOpenCL(std::vector<std::unique_ptr<RGYFilter>>& vppfilters, RGYFilterSsim *videoMetric, std::shared_ptr<RGYOpenCLContext> cl, int outMaxQueueSize, std::shared_ptr<RGYLog> log) :
        PipelineTask(PipelineTaskType::OPENCL, outMaxQueueSize, log), m_cl(cl), m_vpFilters(vppfilters), m_prevInputFrame(), m_videoMetric(videoMetric), m_clFrameInput(), m_clFrameOutput() {

    };
    virtual ~PipelineTaskOpenCL() {
        m_clFrameInput.reset();
        m_clFrameOutput.reset();
        m_prevInputFrame.clear();
        m_cl.reset();
    };

    void setVideoQualityMetricFilter(RGYFilterSsim *videoMetric) {
        m_videoMetric = videoMetric;
    }

    virtual std::optional<std::pair<RGYFrameInfo, int>> requiredSurfIn() override { return std::nullopt; };
    virtual std::optional<std::pair<RGYFrameInfo, int>> requiredSurfOut() override { return std::nullopt; };
    virtual RGY_ERR sendFrame(std::unique_ptr<PipelineTaskOutput>& frame) override {
        if (m_prevInputFrame.size() > 0) {
            //前回投入したフレームの処理が完了していることを確認したうえで参照を破棄することでロックを解放する
            auto prevframe = std::move(m_prevInputFrame.front());
            m_prevInputFrame.pop_front();
            prevframe->depend_clear();
        }

        std::deque<std::pair<RGYFrameInfo, uint32_t>> filterframes;
        bool drain = !frame;
        if (!frame) {
            filterframes.push_back(std::make_pair(RGYFrameInfo(), 0u));
        } else {
            auto taskSurf = dynamic_cast<PipelineTaskOutputSurf *>(frame.get());
            if (taskSurf == nullptr) {
                PrintMes(RGY_LOG_ERROR, _T("Invalid task surface.\n"));
                return RGY_ERR_NULL_PTR;
            }
            if (auto surfVppInMpp = taskSurf->surf().mpp(); surfVppInMpp != nullptr) {
                auto mppInInfoCopy = surfVppInMpp->getInfoCopy();
                if (!m_clFrameInput) {
                    m_clFrameInput = m_cl->createFrameBuffer(mppInInfoCopy);
                    if (!m_clFrameInput) {
                        PrintMes(RGY_LOG_ERROR, _T("Failed to allocate OpenCL input buffer.\n"));
                        return RGY_ERR_NULL_PTR;
                    }
                }
                RGYOpenCLEvent clevent;
                if (auto err = m_cl->copyFrame(&m_clFrameInput->frame, &mppInInfoCopy, nullptr, m_cl->queue(), &clevent); err != RGY_ERR_NONE) {
                    PrintMes(RGY_LOG_ERROR, _T("Failed to copy frame to OpenCL input buffer.\n"));
                    return RGY_ERR_NULL_PTR;
                }
                // taskSurfをm_prevInputFrameに登録するが、そのときに待機すべきイベントとして、ここのcopyFrameのイベントを登録する
                taskSurf->addClEvent(clevent);
                filterframes.push_back(std::make_pair(m_clFrameInput->frameInfo(), 0u));
            } else if (auto surfVppInCL = taskSurf->surf().cl(); surfVppInCL != nullptr) {
                filterframes.push_back(std::make_pair(surfVppInCL->frameInfo(), 0u));
            } else {
                PrintMes(RGY_LOG_ERROR, _T("Invalid task surface (not opencl or mpp).\n"));
                return RGY_ERR_NULL_PTR;
            }
            //ここでinput frameの参照を m_prevInputFrame で保持するようにして、OpenCLによるフレームの処理が完了しているかを確認できるようにする
            //これを行わないとこのフレームが再度使われてしまうことになる
            m_prevInputFrame.push_back(std::move(frame));
        }

        std::vector<std::unique_ptr<PipelineTaskOutputSurf>> outputSurfs;
        while (filterframes.size() > 0 || drain) {
            //フィルタリングするならここ
            for (uint32_t ifilter = filterframes.front().second; ifilter < m_vpFilters.size() - 1; ifilter++) {
                // コピーを作ってそれをfilter関数に渡す
                // vpp-rffなどoverwirteするフィルタのときに、filterframes.pop_front -> push がうまく動作しない
                RGYFrameInfo input = filterframes.front().first;

                int nOutFrames = 0;
                RGYFrameInfo *outInfo[16] = { 0 };
                auto sts_filter = m_vpFilters[ifilter]->filter(&input, (RGYFrameInfo **)&outInfo, &nOutFrames);
                if (sts_filter != RGY_ERR_NONE) {
                    PrintMes(RGY_LOG_ERROR, _T("Error while running filter \"%s\".\n"), m_vpFilters[ifilter]->name().c_str());
                    return sts_filter;
                }
                if (nOutFrames == 0) {
                    if (drain) {
                        filterframes.front().second++;
                        continue;
                    }
                    return RGY_ERR_NONE;
                }
                drain = false; //途中でフレームが出てきたら、drain完了していない

                filterframes.pop_front();
                //最初に出てきたフレームは先頭に追加する
                for (int jframe = nOutFrames - 1; jframe >= 0; jframe--) {
                    filterframes.push_front(std::make_pair(*outInfo[jframe], ifilter + 1));
                }
            }
            if (drain) {
                return RGY_ERR_MORE_DATA; //最後までdrain = trueなら、drain完了
            }
            
            //最後のフィルタ
            auto surfVppOut = getWorkSurf();
            if (surfVppOut == nullptr) {
                PrintMes(RGY_LOG_ERROR, _T("failed to get work surface for input.\n"));
                return RGY_ERR_NOT_ENOUGH_BUFFER;
            }
            if (!surfVppOut.cl()) {
                PrintMes(RGY_LOG_ERROR, _T("Unexpected surface type for output.\n"));
                return RGY_ERR_NOT_ENOUGH_BUFFER;
            }
            auto surfVppOutInfo = surfVppOut.cl()->frameInfo();
            auto &lastFilter = m_vpFilters[m_vpFilters.size() - 1];
            //最後のフィルタはRGYFilterCspCropでなければならない
            if (typeid(*lastFilter.get()) != typeid(RGYFilterCspCrop)) {
                PrintMes(RGY_LOG_ERROR, _T("Last filter setting invalid.\n"));
                return RGY_ERR_INVALID_PARAM;
            }
            //エンコードバッファのポインタを渡す
            int nOutFrames = 0;
            RGYFrameInfo *outInfo[1];
            outInfo[0] = &surfVppOutInfo;
            auto sts_filter = lastFilter->filter(&filterframes.front().first, (RGYFrameInfo **)&outInfo, &nOutFrames, m_cl->queue());
            if (sts_filter != RGY_ERR_NONE) {
                PrintMes(RGY_LOG_ERROR, _T("Error while running filter \"%s\".\n"), lastFilter->name().c_str());
                return sts_filter;
            }
            if (m_videoMetric) {
                //フレームを転送
                int dummy = 0;
                auto err = m_videoMetric->filter(&filterframes.front().first, nullptr, &dummy, m_cl->queue());
                if (err != RGY_ERR_NONE) {
                    PrintMes(RGY_LOG_ERROR, _T("Failed to send frame for video metric calcualtion: %s.\n"), get_err_mes(err));
                    return err;
                }
            }
            filterframes.pop_front();

            if (true) { // surfVppOutInfo に設定された情報をqueueMapBufferを呼ぶ前に設定する必要がある
                surfVppOut.frame()->setDuration(surfVppOutInfo.duration);
                surfVppOut.frame()->setTimestamp(surfVppOutInfo.timestamp);
                surfVppOut.frame()->setInputFrameId(surfVppOutInfo.inputFrameId);
                surfVppOut.frame()->setPicstruct(surfVppOutInfo.picstruct);
                surfVppOut.frame()->setFlags(surfVppOutInfo.flags);
                surfVppOut.frame()->setDataList(surfVppOutInfo.dataList);
            }

            auto err = surfVppOut.cl()->queueMapBuffer(m_cl->queue(), CL_MAP_READ); // CPUが読み込むためにMapする
            if (err != RGY_ERR_NONE) {
                PrintMes(RGY_LOG_ERROR, _T("Failed to map buffer: %s.\n"), get_err_mes(err));
                return err;
            }
            PrintMes(RGY_LOG_TRACE, _T("out frame: 0x%08x, %10lld, %10lld.\n"), surfVppOut.cl(), surfVppOut.frame()->timestamp(), surfVppOut.cl()->mappedHost()->timestamp());

            // frameの依存関係の登録は、このステップの最終出力フレームに登録するようにして、使用中に解放されないようにする
            // なので、filterframes.empty()で最終フレームになっているか確認する
            // こうしないと、複数のフレームを出力する場合にフレーム内容が途中で上書きされておかしくなる
            auto outputSurf = (filterframes.empty()) ? std::make_unique<PipelineTaskOutputSurf>(surfVppOut, frame)
                                                     : std::make_unique<PipelineTaskOutputSurf>(surfVppOut);
            // outputSurf が待機すべきeventとして、queueMapBufferのeventを登録する
            for (auto& event : surfVppOut.cl()->mapEvents()) {
                outputSurf->addClEvent(event);
            }
            outputSurfs.push_back(std::move(outputSurf));

            #undef clFrameOutInteropRelease
        }
        m_outQeueue.insert(m_outQeueue.end(),
            std::make_move_iterator(outputSurfs.begin()),
            std::make_move_iterator(outputSurfs.end())
        );
        return RGY_ERR_NONE;
    }
};

#endif //__MPP_PIPELINE_H__

