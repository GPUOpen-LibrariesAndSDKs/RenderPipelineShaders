// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#else
#error "TODO"
#endif

#define RPS_VK_RUNTIME 1

#include "test_render_to_texture_shared.hpp"

#include "utils/rps_test_common.h"
#include "utils/rps_test_win32.hpp"
#include "utils/rps_test_vk_renderer.hpp"

#include <DirectXMath.h>

class TestVkRenderToTexture : public RpsTestVulkanRenderer, public TestRpsRenderToTexture
{
protected:
    virtual void OnInit(VkCommandBuffer initCmdBuf, InitTempResources& tempResources) override
    {
        LoadAssets(initCmdBuf, tempResources);

        TestRpsRenderToTexture::OnInit();

        RpsResult result =
            rpsProgramBindNode(rpsRenderGraphGetMainEntry(GetRpsRenderGraph()), "Quads", &DrawQuadsCb, this);
        REQUIRE(result == RPS_OK);
    }

    virtual void OnPostResize() override
    {
    }

    virtual void OnCleanUp() override
    {
        TestRpsRenderToTexture::OnCleanUp();

        vkDestroyPipeline(m_device, m_geoPipeline, nullptr);
        vkDestroyPipeline(m_device, m_geoPipelineMSAA, nullptr);
        vkDestroyPipeline(m_device, m_quadPipeline, nullptr);
        vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
        vkDestroySampler(m_device, m_defaultSampler, nullptr);
        vkDestroyDescriptorSetLayout(m_device, m_sharedDescriptorSetLayout, nullptr);
        vkDestroyImage(m_device, m_checkerboardTexture, nullptr);
        vkDestroyImageView(m_device, m_checkerboardTextureView, nullptr);
        vkDestroyBuffer(m_device, m_vertexBuffer, nullptr);
    }

    virtual void OnUpdate(uint32_t frameIndex) override
    {
        TestRpsRenderToTexture::OnUpdate(frameIndex, m_width, m_height);
        UpdatePipeline(frameIndex, CalcGuaranteedCompletedFrameIndexForRps());
    }

    virtual void OnRender(uint32_t frameIndex) override
    {
        REQUIRE(RPS_SUCCEEDED(ExecuteRenderGraph(frameIndex, GetRpsRenderGraph())));
    }

protected:
    virtual void CreateRpsDevice(RpsDevice& rpsDeviceOut) override final
    {
        rpsDeviceOut = rpsTestUtilCreateDevice(
            [this](auto pCreateInfo, auto phDevice) { return CreateRpsRuntimeDevice(*pCreateInfo, *phDevice); });
    }

