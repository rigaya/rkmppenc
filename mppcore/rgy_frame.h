﻿// -----------------------------------------------------------------------------------------
// QSVEnc/NVEnc by rigaya
// -----------------------------------------------------------------------------------------
// The MIT License
//
// Copyright (c) 2020 rigaya
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
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// --------------------------------------------------------------------------------------------

#pragma once
#ifndef __RGY_FRAME_H__
#define __RGY_FRAME_H__

#include <memory>
#include <array>
#include "rgy_version.h"
#include "rgy_err.h"
#include "rgy_def.h"
#include "convert_csp.h"
#include "rgy_frame_info.h"
#if ENABLE_VPP_SMOOTH_QP_FRAME
#include "rgy_cuda_util.h"
#endif //#if ENABLE_VPP_SMOOTH_QP_FRAME

class RGYLog;

enum RGYFrameDataType {
    RGY_FRAME_DATA_NONE,
    RGY_FRAME_DATA_QP,
    RGY_FRAME_DATA_METADATA,
    RGY_FRAME_DATA_HDR10PLUS,
    RGY_FRAME_DATA_DOVIRPU,

    RGY_FRAME_DATA_MAX,
};

const TCHAR *RGYFrameDataTypeToStr(const RGYFrameDataType type);

class RGYFrameData {
public:
    RGYFrameData() : m_dataType(RGY_FRAME_DATA_NONE) {};
    virtual ~RGYFrameData() {};
    RGYFrameDataType dataType() const { return m_dataType; }
protected:
    RGYFrameDataType m_dataType;
};

struct CUFrameBuf;

class RGYFrameDataQP : public RGYFrameData {
public:
    RGYFrameDataQP();
    virtual ~RGYFrameDataQP();
    RGY_ERR setQPTable(const int8_t *qpTable, int qpw, int qph, int qppitch, int scaleType, int frameType, int64_t timestamp);
#if ENABLE_VPP_SMOOTH_QP_FRAME
    RGY_ERR transferToGPU(cudaStream_t stream);
#endif //#if ENABLE_VPP_SMOOTH_QP_FRAME
    int frameType() const { return m_frameType; }
    int qpScaleType() const { return m_qpScaleType; }
#if ENABLE_VPP_SMOOTH_QP_FRAME
    cudaEvent_t event() { return *m_event.get(); }
    CUFrameBuf *qpDev() { return m_qpDev.get(); }
#endif //#if ENABLE_VPP_SMOOTH_QP_FRAME
protected:
    int m_frameType;
    int m_qpScaleType;
#if ENABLE_VPP_SMOOTH_QP_FRAME
    std::unique_ptr<CUFrameBuf> m_qpDev;
    std::unique_ptr<cudaEvent_t, cudaevent_deleter> m_event;
#endif //#if ENABLE_VPP_SMOOTH_QP_FRAME
    RGYFrameInfo m_qpHost;
};

class RGYFrameDataMetadataConvertParam {
    RGYDOVIRpuConvertParam dovirpu;
public:
    RGYFrameDataMetadataConvertParam() : dovirpu() {};
    RGYFrameDataMetadataConvertParam(const RGYDOVIRpuConvertParam& dovirpu_) : dovirpu(dovirpu_) {};
    virtual ~RGYFrameDataMetadataConvertParam() {};

    const RGYDOVIRpuConvertParam *doviRpu() const { return &dovirpu; }
};

class RGYFrameDataMetadata : public RGYFrameData {
public:
    RGYFrameDataMetadata();
    RGYFrameDataMetadata(const uint8_t* data, size_t size, int64_t timestamp);
    virtual ~RGYFrameDataMetadata();

    virtual RGY_ERR convert([[maybe_unused]] const RGYFrameDataMetadataConvertParam *prm, [[maybe_unused]] RGYLog *log) { return RGY_ERR_NONE; }
    virtual std::vector<uint8_t> gen_nal() const = 0;
    virtual std::vector<uint8_t> gen_obu() const = 0;
    const std::vector<uint8_t>& getData() const { return m_data; }
    int64_t timestamp() const { return m_timestamp; }
protected:
    int64_t m_timestamp;
    std::vector<uint8_t> m_data;
};

