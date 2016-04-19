// Copyright 2016 Intel Corporation All Rights Reserved
// 
// Intel makes no representations about the suitability of this software for any purpose.
// THIS SOFTWARE IS PROVIDED ""AS IS."" INTEL SPECIFICALLY DISCLAIMS ALL WARRANTIES,
// EXPRESS OR IMPLIED, AND ALL LIABILITY, INCLUDING CONSEQUENTIAL AND OTHER INDIRECT DAMAGES,
// FOR THE USE OF THIS SOFTWARE, INCLUDING LIABILITY FOR INFRINGEMENT OF ANY PROPRIETARY
// RIGHTS, AND INCLUDING THE WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
// Intel does not assume any responsibility for any errors which may appear in this software
// nor any responsibility to update it.

#if !defined(_VK_NO_INIT_FUNCTIONS)
VK_FUNCTION(vkCreateInstance);
VK_FUNCTION(vkDestroyInstance);
VK_FUNCTION(vkEnumeratePhysicalDevices);
VK_FUNCTION(vkGetPhysicalDeviceFeatures);
VK_FUNCTION(vkGetPhysicalDeviceFormatProperties);
VK_FUNCTION(vkGetPhysicalDeviceProperties);
VK_FUNCTION(vkGetPhysicalDeviceQueueFamilyProperties);
VK_FUNCTION(vkGetPhysicalDeviceMemoryProperties);
VK_FUNCTION(vkGetInstanceProcAddr);
VK_FUNCTION(vkGetDeviceProcAddr);
VK_FUNCTION(vkCreateDevice);
VK_FUNCTION(vkDestroyDevice);
VK_FUNCTION(vkEnumerateInstanceExtensionProperties);
VK_FUNCTION(vkEnumerateDeviceExtensionProperties);
VK_FUNCTION(vkEnumerateInstanceLayerProperties);
VK_FUNCTION(vkEnumerateDeviceLayerProperties);

// VK_KHR_surface
VK_FUNCTION(vkDestroySurfaceKHR);
VK_FUNCTION(vkGetPhysicalDeviceSurfaceSupportKHR);
VK_FUNCTION(vkGetPhysicalDeviceSurfaceCapabilitiesKHR);
VK_FUNCTION(vkGetPhysicalDeviceSurfaceFormatsKHR);
VK_FUNCTION(vkGetPhysicalDeviceSurfacePresentModesKHR);

#ifdef VK_USE_PLATFORM_WIN32_KHR
// VK_KHR_win32_surface
VK_FUNCTION(vkCreateWin32SurfaceKHR);
VK_FUNCTION(vkGetPhysicalDeviceWin32PresentationSupportKHR);
#endif

#ifdef VK_USE_PLATFORM_XCB_KHR
// VK_KHR_xcb_surface
VK_FUNCTION(vkCreateXcbSurfaceKHR)
VK_FUNCTION(vkGetPhysicalDeviceXcbPresentationSupportKHR);
#endif

#ifdef VK_USE_PLATFORM_ANDROID_KHR
// VK_KHR_android_surface
VK_FUNCTION(vkCreateAndroidSurfaceKHR);
#endif
#endif