    virtual void DrawTriangle(const RpsCmdCallbackContext* pContext, bool isMSAA) override final
    {
        if (isMSAA && (m_geoPipelineMSAA == RPS_NULL_HANDLE))
        {
            VkRenderPass rp;
            RpsResult    result = rpsVKGetCmdRenderPass(pContext, &rp);
            REQUIRE(result == RPS_OK);

            CreatePipeline(c_Shader, rp, &m_geoPipelineMSAA, true);
        }
        else if (!isMSAA && (m_geoPipeline == RPS_NULL_HANDLE))
        {
            VkRenderPass rp;
            RpsResult    result = rpsVKGetCmdRenderPass(pContext, &rp);
            REQUIRE(result == RPS_OK);

            CreatePipeline(c_Shader, rp, &m_geoPipeline, false);
        }

        VkCommandBuffer cmdBuf = rpsVKCommandBufferFromHandle(pContext->hCommandBuffer);

        VkDescriptorSet ds;
        ThrowIfFailedVK(AllocFrameDescriptorSet(&m_sharedDescriptorSetLayout, 1, &ds));

        VkWriteDescriptorSet writeDescriptorSet[2] = {};

        VkDescriptorBufferInfo bufInfo =
            AllocAndWriteFrameConstants(&m_triangleAnimationData, sizeof(m_triangleAnimationData));
        AppendWriteDescriptorSetBuffers(&writeDescriptorSet[0], ds, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &bufInfo);

        VkDescriptorImageInfo imageInfo = {
            VK_NULL_HANDLE, m_checkerboardTextureView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        AppendWriteDescriptorSetImages(&writeDescriptorSet[1], ds, 1, 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, &imageInfo);

        vkUpdateDescriptorSets(m_device, _countof(writeDescriptorSet), writeDescriptorSet, 0, nullptr);

        vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &ds, 0, nullptr);
        vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, isMSAA ? m_geoPipelineMSAA : m_geoPipeline);
        vkCmdBindVertexBuffers(cmdBuf, 0, 1, &m_vertexBuffer, &m_triangleVbOffset);
        vkCmdDraw(cmdBuf, 3, 1, 0, 0);
    }

    void DrawQuads(const RpsCmdCallbackContext* pContext, rps::UnusedArg backBuffer, VkImageView offscreenRTSrv)
    {
        if (m_quadPipeline == RPS_NULL_HANDLE)
        {
            VkRenderPass rp;
            RpsResult    result = rpsVKGetCmdRenderPass(pContext, &rp);
            REQUIRE(result == RPS_OK);

            CreatePipeline(c_Shader, rp, &m_quadPipeline, false);
        }

        VkCommandBuffer cmdBuf = rpsVKCommandBufferFromHandle(pContext->hCommandBuffer);

        VkDescriptorSet ds;
        ThrowIfFailedVK(AllocFrameDescriptorSet(&m_sharedDescriptorSetLayout, 1, &ds));

        VkWriteDescriptorSet writeDescriptorSet[2] = {};

        VkDescriptorBufferInfo bufInfo = AllocAndWriteFrameConstants(&m_quadConstantData, sizeof(m_quadConstantData));
        AppendWriteDescriptorSetBuffers(&writeDescriptorSet[0], ds, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &bufInfo);

        VkDescriptorImageInfo imageInfo = {VK_NULL_HANDLE, offscreenRTSrv, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        AppendWriteDescriptorSetImages(&writeDescriptorSet[1], ds, 1, 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, &imageInfo);

        vkUpdateDescriptorSets(m_device, _countof(writeDescriptorSet), writeDescriptorSet, 0, nullptr);

        vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &ds, 0, nullptr);
        vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_quadPipeline);
        vkCmdBindVertexBuffers(cmdBuf, 0, 1, &m_vertexBuffer, &m_quadVbOffset);
        vkCmdDraw(cmdBuf, 24, 1, 0, 0);
    }

    static void DrawQuadsCb(const RpsCmdCallbackContext* pContext)
    {
        auto pThis = static_cast<TestVkRenderToTexture*>(pContext->pCmdCallbackContext);

        VkImageView hSrv;
        REQUIRE(RPS_OK == rpsVKGetCmdArgImageView(pContext, 1, &hSrv));

        pThis->DrawQuads(pContext, {}, hSrv);
    }