class RGYFrameDataHDR10plus : public RGYFrameDataMetadata {
public:
    RGYFrameDataHDR10plus();
    RGYFrameDataHDR10plus(const uint8_t* data, size_t size, int64_t timestamp);
    virtual ~RGYFrameDataHDR10plus();
    virtual std::vector<uint8_t> gen_nal() const override;
    virtual std::vector<uint8_t> gen_obu() const override;
};

class RGYFrameDataDOVIRpuConvertParam : public RGYFrameDataMetadataConvertParam {
public:
    RGYDOVIProfile doviProfileDst;

    RGYFrameDataDOVIRpuConvertParam() : RGYFrameDataMetadataConvertParam(), doviProfileDst(RGY_DOVI_PROFILE_UNSET) {}
    RGYFrameDataDOVIRpuConvertParam(RGYDOVIProfile profile, const RGYDOVIRpuConvertParam& param) :
        RGYFrameDataMetadataConvertParam(param), doviProfileDst(profile) {
    }
    virtual ~RGYFrameDataDOVIRpuConvertParam() {};
};

class RGYFrameDataDOVIRpu : public RGYFrameDataMetadata {
public:
    RGYFrameDataDOVIRpu();
    RGYFrameDataDOVIRpu(const uint8_t* data, size_t size, int64_t timestamp);
    virtual ~RGYFrameDataDOVIRpu();
    virtual RGY_ERR convert(const RGYFrameDataMetadataConvertParam *prm, RGYLog *log);
    virtual std::vector<uint8_t> gen_nal() const override;
    virtual std::vector<uint8_t> gen_obu() const override;
};

struct RGYFrame {
public:
    RGYFrame() {};
    virtual ~RGYFrame() { }
    virtual bool isempty() const = 0;
    std::array<void*, RGY_MAX_PLANES> ptr() const {
        auto frame = getInfo();
        std::array<void*, RGY_MAX_PLANES> ptrarray;
        for (size_t i = 0; i < ptrarray.size(); i++) {
            ptrarray[i] = (void *)getPlane(&frame, (RGY_PLANE)i).ptr[0];
        }
        return ptrarray;
    }
    void ptrArray(void *array[RGY_MAX_PLANES]) {
        auto frame = getInfo();
        for (size_t i = 0; i < RGY_MAX_PLANES; i++) {
            array[i] = (void *)getPlane(&frame, (RGY_PLANE)i).ptr[0];
        }
    }
    uint8_t *ptrPlane(const RGY_PLANE plane) const {
        auto frame = getInfo();
        return (uint8_t *)getPlane(&frame, plane).ptr[0];
    }
    uint8_t *ptrY() const {
        return ptrPlane(RGY_PLANE_Y);
    }
    uint8_t *ptrUV() const {
        return ptrPlane(RGY_PLANE_C);
    }
    uint8_t *ptrU() const {
        return ptrPlane(RGY_PLANE_U);
    }
    uint8_t *ptrV() const {
        return ptrPlane(RGY_PLANE_V);
    }
    uint8_t *ptrRGB() const {
        return ptrPlane(RGY_PLANE_R);
    }
    RGY_CSP csp() const {
        return getInfo().csp;
    }
    RGY_MEM_TYPE mem_type() const {
        return getInfo().mem_type;
    }
    int width() const {
        return getInfo().width;
    }
    int height() const {
        return getInfo().height;
    }
    uint32_t pitch(const RGY_PLANE plane = RGY_PLANE_Y) const {
        auto frame = getInfo();
        return getPlane(&frame, plane).pitch[0];
    }
    uint64_t timestamp() const {
        return getInfo().timestamp;
    }
    virtual void setTimestamp(uint64_t timestamp) = 0;
    int64_t duration() const {
        return getInfo().duration;
    }
    virtual void setDuration(uint64_t duration) = 0;
    RGY_PICSTRUCT picstruct() const {
        return getInfo().picstruct;
    }
    virtual void setPicstruct(RGY_PICSTRUCT picstruct) = 0;
    int inputFrameId() const {
        return getInfo().inputFrameId;
    }
    virtual void setInputFrameId(int id) = 0;
    RGY_FRAME_FLAGS flags() const {
        return getInfo().flags;
    }
    virtual void setFlags(RGY_FRAME_FLAGS flags) = 0;
    virtual void clearDataList() = 0;
    virtual const std::vector<std::shared_ptr<RGYFrameData>>& dataList() const = 0;
    virtual std::vector<std::shared_ptr<RGYFrameData>>& dataList() = 0;
    virtual void setDataList(const std::vector<std::shared_ptr<RGYFrameData>>& dataList) = 0;

