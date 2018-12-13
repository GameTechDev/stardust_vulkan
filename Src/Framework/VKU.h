/////////////////////////////////////////////////////////////////////////////////////////////
// Copyright 2017 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
/////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#if defined(_WIN32) || defined(_WIN64)
#define VK_USE_PLATFORM_WIN32_KHR
#define VK_KHR_PLATFORM_SPECIFIC_SURFACE_EXTENSION_NAME VK_KHR_WIN32_SURFACE_EXTENSION_NAME
#elif defined(__ANDROID__)
#define VK_USE_PLATFORM_ANDROID_KHR
#define VK_KHR_PLATFORM_SPECIFIC_SURFACE_EXTENSION_NAME VK_KHR_ANDROID_SURFACE_EXTENSION_NAME
#endif

#define VK_NO_PROTOTYPES
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

int VK__Load_Global_Api(void *(*Loader)(const char *));

int VK__Load_Device_Api(VkDevice device);

int VK__Load_Instance_Api(VkInstance instance);

//=============================================================================
// GPU initialization
#define VK_FUNCTION(func) extern PFN_##func func
#define _VK_ALL_FUNCTIONS
#include "vkFuncList.h"
#undef _VK_ALL_FUNCTIONS
#undef VK_FUNCTION
//=============================================================================
