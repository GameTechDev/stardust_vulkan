// Copyright 2016 Intel Corporation All Rights Reserved
// 
// Intel makes no representations about the suitability of this software for any purpose.
// THIS SOFTWARE IS PROVIDED ""AS IS."" INTEL SPECIFICALLY DISCLAIMS ALL WARRANTIES,
// EXPRESS OR IMPLIED, AND ALL LIABILITY, INCLUDING CONSEQUENTIAL AND OTHER INDIRECT DAMAGES,
// FOR THE USE OF THIS SOFTWARE, INCLUDING LIABILITY FOR INFRINGEMENT OF ANY PROPRIETARY
// RIGHTS, AND INCLUDING THE WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
// Intel does not assume any responsibility for any errors which may appear in this software
// nor any responsibility to update it.

#pragma once

#if defined(_WIN32) || defined(_WIN64)
#define VK_USE_PLATFORM_WIN32_KHR
#define VK_KHR_PLATFORM_SPECIFIC_SURFACE_EXTENSION_NAME VK_KHR_WIN32_SURFACE_EXTENSION_NAME
#elif defined(__ANDROID__)
#define VK_USE_PLATFORM_ANDROID_KHR
#define VK_KHR_PLATFORM_SPECIFIC_SURFACE_EXTENSION_NAME VK_KHR_ANDROID_SURFACE_EXTENSION_NAME
#endif

#include "vulkan.h"
#include "Misc.h"

#define NO_ALLOC_CALLBACK (VkAllocationCallbacks*)NULL
#define VKU_PAGE_SIZE (1024*64)
#define VKU_ALIGN(v, a) (((v) % (a)) ? ((v) + ((a) - ((v) % (a)))) : (v))
#define VKU_ALIGN_PAGE(v) VKU_ALIGN(v, VKU_PAGE_SIZE)
#define VKU_FREE_MEM(mem) if (mem) { vkFreeMemory(s_gpu_device, mem, NO_ALLOC_CALLBACK); mem = VK_NULL_HANDLE; }
#define VKU_FREE_CMD_BUF(cmdPool, count, cmdBuf) if (cmdBuf[0]) { vkFreeCommandBuffers(s_gpu_device, cmdPool, count, cmdBuf); cmdBuf[0] = VK_NULL_HANDLE; }
#define VKU_DESTROY(f, obj) if (obj) { f(s_gpu_device, obj, NO_ALLOC_CALLBACK); obj = VK_NULL_HANDLE; }
#define VKU_VR(f) { VkResult _r = (f); if (_r < 0) { Log("%s failed with result: 0x%x", #f, _r); LOG_AND_RETURN0();} }

typedef struct VKU_BUFFER_MEMORY_POOL
{
  VkDevice       device;
  VkQueue        queue;
  VkDeviceMemory memory;
  VkDeviceSize   size;
  VkDeviceSize   offset;
  VkBuffer      *buffers;
} VKU_BUFFER_MEMORY_POOL;

typedef struct VKU_IMAGE_MEMORY_POOL
{
    VkDevice       device;
    VkQueue        queue;
    VkDeviceMemory memory;
    VkDeviceSize   size;
    VkDeviceSize   offset;
    VkImage       *images;
} VKU_IMAGE_MEMORY_POOL;

int VKU_Create_Device(void          *hwnd,
                      int          width,
                      int          height,
                      int          win_image_count,
                      VkBool32     windowed,
                      uint32_t     *queue_family_index,
                      VkDevice     *device,
                      VkQueue      *queue,
                      VkImage      *images,
                      VkSwapchainKHR       *swap_chain,
                      VkPhysicalDevice     *gpu);

void VKU_Quit(void);

int VKU_Present(uint32_t *image_indice);

int VKU_Create_Buffer_Memory_Pool(VkDevice                   device,
                                  VkQueue                    queue,
                                  const VkMemoryAllocateInfo *alloc,
                                  VKU_BUFFER_MEMORY_POOL     **mempool);

int VKU_Create_Image_Memory_Pool(VkDevice                   device,
                                 VkQueue                    queue,
                                 const VkMemoryAllocateInfo *alloc,
                                 VKU_IMAGE_MEMORY_POOL      **mempool);

int VKU_Free_Buffer_Memory_Pool(VKU_BUFFER_MEMORY_POOL *mempool);

int VKU_Free_Image_Memory_Pool(VKU_IMAGE_MEMORY_POOL *mempool);

int VKU_Alloc_Buffer_Memory(VKU_BUFFER_MEMORY_POOL *mempool,
                            VkDeviceSize           size,
                            VkDeviceSize           alignment,
                            VkDeviceSize           *offset);

int VKU_Alloc_Image_Memory(VKU_IMAGE_MEMORY_POOL *mempool,
                           VkDeviceSize          size,
                           VkDeviceSize          alignment,
                           VkDeviceSize          *offset);

int VKU_Alloc_Buffer_Object(VKU_BUFFER_MEMORY_POOL *objpool,
                            VkBuffer               buffer,
                            VkDeviceSize           *offset);

int VKU_Alloc_Image_Object(VKU_IMAGE_MEMORY_POOL *objpool,
                           VkImage               image,
                           VkDeviceSize          *offset,
						   uint32_t              memtypeindex);


int VKU_Load_Shader(VkDevice        device,
                    const char      *filename,
                    VkShaderModule  *shaderModule);
//=============================================================================
// GPU initialization
#define VK_FUNCTION(func) extern PFN_##func func
#include "vkFuncList.h"
#undef VK_FUNCTION
//=============================================================================