    void setPropertyFrom(const RGYFrame *frame) {
        setDuration(frame->duration());
        setTimestamp(frame->timestamp());
        setInputFrameId(frame->inputFrameId());
        setPicstruct(frame->picstruct());
        setFlags(frame->flags());
        setDataList(frame->dataList());
    }
protected:
    virtual RGYFrameInfo getInfo() const = 0;
};

struct RGYSysFrame : public RGYFrame {
public:
    RGYSysFrame();
    RGYSysFrame(const RGYFrameInfo& frame_);
    virtual ~RGYSysFrame();
    virtual RGY_ERR allocate(const int width, const int height, const RGY_CSP csp, const int bitdepth);
    virtual RGY_ERR allocate(const RGYFrameInfo &frame);
    virtual void deallocate();
    const RGYFrameInfo& frameInfo() { return frame; }
    virtual bool isempty() const { return !frame.ptr[0]; }
    virtual void setTimestamp(uint64_t timestamp) override { frame.timestamp = timestamp; }
    virtual void setDuration(uint64_t duration) override { frame.duration = duration; }
    virtual void setPicstruct(RGY_PICSTRUCT picstruct) override { frame.picstruct = picstruct; }
    virtual void setInputFrameId(int id) override { frame.inputFrameId = id; }
    virtual void setFlags(RGY_FRAME_FLAGS frameflags) override { frame.flags = frameflags; }
    virtual void clearDataList() override { frame.dataList.clear(); }
    virtual const std::vector<std::shared_ptr<RGYFrameData>>& dataList() const override { return frame.dataList; }
    virtual std::vector<std::shared_ptr<RGYFrameData>>& dataList() override { return frame.dataList; }
    virtual void setDataList(const std::vector<std::shared_ptr<RGYFrameData>>& dataList) override { frame.dataList = dataList; }
protected:
    RGYSysFrame(const RGYSysFrame &) = delete;
    void operator =(const RGYSysFrame &) = delete;
    virtual RGYFrameInfo getInfo() const override {
        return frame;
    }
    RGYFrameInfo frame;
};

struct RGYFrameRef : public RGYFrame {
public:
    RGYFrameRef(RGYFrameInfo& frame_);
    virtual ~RGYFrameRef();
    const RGYFrameInfo& frameInfo() { return frame; }
    virtual bool isempty() const { return !frame.ptr[0]; }
    virtual void setTimestamp(uint64_t timestamp) override { frame.timestamp = timestamp; }
    virtual void setDuration(uint64_t duration) override { frame.duration = duration; }
    virtual void setPicstruct(RGY_PICSTRUCT picstruct) override { frame.picstruct = picstruct; }
    virtual void setInputFrameId(int id) override { frame.inputFrameId = id; }
    virtual void setFlags(RGY_FRAME_FLAGS frameflags) override { frame.flags = frameflags; }
    virtual void clearDataList() override { frame.dataList.clear(); }
    virtual const std::vector<std::shared_ptr<RGYFrameData>>& dataList() const override { return frame.dataList; }
    virtual std::vector<std::shared_ptr<RGYFrameData>>& dataList() override { return frame.dataList; }
    virtual void setDataList(const std::vector<std::shared_ptr<RGYFrameData>>& dataList) override { frame.dataList = dataList; }
protected:
    RGYFrameRef(const RGYFrameRef &) = delete;
    void operator =(const RGYFrameRef &) = delete;
    virtual RGYFrameInfo getInfo() const override {
        return frame;
    }
    RGYFrameInfo& frame;
};

#endif //__RGY_FRAME_H__
