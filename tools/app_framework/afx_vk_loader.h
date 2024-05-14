// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#pragma once

#ifdef VK_NO_PROTOTYPES

#include <vulkan/vulkan.h>

//Global
#define RPS_AFX_VK_GLOBAL_FUNCS(FuncEntry)              \
    FuncEntry(vkGetInstanceProcAddr)                    \
    FuncEntry(vkCreateInstance)                         \
    FuncEntry(vkEnumerateInstanceLayerProperties)       \
    FuncEntry(vkEnumerateInstanceExtensionProperties)

//Instance
#define RPS_AFX_VK_INSTANCE_FUNCS(FuncEntry)                \
    FuncEntry(vkEnumeratePhysicalDevices)                   \
    FuncEntry(vkGetPhysicalDeviceMemoryProperties)          \
    FuncEntry(vkGetPhysicalDeviceQueueFamilyProperties)     \
    FuncEntry(vkGetPhysicalDeviceProperties)                \
    FuncEntry(vkEnumerateDeviceExtensionProperties)         \
    FuncEntry(vkCreateWin32SurfaceKHR)                      \
    FuncEntry(vkGetPhysicalDeviceSurfaceSupportKHR)         \
    FuncEntry(vkCreateDevice)                               \
    FuncEntry(vkGetDeviceProcAddr)                          \
    FuncEntry(vkDestroyInstance)                            \
    FuncEntry(vkGetPhysicalDeviceSurfaceCapabilitiesKHR)    \
    FuncEntry(vkGetPhysicalDeviceSurfacePresentModesKHR)    \
    FuncEntry(vkGetPhysicalDeviceSurfaceFormatsKHR)         \
    FuncEntry(vkGetPhysicalDeviceFeatures2)                 \
    FuncEntry(vkDestroySurfaceKHR)


//Device
#define RPS_AFX_VK_DEVICE_FUNCS(FuncEntry)  \
    FuncEntry(vkGetDeviceQueue)             \
    FuncEntry(vkCreateDescriptorPool)       \
    FuncEntry(vkCreateSwapchainKHR)         \
    FuncEntry(vkDestroyImageView)           \
    FuncEntry(vkDestroySwapchainKHR)        \
    FuncEntry(vkGetSwapchainImagesKHR)      \
    FuncEntry(vkCreateSemaphore)            \
    FuncEntry(vkCreateFence)                \
    FuncEntry(vkDestroyBuffer)              \
    FuncEntry(vkFreeMemory)                 \
    FuncEntry(vkCreateBuffer)               \
    FuncEntry(vkGetBufferMemoryRequirements)\
    FuncEntry(vkAllocateMemory)             \
    FuncEntry(vkBindBufferMemory)           \
    FuncEntry(vkMapMemory)                  \
    FuncEntry(vkCreateImageView)            \
    FuncEntry(vkCmdPipelineBarrier)         \
    FuncEntry(vkCmdCopyBufferToImage)       \
    FuncEntry(vkGetImageMemoryRequirements) \
    FuncEntry(vkBindImageMemory)            \
    FuncEntry(vkFlushMappedMemoryRanges)    \
    FuncEntry(vkResetCommandPool)           \
    FuncEntry(vkFreeCommandBuffers)         \
    FuncEntry(vkResetDescriptorPool)        \
    FuncEntry(vkAllocateDescriptorSets)     \
    FuncEntry(vkEndCommandBuffer)           \
    FuncEntry(vkBeginCommandBuffer)         \
    FuncEntry(vkAllocateCommandBuffers)     \
    FuncEntry(vkCreateCommandPool)          \
    FuncEntry(vkQueueSubmit)                \
    FuncEntry(vkCmdClearColorImage)         \
    FuncEntry(vkResetFences)                \
    FuncEntry(vkWaitForFences)              \
    FuncEntry(vkAcquireNextImageKHR)        \
    FuncEntry(vkDestroyDevice)              \
    FuncEntry(vkDestroyDescriptorPool)      \
    FuncEntry(vkDestroyCommandPool)         \
    FuncEntry(vkDestroySemaphore)           \
    FuncEntry(vkQueuePresentKHR)            \
    FuncEntry(vkDestroyImage)               \
    FuncEntry(vkCreateFramebuffer)          \
    FuncEntry(vkDestroyFramebuffer)         \
    FuncEntry(vkCreateRenderPass)           \
    FuncEntry(vkDestroyRenderPass)          \
    FuncEntry(vkCreateBufferView)           \
    FuncEntry(vkDestroyBufferView)          \
    FuncEntry(vkCmdSetViewport)             \
    FuncEntry(vkCmdSetScissor)              \
    FuncEntry(vkCmdClearDepthStencilImage)  \
    FuncEntry(vkCmdCopyImage)               \
    FuncEntry(vkCmdCopyBuffer)              \
    FuncEntry(vkCmdCopyImageToBuffer)       \
    FuncEntry(vkCmdResolveImage)            \
    FuncEntry(vkCmdDraw)                    \
    FuncEntry(vkCmdBindPipeline)            \
    FuncEntry(vkCmdBeginRenderPass)         \
    FuncEntry(vkCmdEndRenderPass)           \
    FuncEntry(vkCmdPushConstants)           \
    FuncEntry(vkCreateGraphicsPipelines)    \
    FuncEntry(vkCreateImage)                \
    FuncEntry(vkCreatePipelineLayout)       \
    FuncEntry(vkCreateShaderModule)         \
    FuncEntry(vkDestroyFence)               \
    FuncEntry(vkDestroyPipeline)            \
    FuncEntry(vkDestroyShaderModule)        \
    FuncEntry(vkDeviceWaitIdle)             \
    FuncEntry(vkDestroyPipelineLayout)

#define RPS_AFX_DEFINE_VK_FUNC_PTR_ENTRY(callname) static PFN_##callname callname;

#define RPS_AFX_DECLARE_VK_FUNC_PTR_ENTRY(callname) extern PFN_##callname callname;
RPS_AFX_VK_GLOBAL_FUNCS(RPS_AFX_DECLARE_VK_FUNC_PTR_ENTRY)
RPS_AFX_VK_INSTANCE_FUNCS(RPS_AFX_DECLARE_VK_FUNC_PTR_ENTRY)
RPS_AFX_VK_DEVICE_FUNCS(RPS_AFX_DECLARE_VK_FUNC_PTR_ENTRY)

#define RPS_AFX_DEFINE_VK_FUNC_PTRS                             \
    RPS_AFX_VK_GLOBAL_FUNCS(RPS_AFX_DEFINE_VK_FUNC_PTR_ENTRY)   \
    RPS_AFX_VK_INSTANCE_FUNCS(RPS_AFX_DEFINE_VK_FUNC_PTR_ENTRY) \
    RPS_AFX_VK_DEVICE_FUNCS(RPS_AFX_DEFINE_VK_FUNC_PTR_ENTRY)

#endif //VK_NO_PROTOTYPES