#if !defined(_VK_NO_DEVICE_FUNCTIONS)
VK_FUNCTION(vkGetDeviceQueue);
VK_FUNCTION(vkQueueSubmit);
VK_FUNCTION(vkQueueWaitIdle);
VK_FUNCTION(vkDeviceWaitIdle);
VK_FUNCTION(vkAllocateMemory);
VK_FUNCTION(vkFreeMemory);
VK_FUNCTION(vkMapMemory);
VK_FUNCTION(vkUnmapMemory);
VK_FUNCTION(vkFlushMappedMemoryRanges);
VK_FUNCTION(vkInvalidateMappedMemoryRanges);
VK_FUNCTION(vkBindBufferMemory);
VK_FUNCTION(vkBindImageMemory);
VK_FUNCTION(vkGetImageMemoryRequirements);
VK_FUNCTION(vkGetBufferMemoryRequirements);
VK_FUNCTION(vkQueueBindSparse);
VK_FUNCTION(vkCreateFence);
VK_FUNCTION(vkResetFences);
VK_FUNCTION(vkGetFenceStatus);
VK_FUNCTION(vkWaitForFences);
VK_FUNCTION(vkCreateSemaphore);
VK_FUNCTION(vkCreateEvent);
VK_FUNCTION(vkGetEventStatus);
VK_FUNCTION(vkSetEvent);
VK_FUNCTION(vkResetEvent);
VK_FUNCTION(vkCreateQueryPool);
VK_FUNCTION(vkGetQueryPoolResults);
VK_FUNCTION(vkCreateBuffer);
VK_FUNCTION(vkCreateBufferView);
VK_FUNCTION(vkCreateImage);
VK_FUNCTION(vkGetImageSubresourceLayout);
VK_FUNCTION(vkCreateImageView);
VK_FUNCTION(vkCreateShaderModule);
VK_FUNCTION(vkCreatePipelineCache);
VK_FUNCTION(vkGetPipelineCacheData);
VK_FUNCTION(vkMergePipelineCaches);
VK_FUNCTION(vkCreateGraphicsPipelines);
VK_FUNCTION(vkCreateComputePipelines);
VK_FUNCTION(vkCreatePipelineLayout);
VK_FUNCTION(vkCreateSampler);
VK_FUNCTION(vkCreateDescriptorSetLayout);
VK_FUNCTION(vkCreateDescriptorPool);
VK_FUNCTION(vkResetDescriptorPool);
VK_FUNCTION(vkAllocateDescriptorSets);
VK_FUNCTION(vkFreeDescriptorSets);
VK_FUNCTION(vkUpdateDescriptorSets);
VK_FUNCTION(vkAllocateCommandBuffers);
VK_FUNCTION(vkBeginCommandBuffer);
VK_FUNCTION(vkEndCommandBuffer);
VK_FUNCTION(vkResetCommandBuffer);
VK_FUNCTION(vkCreateCommandPool);
VK_FUNCTION(vkDestroyFence);
VK_FUNCTION(vkDestroySemaphore);
VK_FUNCTION(vkDestroyEvent);
VK_FUNCTION(vkDestroyQueryPool);
VK_FUNCTION(vkDestroyBuffer);
VK_FUNCTION(vkDestroyBufferView);
VK_FUNCTION(vkDestroyImage);
VK_FUNCTION(vkDestroyImageView);
VK_FUNCTION(vkDestroyShaderModule);
VK_FUNCTION(vkDestroyPipelineCache);
VK_FUNCTION(vkDestroyPipeline);
VK_FUNCTION(vkDestroyPipelineLayout);
VK_FUNCTION(vkDestroySampler);
VK_FUNCTION(vkDestroyDescriptorSetLayout);
VK_FUNCTION(vkDestroyDescriptorPool);
VK_FUNCTION(vkDestroyFramebuffer);
VK_FUNCTION(vkDestroyRenderPass);
VK_FUNCTION(vkFreeCommandBuffers);
VK_FUNCTION(vkDestroyCommandPool);
VK_FUNCTION(vkCmdBindPipeline);
VK_FUNCTION(vkCmdSetViewport);
VK_FUNCTION(vkCmdSetScissor);
VK_FUNCTION(vkCmdSetLineWidth);
VK_FUNCTION(vkCmdSetDepthBias);
VK_FUNCTION(vkCmdSetBlendConstants);
VK_FUNCTION(vkCmdSetDepthBounds);
VK_FUNCTION(vkCmdSetStencilCompareMask);
VK_FUNCTION(vkCmdSetStencilWriteMask);
VK_FUNCTION(vkCmdSetStencilReference);
VK_FUNCTION(vkCmdBindDescriptorSets);
VK_FUNCTION(vkCmdBindIndexBuffer);
VK_FUNCTION(vkCmdBindVertexBuffers);
VK_FUNCTION(vkCmdDraw);
VK_FUNCTION(vkCmdDrawIndexed);
VK_FUNCTION(vkCmdDrawIndirect);
VK_FUNCTION(vkCmdDrawIndexedIndirect);
VK_FUNCTION(vkCmdDispatch);
VK_FUNCTION(vkCmdDispatchIndirect);
VK_FUNCTION(vkCmdCopyBuffer);
VK_FUNCTION(vkCmdCopyImage);
VK_FUNCTION(vkCmdBlitImage);
VK_FUNCTION(vkCmdCopyBufferToImage);
VK_FUNCTION(vkCmdCopyImageToBuffer);
VK_FUNCTION(vkCmdUpdateBuffer);
VK_FUNCTION(vkCmdFillBuffer);
VK_FUNCTION(vkCmdClearColorImage);
VK_FUNCTION(vkCmdClearAttachments);
VK_FUNCTION(vkCmdClearDepthStencilImage);
VK_FUNCTION(vkCmdResolveImage);
VK_FUNCTION(vkCmdSetEvent);
VK_FUNCTION(vkCmdResetEvent);
VK_FUNCTION(vkCmdWaitEvents);
VK_FUNCTION(vkCmdPipelineBarrier);
VK_FUNCTION(vkCmdBeginQuery);
VK_FUNCTION(vkCmdEndQuery);
VK_FUNCTION(vkCmdResetQueryPool);
VK_FUNCTION(vkCmdWriteTimestamp);
VK_FUNCTION(vkCmdCopyQueryPoolResults);
VK_FUNCTION(vkCreateFramebuffer);
VK_FUNCTION(vkCreateRenderPass);
VK_FUNCTION(vkCmdBeginRenderPass);
VK_FUNCTION(vkCmdEndRenderPass);
VK_FUNCTION(vkCmdExecuteCommands);
VK_FUNCTION(vkCmdPushConstants);

// VK_KHR_swapchain
VK_FUNCTION(vkCreateSwapchainKHR);
VK_FUNCTION(vkDestroySwapchainKHR);
VK_FUNCTION(vkGetSwapchainImagesKHR);
VK_FUNCTION(vkAcquireNextImageKHR);
VK_FUNCTION(vkQueuePresentKHR);

#endif
