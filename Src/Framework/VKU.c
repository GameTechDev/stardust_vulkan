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

#include "VKU.h"
#include <stdlib.h>
#include <stdio.h>
#include "stretchy_buffer.h"
#include "Misc.h"

//=============================================================================
int VKU_Create_Buffer_Memory_Pool(
    VkDevice                   device,
    VkQueue                    queue,
    const VkMemoryAllocateInfo *alloc,
    VKU_BUFFER_MEMORY_POOL     **mempool)
{
    if (!device || !alloc || !mempool) LOG_AND_RETURN0();

    VkDeviceMemory mem;
    VKU_VR(vkAllocateMemory(device, alloc, NO_ALLOC_CALLBACK, &mem));

    VKU_BUFFER_MEMORY_POOL *mpool = malloc(sizeof(*mpool));
    if (!mpool) {
        if (mem) { vkFreeMemory(device, mem, NO_ALLOC_CALLBACK); mem = VK_NULL_HANDLE; }
        LOG_AND_RETURN0();
    }

    mpool->device = device;
    mpool->queue  = queue;
    mpool->memory = mem;
    mpool->offset = 0;
    mpool->size = alloc->allocationSize;
    mpool->buffers = NULL;

    *mempool = mpool;

    return 1;
}
//-----------------------------------------------------------------------------
int VKU_Create_Image_Memory_Pool(
    VkDevice                   device,
    VkQueue                    queue,
    const VkMemoryAllocateInfo *alloc,
    VKU_IMAGE_MEMORY_POOL      **mempool)
{
    if (!device || !alloc || !mempool) LOG_AND_RETURN0();

    VkDeviceMemory mem;
    VKU_VR(vkAllocateMemory(device, alloc, NO_ALLOC_CALLBACK, &mem));

    VKU_IMAGE_MEMORY_POOL *mpool = malloc(sizeof(*mpool));
    if (!mpool) {
        if (mem) { vkFreeMemory(device, mem, NO_ALLOC_CALLBACK); mem = VK_NULL_HANDLE; }
        LOG_AND_RETURN0();
    }

    mpool->device = device;
    mpool->queue = queue;
    mpool->memory = mem;
    mpool->offset = 0;
    mpool->size = alloc->allocationSize;
    mpool->images = NULL;

    *mempool = mpool;

    return 1;
}
//-----------------------------------------------------------------------------
int VKU_Free_Buffer_Memory_Pool(VKU_BUFFER_MEMORY_POOL *mempool)
{
    if (!mempool) LOG_AND_RETURN0();

    sb_free(mempool->buffers);
    mempool->buffers = NULL;

    if (mempool->memory) {
        vkFreeMemory(mempool->device, mempool->memory, NO_ALLOC_CALLBACK);
        mempool->memory = VK_NULL_HANDLE;
    }
    free(mempool);

    return 1;
}
int VKU_Free_Image_Memory_Pool(VKU_IMAGE_MEMORY_POOL *mempool)
{
    if (!mempool) LOG_AND_RETURN0();

    sb_free(mempool->images);
    mempool->images = NULL;


    if (mempool->memory) { vkFreeMemory(mempool->device, mempool->memory, NO_ALLOC_CALLBACK); mempool->memory = VK_NULL_HANDLE; }
    free(mempool);

    return 1;
}
//-----------------------------------------------------------------------------
int VKU_Alloc_Buffer_Memory(
    VKU_BUFFER_MEMORY_POOL *mempool,
    VkDeviceSize            size,
    VkDeviceSize            alignment,
    VkDeviceSize           *offset)
{
    if (!mempool || size == 0 || !offset) LOG_AND_RETURN0();
    if (alignment == 0) alignment = 1;

    VkDeviceSize r = mempool->offset % alignment;
    VkDeviceSize offset_change = r > 0 ? alignment - r : 0;
    if (mempool->offset + offset_change + size > mempool->size) LOG_AND_RETURN0();

    mempool->offset += offset_change;
    *offset = mempool->offset;
    mempool->offset += size;

    return 1;
}
//-----------------------------------------------------------------------------
int VKU_Alloc_Image_Memory(
    VKU_IMAGE_MEMORY_POOL *mempool,
    VkDeviceSize           size,
    VkDeviceSize           alignment,
    VkDeviceSize          *offset)
{
    if (!mempool || size == 0 || !offset) LOG_AND_RETURN0();
    if (alignment == 0) alignment = 1;

    VkDeviceSize r = mempool->offset % alignment;
    VkDeviceSize offset_change = r > 0 ? alignment - r : 0;
    if (mempool->offset + offset_change + size > mempool->size) LOG_AND_RETURN0();

    mempool->offset += offset_change;
    *offset = mempool->offset;
    mempool->offset += size;

    return 1;
}
//-----------------------------------------------------------------------------
int VKU_Alloc_Buffer_Object(VKU_BUFFER_MEMORY_POOL *mempool,
    VkBuffer          buffer,
    VkDeviceSize     *offset)
{
    if (!mempool || !buffer) LOG_AND_RETURN0();

    VkMemoryRequirements mreq;
    vkGetBufferMemoryRequirements(mempool->device, buffer, &mreq);

    if (mreq.size > 0) {
        VkDeviceSize off;
        if (!VKU_Alloc_Buffer_Memory(mempool, mreq.size, mreq.alignment, &off))
            LOG_AND_RETURN0();

        VKU_VR(vkBindBufferMemory(mempool->device, buffer, mempool->memory, off));

        sb_push(mempool->buffers, buffer);
        if (offset) *offset = off;
    }

    return 1;
}
//-----------------------------------------------------------------------------
int VKU_Alloc_Image_Object(VKU_IMAGE_MEMORY_POOL *mempool,
    VkImage           image,
    VkDeviceSize     *offset,
	uint32_t          memtypeindex)
{
    if (!mempool || !image) LOG_AND_RETURN0();

	VkMemoryRequirements mreq;
	vkGetImageMemoryRequirements(mempool->device, image, &mreq);

    if (mreq.size > 0) {
        VkDeviceSize off;
        if (!VKU_Alloc_Image_Memory(mempool, mreq.size, mreq.alignment, &off))
            LOG_AND_RETURN0();

		VkMemoryAllocateInfo mem_alloc;
		mem_alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		mem_alloc.pNext = NULL;
		mem_alloc.allocationSize = mreq.size;
		mem_alloc.memoryTypeIndex = memtypeindex;

		VKU_VR(vkAllocateMemory(mempool->device, &mem_alloc, NULL, &mempool->memory));
		VKU_VR(vkBindImageMemory(mempool->device, image, mempool->memory, 0));

        sb_push(mempool->images, image);
        if (offset) *offset = off;
    }

    return 1;
}
//=============================================================================
int VKU_Load_Shader(VkDevice device,
    const char *filename,
    VkShaderModule *shaderModule)
{
    if (!device || !filename || !shaderModule) LOG_AND_RETURN0();

    FILE *fp = fopen(filename, "rb");
    if (!fp) LOG_AND_RETURN0();

    fseek(fp, 0, SEEK_END);
    int size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    unsigned char *bytecode = malloc(size);

    if (!bytecode) return 0;

    fread(bytecode, 1, size, fp);
    fclose(fp);

    VkShaderModule sm;
    const VkShaderModuleCreateInfo shader_module_info = {
        VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, NULL, 0, size, (uint32_t*)bytecode
    };

    VkResult res = vkCreateShaderModule(device, &shader_module_info, NO_ALLOC_CALLBACK, &sm);
    if (res != VK_SUCCESS) {
        free(bytecode);
        LOG_AND_RETURN0();
    }

    free(bytecode);
    *shaderModule = sm;

    return 1;
}
//=============================================================================
// Define function pointers
#define VK_FUNCTION(func) PFN_##func func = NULL
#include "vkFuncList.h"
#undef VK_FUNCTION
//=============================================================================
int VK__Load_Api(void *(*Loader)(const char *))
{
    // Load function pointers
#define VK_FUNCTION(func) if (!(func = (PFN_##func)Loader(#func))) { LOG_AND_RETURN0(); }
#define _VK_NO_INIT_FUNCTIONS
#include "vkFuncList.h"
#undef _VK_NO_INIT_FUNCTIONS
#undef VK_FUNCTION
    return 1;
}
//=============================================================================
int VK__Load_Init_Api(void *(*Loader)(const char *))
{
    // Load function pointers
#define VK_FUNCTION(func) if (!(func = (PFN_##func)Loader(#func))) { LOG_AND_RETURN0(); }
#define _VK_NO_DEVICE_FUNCTIONS
#include "vkFuncList.h"
#undef _VK_NO_DEVICE_FUNCTIONS
#undef VK_FUNCTION
    return 1;
}
//=============================================================================
