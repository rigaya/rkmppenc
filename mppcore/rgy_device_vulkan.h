﻿// -----------------------------------------------------------------------------------------
//     VCEEnc by rigaya
// -----------------------------------------------------------------------------------------
// The MIT License
//
// Copyright (c) 2021 rigaya
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
#ifndef __RGY_DEVICE_VULKAN_H__
#define __RGY_DEVICE_VULKAN_H__

#include <unordered_map>

#include "rgy_version.h"
#include "rgy_util.h"
#include "rgy_log.h"
#include "rgy_err.h"

class DeviceVulkan;

#if ENABLE_VULKAN
#include "rgy_vulkan.h"

#define RGY_VK_API_VER (VK_API_VERSION_1_2)

#if ENCODER_VCEENC
#include "core/VulkanAMF.h"
#endif // #if ENCODER_VCEENC

class DeviceVulkan {
public:
    DeviceVulkan();
    virtual ~DeviceVulkan();

    RGY_ERR Init(int adapterID, const std::vector<const char*> &extInstance, const std::vector<const char*> &extDevice, std::shared_ptr<RGYLog> log, bool logTryMode);
    RGY_ERR Terminate();

    RGYVulkanFuncs *GetVulkan();
#if ENCODER_VCEENC
    amf::AMFVulkanDevice *GetAMFDevice();
#endif
    const std::string& GetDisplayDeviceName() const { return m_displayDeviceName; }
    const uint8_t *GetUUID() const { return m_uuid; }

    int GetQueueGraphicFamilyIndex() { return m_uQueueGraphicsFamilyIndex; }
    VkQueue GetQueueGraphicQueue() { return m_hQueueGraphics; }

    int GetQueueComputeFamilyIndex() { return m_uQueueComputeFamilyIndex; }
    VkQueue GetQueueComputeQueue() { return m_hQueueCompute; }

    int adapterCount();

    VkDevice GetDevice() { return m_vkDevice; }
    VkPhysicalDevice GetPhysicalDevice() { return m_vkPhysicalDevice; }
    VkInstance GetInstance() { return m_vkInstance; }

#if defined(_WIN32) || defined(_WIN64)
    HANDLE getMemHandle(VkDeviceMemory memory);
    HANDLE getSemaphoreHandle(VkSemaphore &sempahore);
#else
    int getMemHandle(VkDeviceMemory memory);
    int getSemaphoreHandle(VkSemaphore &sempahore);
#endif

protected:
    void AddMessage(RGYLogLevel log_level, const tstring &str);
    void AddMessage(RGYLogLevel log_level, const TCHAR *format, ...);
private:
    RGY_ERR CreateInstance(const std::vector<const char*> &extInstance);
    RGY_ERR CreateDeviceAndFindQueues(int adapterID, const std::vector<const char*> &extDevice);

    std::vector<const char*> GetDebugInstanceExtensionNames();
    std::vector<const char*> GetDebugInstanceLayerNames();
    std::vector<const char*> GetDebugDeviceLayerNames(VkPhysicalDevice device);

    tstring m_name;
    VkInstance m_vkInstance;
    VkPhysicalDevice m_vkPhysicalDevice;
    VkDevice m_vkDevice;
#if ENCODER_VCEENC
    amf::AMFVulkanDevice m_VulkanDev;
#endif
    std::string m_displayDeviceName;
    uint8_t m_uuid[VK_UUID_SIZE];
    RGYVulkanFuncs m_vk;

    int m_uQueueGraphicsFamilyIndex;
    int m_uQueueComputeFamilyIndex;

    VkQueue m_hQueueGraphics;
    VkQueue m_hQueueCompute;

    std::shared_ptr<RGYLog> m_log;
    bool m_logTryMode;
};

#endif //#if ENABLE_VULKAN

#endif //#if __RGY_DEVICE_VULKAN_H__