private:
    void LoadAssets(VkCommandBuffer initCmdBuf, InitTempResources& tempResources)
    {
        OnPostResize();

        VkSamplerCreateInfo sampCI = {};
        sampCI.sType               = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampCI.magFilter           = VK_FILTER_LINEAR;
        sampCI.minFilter           = VK_FILTER_LINEAR;
        sampCI.mipmapMode          = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        sampCI.addressModeU        = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampCI.addressModeV        = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampCI.addressModeW        = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampCI.mipLodBias          = 0.0f;
        sampCI.compareOp           = VK_COMPARE_OP_NEVER;
        sampCI.minLod              = 0.0f;
        sampCI.maxLod              = FLT_MAX;
        sampCI.maxAnisotropy       = 1.0;
        sampCI.anisotropyEnable    = VK_FALSE;
        sampCI.borderColor         = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        ThrowIfFailedVK(vkCreateSampler(m_device, &sampCI, nullptr, &m_defaultSampler));

        VkDescriptorSetLayoutBinding sharedLayoutBindings[4] = {};
        sharedLayoutBindings[0].binding                      = 0;
        sharedLayoutBindings[0].descriptorCount              = 1;
        sharedLayoutBindings[0].descriptorType               = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        sharedLayoutBindings[0].stageFlags         = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        sharedLayoutBindings[1].binding            = 1;
        sharedLayoutBindings[1].descriptorCount    = 1;
        sharedLayoutBindings[1].descriptorType     = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        sharedLayoutBindings[1].stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;
        sharedLayoutBindings[2].binding            = 2;
        sharedLayoutBindings[2].descriptorCount    = 1;
        sharedLayoutBindings[2].descriptorType     = VK_DESCRIPTOR_TYPE_SAMPLER;
        sharedLayoutBindings[2].stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;
        sharedLayoutBindings[2].pImmutableSamplers = &m_defaultSampler;
        sharedLayoutBindings[3].binding            = 3;
        sharedLayoutBindings[3].descriptorCount    = 1;
        sharedLayoutBindings[3].descriptorType     = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        sharedLayoutBindings[3].stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo setLayoutCI = {};
        setLayoutCI.sType                           = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        setLayoutCI.pBindings                       = sharedLayoutBindings;
        setLayoutCI.bindingCount                    = _countof(sharedLayoutBindings);

        ThrowIfFailedVK(vkCreateDescriptorSetLayout(m_device, &setLayoutCI, nullptr, &m_sharedDescriptorSetLayout));

        VkPipelineLayoutCreateInfo plCI = {};
        plCI.sType                      = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        plCI.setLayoutCount             = 1;
        plCI.pSetLayouts                = &m_sharedDescriptorSetLayout;

        ThrowIfFailedVK(vkCreatePipelineLayout(m_device, &plCI, nullptr, &m_pipelineLayout));

        // Create vertex buffers
        {
            Vertex triangleVertices[] = {
                // triangle
                {{0.0f, 0.25f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.5f, 0.0f}},
                {{0.25f, -0.25f, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
                {{-0.25f, -0.25f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},

                // quad 0
                {{-1.0f, 1.0, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}},
                {{0.f, 0.f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},
                {{-1.f, 0.f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},
                {{-1.0f, 1.0, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}},
                {{0.f, 1.f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}},
                {{0.f, 0.f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},

                // quad 1
                {{-1.0f, 0.0, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},
                {{0.f, -1.f, 0.0f}, {0.f, 0.0f, 1.0f, 1.0f}, {1.0f, 0.0f}},
                {{-1.f, -1.f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}, {0.0f, 0.0f}},
                {{-1.0f, 0.0, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},
                {{0.f, 0.f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},
                {{0.f, -1.f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}, {1.0f, 0.0f}},

                // quad 2
                {{0.0f, 1.0, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
                {{1.f, 0.f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
                {{0.f, 0.f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
                {{0.0f, 1.0, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
                {{1.f, 1.f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
                {{1.f, 0.f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},

                // quad 3
                {{0.0f, 0.0, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
                {{1.f, -1.f, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
                {{0.f, -1.f, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
                {{0.0f, 0.0, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
                {{1.f, 0.f, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
                {{1.f, -1.f, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
            };

            const UINT vertexBufferSize = sizeof(triangleVertices);

            m_vertexBuffer = CreateAndBindStaticBuffer(
                vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
            m_triangleVbOffset = 0;
            m_quadVbOffset     = sizeof(Vertex) * 3;

            auto         vbAlloc = AllocAndWriteFrameConstants(triangleVertices, vertexBufferSize);
            VkBufferCopy vbCopy;
            vbCopy.srcOffset = vbAlloc.offset;
            vbCopy.dstOffset = 0;
            vbCopy.size      = vertexBufferSize;
            vkCmdCopyBuffer(initCmdBuf, vbAlloc.buffer, m_vertexBuffer, 1, &vbCopy);
        }

        CreateCheckerboardTexture(initCmdBuf, tempResources);
    }

    void CreateCheckerboardTexture(VkCommandBuffer initCmdBuf, InitTempResources& tempResources)
    {
        // Texture data contains 4 channels (RGBA) with unnormalized 8-bit values, this is the most commonly supported format
        VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;

        uint32_t width            = 256;
        uint32_t height           = 256;
        uint32_t texturePixelSize = 4;

        const uint32_t rowPitch    = width * texturePixelSize;
        const uint32_t cellPitch   = rowPitch >> 3;  // The width of a cell in the checkboard texture.
        const uint32_t cellHeight  = width >> 3;     // The height of a cell in the checkerboard texture.
        const uint32_t textureSize = rowPitch * height;

        std::vector<UINT8> data(textureSize);
        uint8_t*           textureData = &data[0];

        for (uint32_t n = 0; n < textureSize; n += texturePixelSize)
        {
            uint32_t x = n % rowPitch;
            uint32_t y = n / rowPitch;
            uint32_t i = x / cellPitch;
            uint32_t j = y / cellHeight;

            if (i % 2 == j % 2)
            {
                textureData[n]     = 0xa0;  // R
                textureData[n + 1] = 0xa0;  // G
                textureData[n + 2] = 0xa0;  // B
                textureData[n + 3] = 0xff;  // A
            }
            else
            {
                textureData[n]     = 0xff;  // R
                textureData[n + 1] = 0xff;  // G
                textureData[n + 2] = 0xff;  // B
                textureData[n + 3] = 0xff;  // A
            }
        }

        {
            auto textureDataUploadBuf = AllocAndWriteFrameConstants(textureData, textureSize);

            m_checkerboardTexture =
                CreateAndBindStaticImage(VK_IMAGE_TYPE_2D,
                                         VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                                         format,
                                         width,
                                         height,
                                         1,
                                         1,
                                         1);

            VkBufferImageCopy bufferCopyRegion               = {};
            bufferCopyRegion.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            bufferCopyRegion.imageSubresource.mipLevel       = 0;
            bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
            bufferCopyRegion.imageSubresource.layerCount     = 1;
            bufferCopyRegion.imageExtent.width               = width;
            bufferCopyRegion.imageExtent.height              = height;
            bufferCopyRegion.imageExtent.depth               = 1;
            bufferCopyRegion.bufferOffset                    = textureDataUploadBuf.offset;

            // Transition the texture image layout to transfer target, so we can safely copy our buffer data to it.
            VkImageMemoryBarrier imageMemoryBarrier          = {};
            imageMemoryBarrier.sType                         = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            imageMemoryBarrier.image                         = m_checkerboardTexture;
            imageMemoryBarrier.subresourceRange.aspectMask   = VK_IMAGE_ASPECT_COLOR_BIT;
            imageMemoryBarrier.subresourceRange.baseMipLevel = 0;
            imageMemoryBarrier.subresourceRange.levelCount   = 1;
            imageMemoryBarrier.subresourceRange.layerCount   = 1;
            imageMemoryBarrier.srcAccessMask                 = 0;
            imageMemoryBarrier.dstAccessMask                 = VK_ACCESS_TRANSFER_WRITE_BIT;
            imageMemoryBarrier.oldLayout                     = VK_IMAGE_LAYOUT_UNDEFINED;
            imageMemoryBarrier.newLayout                     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

            // Insert a memory dependency at the proper pipeline stages that will execute the image layout transition
            // Source pipeline stage is host write/read exection (VK_PIPELINE_STAGE_HOST_BIT)
            // Destination pipeline stage is copy command exection (VK_PIPELINE_STAGE_TRANSFER_BIT)
            vkCmdPipelineBarrier(initCmdBuf,
                                 VK_PIPELINE_STAGE_HOST_BIT,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 0,
                                 0,
                                 nullptr,
                                 0,
                                 nullptr,
                                 1,
                                 &imageMemoryBarrier);

            // Copy mip levels from staging buffer
            vkCmdCopyBufferToImage(initCmdBuf,
                                   textureDataUploadBuf.buffer,
                                   m_checkerboardTexture,
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                   1,
                                   &bufferCopyRegion);

            // Once the data has been uploaded we transfer to the texture image to the shader read layout, so it can be sampled from
            imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            imageMemoryBarrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            imageMemoryBarrier.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            vkCmdPipelineBarrier(initCmdBuf,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                 0,
                                 0,
                                 nullptr,
                                 0,
                                 nullptr,
                                 1,
                                 &imageMemoryBarrier);
        }

        // Create image view
        // Textures are not directly accessed by the shaders and
        // are abstracted by image views containing additional
        // information and sub resource ranges
        VkImageViewCreateInfo view = {};
        view.sType                 = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view.viewType              = VK_IMAGE_VIEW_TYPE_2D;
        view.format                = format;
        view.components            = {
                       VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A};
        view.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        view.subresourceRange.baseMipLevel   = 0;
        view.subresourceRange.baseArrayLayer = 0;
        view.subresourceRange.layerCount     = 1;
        view.subresourceRange.levelCount     = 1;
        view.image                           = m_checkerboardTexture;
        ThrowIfFailedVK(vkCreateImageView(m_device, &view, nullptr, &m_checkerboardTextureView));
    }

    void CreatePipeline(const char* pShaderCode, VkRenderPass renderPass, VkPipeline* pPipeline, bool bMSAA)
    {
        VkVertexInputBindingDescription vertBinding = {};
        vertBinding.binding                         = 0;
        vertBinding.stride                          = sizeof(Vertex);
        vertBinding.inputRate                       = VK_VERTEX_INPUT_RATE_VERTEX;

        VkVertexInputAttributeDescription vertAttrBinding[3] = {};
        vertAttrBinding[0].binding                           = 0;
        vertAttrBinding[0].location                          = 0;
        vertAttrBinding[0].format                            = VK_FORMAT_R32G32B32_SFLOAT;
        vertAttrBinding[0].offset                            = offsetof(Vertex, position);
        vertAttrBinding[1].binding                           = 0;
        vertAttrBinding[1].location                          = 1;
        vertAttrBinding[1].format                            = VK_FORMAT_R32G32B32_SFLOAT;
        vertAttrBinding[1].offset                            = offsetof(Vertex, color);
        vertAttrBinding[2].binding                           = 0;
        vertAttrBinding[2].location                          = 2;
        vertAttrBinding[2].format                            = VK_FORMAT_R32G32_SFLOAT;
        vertAttrBinding[2].offset                            = offsetof(Vertex, uv);

        VkPipelineVertexInputStateCreateInfo vi = {};
        vi.sType                                = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vi.pNext                                = NULL;
        vi.flags                                = 0;
        vi.vertexBindingDescriptionCount        = 1;
        vi.pVertexBindingDescriptions           = &vertBinding;
        vi.vertexAttributeDescriptionCount      = _countof(vertAttrBinding);
        vi.pVertexAttributeDescriptions         = vertAttrBinding;

        // input assembly state
        //
        VkPipelineInputAssemblyStateCreateInfo ia;
        ia.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        ia.pNext                  = NULL;
        ia.flags                  = 0;
        ia.primitiveRestartEnable = VK_FALSE;
        ia.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        // rasterizer state
        VkPipelineRasterizationStateCreateInfo rs;
        rs.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rs.pNext                   = NULL;
        rs.flags                   = 0;
        rs.polygonMode             = VK_POLYGON_MODE_FILL;
        rs.cullMode                = VK_CULL_MODE_NONE;
        rs.frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rs.depthClampEnable        = VK_FALSE;
        rs.rasterizerDiscardEnable = VK_FALSE;
        rs.depthBiasEnable         = VK_FALSE;
        rs.depthBiasConstantFactor = 0;
        rs.depthBiasClamp          = 0;
        rs.depthBiasSlopeFactor    = 0;
        rs.lineWidth               = 1.0f;

        VkPipelineColorBlendAttachmentState bs[1] = {};
        bs[0].blendEnable                         = VK_FALSE;
        bs[0].srcColorBlendFactor                 = VK_BLEND_FACTOR_ONE;
        bs[0].dstColorBlendFactor                 = VK_BLEND_FACTOR_ZERO;
        bs[0].colorBlendOp                        = VK_BLEND_OP_ADD;
        bs[0].srcAlphaBlendFactor                 = VK_BLEND_FACTOR_ONE;
        bs[0].dstAlphaBlendFactor                 = VK_BLEND_FACTOR_ZERO;
        bs[0].alphaBlendOp                        = VK_BLEND_OP_ADD;
        bs[0].colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        // Color blend state
        VkPipelineColorBlendStateCreateInfo cb;
        cb.sType             = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        cb.flags             = 0;
        cb.pNext             = NULL;
        cb.attachmentCount   = 1;
        cb.pAttachments      = bs;
        cb.logicOpEnable     = VK_FALSE;
        cb.logicOp           = VK_LOGIC_OP_NO_OP;
        cb.blendConstants[0] = 1.0f;
        cb.blendConstants[1] = 1.0f;
        cb.blendConstants[2] = 1.0f;
        cb.blendConstants[3] = 1.0f;

        std::vector<VkDynamicState>      dynamicStateEnables = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamicState        = {};
        dynamicState.sType                                   = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.pNext                                   = NULL;
        dynamicState.pDynamicStates                          = dynamicStateEnables.data();
        dynamicState.dynamicStateCount                       = (uint32_t)dynamicStateEnables.size();

        // view port state

        VkPipelineViewportStateCreateInfo vp = {};
        vp.sType                             = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        vp.pNext                             = NULL;
        vp.flags                             = 0;
        vp.viewportCount                     = 1;
        vp.scissorCount                      = 1;
        vp.pScissors                         = NULL;
        vp.pViewports                        = NULL;

        // depth stencil state

        VkPipelineDepthStencilStateCreateInfo ds;
        ds.sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        ds.pNext                 = NULL;
        ds.flags                 = 0;
        ds.depthTestEnable       = VK_FALSE;
        ds.depthWriteEnable      = VK_FALSE;
        ds.depthCompareOp        = VK_COMPARE_OP_LESS_OR_EQUAL;
        ds.depthBoundsTestEnable = VK_FALSE;
        ds.stencilTestEnable     = VK_FALSE;
        ds.back.failOp           = VK_STENCIL_OP_KEEP;
        ds.back.passOp           = VK_STENCIL_OP_KEEP;
        ds.back.compareOp        = VK_COMPARE_OP_ALWAYS;
        ds.back.compareMask      = 0;
        ds.back.reference        = 0;
        ds.back.depthFailOp      = VK_STENCIL_OP_KEEP;
        ds.back.writeMask        = 0;
        ds.minDepthBounds        = 0;
        ds.maxDepthBounds        = 0;
        ds.stencilTestEnable     = VK_FALSE;
        ds.front                 = ds.back;

        // multi sample state

        VkPipelineMultisampleStateCreateInfo ms;
        ms.sType                 = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        ms.pNext                 = NULL;
        ms.flags                 = 0;
        ms.pSampleMask           = NULL;
        ms.rasterizationSamples  = bMSAA ? VK_SAMPLE_COUNT_4_BIT : VK_SAMPLE_COUNT_1_BIT;
        ms.sampleShadingEnable   = VK_FALSE;
        ms.alphaToCoverageEnable = VK_FALSE;
        ms.alphaToOneEnable      = VK_FALSE;
        ms.minSampleShading      = 0.0;

        VkShaderModule       vsModule, psModule;
        std::vector<uint8_t> vsCode, psCode;

        DxcCompileToSpirv(pShaderCode, L"VSMain", L"vs_6_0", L"", nullptr, 0, vsCode);
        DxcCompileToSpirv(pShaderCode, L"PSMain", L"ps_6_0", L"", nullptr, 0, psCode);

        VkShaderModuleCreateInfo smCI = {};
        smCI.sType                    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;

        smCI.pCode    = reinterpret_cast<const uint32_t*>(vsCode.data());
        smCI.codeSize = vsCode.size();

        ThrowIfFailedVK(vkCreateShaderModule(m_device, &smCI, nullptr, &vsModule));

        smCI.pCode    = reinterpret_cast<const uint32_t*>(psCode.data());
        smCI.codeSize = psCode.size();

        ThrowIfFailedVK(vkCreateShaderModule(m_device, &smCI, nullptr, &psModule));

        VkPipelineShaderStageCreateInfo shaderStages[2] = {};
        shaderStages[0].sType                           = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStages[0].module                          = vsModule;
        shaderStages[0].pName                           = "VSMain";
        shaderStages[0].stage                           = VK_SHADER_STAGE_VERTEX_BIT;
        shaderStages[1].sType                           = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStages[1].module                          = psModule;
        shaderStages[1].pName                           = "PSMain";
        shaderStages[1].stage                           = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkGraphicsPipelineCreateInfo psoCI = {};
        psoCI.sType                        = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        psoCI.pNext                        = NULL;
        psoCI.layout                       = m_pipelineLayout;
        psoCI.basePipelineHandle           = VK_NULL_HANDLE;
        psoCI.basePipelineIndex            = 0;
        psoCI.flags                        = 0;
        psoCI.pVertexInputState            = &vi;
        psoCI.pInputAssemblyState          = &ia;
        psoCI.pRasterizationState          = &rs;
        psoCI.pColorBlendState             = &cb;
        psoCI.pTessellationState           = NULL;
        psoCI.pMultisampleState            = &ms;
        psoCI.pDynamicState                = &dynamicState;
        psoCI.pViewportState               = &vp;
        psoCI.pDepthStencilState           = &ds;
        psoCI.pStages                      = shaderStages;
        psoCI.stageCount                   = _countof(shaderStages);
        psoCI.renderPass                   = renderPass;
        psoCI.subpass                      = 0;

        ThrowIfFailedVK(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &psoCI, nullptr, pPipeline));

        vkDestroyShaderModule(m_device, vsModule, nullptr);
        vkDestroyShaderModule(m_device, psModule, nullptr);
    }

    void UpdatePipeline(uint64_t frameIndex, uint64_t completedFrameIndex)
    {
        RpsRuntimeResource backBuffers[16];

        if (m_swapChainImages.size() > RPS_TEST_COUNTOF(backBuffers))
            throw;

        for (uint32_t i = 0; i < m_swapChainImages.size(); i++)
        {
            backBuffers[i] = rpsVKImageToHandle(m_swapChainImages[i].image);
        }

        RpsResourceDesc backBufferDesc   = {};
        backBufferDesc.type              = RPS_RESOURCE_TYPE_IMAGE_2D;
        backBufferDesc.temporalLayers    = uint32_t(m_swapChainImages.size());
        backBufferDesc.image.arrayLayers = 1;
        backBufferDesc.image.mipLevels   = 1;
        backBufferDesc.image.format      = rpsFormatFromVK(m_swapChainFormat.format);
        backBufferDesc.image.width       = m_width;
        backBufferDesc.image.height      = m_height;
        backBufferDesc.image.sampleCount = 1;

        TestRpsRenderToTexture::UpdateRpsPipeline(frameIndex, completedFrameIndex, backBufferDesc, backBuffers);
    }

private:
    VkPipeline m_geoPipeline     = VK_NULL_HANDLE;
    VkPipeline m_geoPipelineMSAA = VK_NULL_HANDLE;
    VkPipeline m_quadPipeline    = VK_NULL_HANDLE;

    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;

    VkSampler m_defaultSampler = VK_NULL_HANDLE;

    VkDescriptorSetLayout m_sharedDescriptorSetLayout = VK_NULL_HANDLE;

    VkImage     m_checkerboardTexture     = RPS_NULL_HANDLE;
    VkImageView m_checkerboardTextureView = RPS_NULL_HANDLE;
    VkBuffer    m_vertexBuffer            = RPS_NULL_HANDLE;

    VkDeviceSize m_triangleVbOffset = 0;
    VkDeviceSize m_quadVbOffset     = 0;
};

TEST_CASE(TEST_APP_NAME)
{
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#if defined(BREAK_AT_ALLOC_ID)
    _CrtSetBreakAlloc(BREAK_AT_ALLOC_ID);
#endif

    TestVkRenderToTexture renderer;

    RpsTestRunWindowInfo runInfo = {};
    runInfo.title                = TEXT(TEST_APP_NAME);
    runInfo.numFramesToRender    = g_exitAfterFrame;
    runInfo.width                = 1280;
    runInfo.height               = 720;
    runInfo.pRenderer            = &renderer;
    RpsTestRunWindowApp(&runInfo);
}
