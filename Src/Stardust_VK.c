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

#include "Settings.h"
#include <string.h>
#include "SDL.h"
#include "VKU.h"
#include "Misc.h"
#include "Graph.h"
#include "Stardust.h"
#include "vectormath_aos.h"
#include "Metrics.h"
#include "stretchy_buffer.h"
#include "stb_image.h"
#include "stb_font_consolas_24_usascii.inl"
#include <math.h>

#if !defined(_MSC_VER)
#include <signal.h>
#define __debugbreak() raise(SIGTRAP)
#endif

#if defined(NDEBUG)
#define VK_ASSERT(e)
#else
#define VK_ASSERT(e) if(!(e)) { __debugbreak(); }
#endif

#if defined(_WIN32)
#include <windows.h>
#endif
//=============================================================================
#define k_Window_Buffering k_Resource_Buffering
#define DRAW_PER_THREAD ((s_glob_state->point_count / s_glob_state->batch_size) / s_glob_state->cpu_core_count)
#define MT_UPDATE
//=============================================================================
typedef struct ViewportState
{
    uint32_t                           viewportCount;
    VkViewport                         viewport;
    uint32_t                           scissorCount;
    VkRect2D                           scissors;
} ViewportState;

typedef struct RasterState
{
    float                              depthBias;
    float                              depthBiasClamp;
    float                              slopeScaledDepthBias;
    float                              lineWidth;
} RasterState;

typedef struct ColorBlendState
{
    float                              blendConst[4];
} ColorBlendState;

typedef struct DepthStencilState
{
    float                              minDepthBounds;
    float                              maxDepthBounds;
} DepthStencilState;

typedef struct THREAD_DATA
{
    int                                 tid;
    VkCommandBuffer                     cmdbuf[k_Resource_Buffering];
    VkCommandPool                       cmdpool;
    SDL_sem                             *sem;
    SDL_Thread                          *thread;
} THREAD_DATA;

typedef struct GRAPH_SHADER_IN
{
    VmathVector2                       p;
    VmathVector4                       c;
} GRAPH_SHADER_IN;

typedef struct GRAPH
{
    VkDeviceMemory                     buffer_mem[k_Resource_Buffering];
    VkBuffer                           buffer[k_Resource_Buffering];
    int                                x, y, w, h;
    float                              color[4];
    ViewportState                      viewport;
    int                                draw_background;
} GRAPH;
//=============================================================================
static int                            Demo_Init(void);
static int                            Demo_Shutdown(void);
static int                            Demo_Update(void);
static void                           Update_Camera(void);
static int                            Update_Constant_Memory(void);
static int                            Create_Depth_Stencil(void);
static int                            Create_Common_Dset(void);
static void                           Update_Common_Dset(void);
static int                            Create_Constant_Memory(void);
static int                            Create_Particles(void);
static int                            Create_Skybox_Geometry(void);
static int                            Create_Float_Renderpass(void);
static int                            Init_Dynamic_States(void);
static int                            Create_Skybox_Pipeline(void);
static int                            Create_Skybox_Generate_Pipeline(void);
static int                            Create_Particle_Pipeline(void);
static int                            Create_Display_Renderpass(void);
static int                            Create_Display_Pipeline(void);
static int                            Create_Copy_Renderpass(void);
static int                            Create_Copy_Pipeline(void);
static int                            Create_Window_Framebuffer(void);
static int                            Create_Float_Image_And_Framebuffer(void);
static int                            Create_Skybox_Image(void);
static int                            Create_Palette_Images(void);
static int                            Render_To_Skybox_Image(void);
static int                            Render_To_Palette_Images(void);
static int                            Init_Particle_Thread(THREAD_DATA *thrd);
static int                            Finish_Particle_Thread(THREAD_DATA *thrd);
static int                            Release_Particle_Thread(THREAD_DATA *thrd);
static int                            Update_Particle_Thread(THREAD_DATA *thrd);
static int                            SDLCALL Particle_Thread(void *data);
static void                           Cmd_Clear(VkCommandBuffer cmdbuf);
static void                           Cmd_Begin_Win_RenderPass(VkCommandBuffer cmdbuf);
static void                           Cmd_End_Win_RenderPass(VkCommandBuffer cmdbuf);
static void                           Cmd_Display_Fractal(VkCommandBuffer cmdbuf);
static void                           Cmd_Render_Skybox(VkCommandBuffer cmdbuf);
//-----------------------------------------------------------------------------
static int                            Graph_Init(GRAPH *graph, struct graph_data_t *data, int x, int y, int w, int h, float color[4], int draw_background);
static void                           Graph_Release(GRAPH *graph);
static void                           Graph_Draw(GRAPH *graph, VkCommandBuffer cmdbuf);
static int                            Graph_Update_Buffer(GRAPH *graph, struct graph_data_t* data);
//-----------------------------------------------------------------------------
static int                            Create_Common_Graph_Resources(void);
static int                            Update_Common_Graph_Resources(void);
static int                            Create_Graph_Pipeline(VkPrimitiveTopology topology, VkPipeline *pipe);
//-----------------------------------------------------------------------------
static int                            Create_Font_Resources(void);
static int                            Create_Font_Pipeline(void);
static int                            Generate_Text(void);
static void                           Cmd_Draw_Text(VkCommandBuffer cmdbuf);
//-----------------------------------------------------------------------------
static uint32_t                       Get_Mem_Type_Index(VkMemoryPropertyFlagBits bit);
//-----------------------------------------------------------------------------
static int                            s_exit_code;
static __inline int                   Set_Exit_Code(int exit_code)
{
    VK_ASSERT(exit_code != STARDUST_ERROR);
    return s_exit_code = exit_code;
}
//=============================================================================
static struct glob_state_t              *s_glob_state;
static double                           s_time;
static float                            s_time_delta;
static float                            s_fps;
static float                            s_ms;
static int                              s_res_idx;
static int                              s_win_idx;
static VkDevice                         s_gpu_device;
static VkQueue                          s_gpu_queue;
static VkPhysicalDevice                 s_gpu;
static VkPhysicalDeviceProperties       s_gpu_properties;
static VkImage                          s_win_images[k_Window_Buffering];
static VkImageView                      s_win_image_view[k_Window_Buffering];
static VkFramebuffer                    s_win_framebuffer[k_Window_Buffering];
static VkRenderPass                     s_win_renderpass;
static VkImage                          s_depth_stencil_image;
static VkImageView                      s_depth_stencil_view;
static VKU_BUFFER_MEMORY_POOL           *s_buffer_mempool_target;
static VKU_BUFFER_MEMORY_POOL           *s_buffer_mempool_state;
static VKU_BUFFER_MEMORY_POOL           *s_buffer_mempool_texture;
static VKU_IMAGE_MEMORY_POOL            *s_image_mempool_target;
static VKU_IMAGE_MEMORY_POOL            *s_image_mempool_state;
static VKU_IMAGE_MEMORY_POOL            *s_image_mempool_texture;
static VkCommandPool                    s_command_pool;
static VkCommandBuffer                  s_cmdbuf_display[k_Resource_Buffering];
static VkCommandBuffer                  s_cmdbuf_clear[k_Resource_Buffering];
static VkFence                          s_fence[k_Resource_Buffering];
static VkDescriptorPool                 s_common_dpool;
static VkDescriptorSetLayout            s_common_dset_layout;
static VkPipelineLayout                 s_common_pipeline_layout;
static VkDescriptorSet                  s_common_dset[k_Resource_Buffering];
static VkDeviceMemory                   s_constant_mem[k_Resource_Buffering];
static VkBuffer                         s_constant_buf[k_Resource_Buffering];
static VkDeviceMemory                   s_particle_seed_mem;
static VkBuffer                         s_particle_seed_buf;
static VkPipeline                       s_particle_pipe;
static VkImage                          s_float_image;
static VkImageView                      s_float_image_view;
static VkFramebuffer                    s_float_framebuffer;
static VkRenderPass                     s_float_renderpass;
static VkPipeline                       s_display_pipe;
static VkSampler                        s_sampler;
static VkSampler                        s_sampler_repeat;
static VkSampler                        s_sampler_nearest;
static VkImage                          s_skybox_image;
static VkImageView                      s_skybox_image_view;
static VkPipeline                       s_skybox_pipe;
static VkPipeline                       s_skybox_generate_pipe;
static VkDeviceMemory                   s_skybox_mem;
static VkBuffer                         s_skybox_buf;
static VkRenderPass                     s_copy_renderpass;
static VkPipeline                       s_copy_image_pipe;
static VkFramebuffer                    s_copy_framebuffer;
static VmathVector3                     s_camera_position;
static VmathVector3                     s_camera_direction;
static VmathVector3                     s_camera_right;
static float                            s_camera_pitch;
static float                            s_camera_yaw;
static VmathMatrix4                     s_transform_a[3];
static VmathMatrix4                     s_transform_b[3];
static VkImage                          s_palette_image[6];
static VkImageView                      s_palette_image_view[6];
static THREAD_DATA                      s_thread[MAX_CPU_CORES];
static SDL_sem                          *s_cmdgen_sem[MAX_CPU_CORES];
static uint32_t                         s_queue_family_index;
static VkSwapchainKHR                   s_swap_chain;
static VkSemaphore                      s_swap_chain_image_ready_semaphore;
static ViewportState                    s_vp_state;
static ViewportState                    s_vp_state_copy_skybox;
static ViewportState                    s_vp_state_copy_palette;
static ViewportState                    s_vp_state_legend_cpu;
static ViewportState                    s_vp_state_legend_gpu;
static RasterState                      s_rs_state;
static ColorBlendState                  s_cb_state;
static DepthStencilState                s_ds_state;
//-------------------------------------------------------------------------------
static VkDeviceMemory                   s_graph_buffer_mem[k_Resource_Buffering];
static VkBuffer                         s_graph_buffer[k_Resource_Buffering];
static VkPipeline                       s_graph_tri_strip_pipe;
static VkPipeline                       s_graph_line_list_pipe;
static VkPipeline                       s_graph_line_strip_pipe;
static GRAPH                            s_graph[MAX_CPU_CORES];
//-------------------------------------------------------------------------------
static VkImage                          s_font_image;
static VkImageView                      s_font_image_view;
static VkDeviceMemory                   s_font_image_mem;
static VkBuffer                         s_font_buffer[k_Resource_Buffering];
static VkDeviceMemory                   s_font_buffer_mem[k_Resource_Buffering];
static VkPipeline                       s_font_pipe;
static stb_fontchar                     s_font_24_data[STB_FONT_consolas_24_usascii_NUM_CHARS];
static int                              s_font_letter_count;
//-------------------------------------------------------------------------------
#define VM_PI 3.141592654f
#define VM_2PI 6.283185307f
#define VM_1DIVPI 0.318309886f
#define VM_1DIV2PI 0.159154943f
#define VM_PIDIV2 1.570796327f
#define VM_PIDIV4 0.785398163f
//=============================================================================
static int Demo_Init(void)
{
    s_font_letter_count = 0;

    vmathV3MakeFromElems(&s_camera_position, 24.0f, 24.0f, 10.0f);
    s_camera_pitch = -VM_PIDIV4;
    s_camera_yaw = 1.5f * VM_PIDIV4;

    VkCommandPoolCreateInfo command_pool_info;
    command_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    command_pool_info.pNext = NULL;
    command_pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    command_pool_info.queueFamilyIndex = s_queue_family_index;
    VKU_VR(vkCreateCommandPool(s_gpu_device, &command_pool_info, NO_ALLOC_CALLBACK, &s_command_pool));

    VkCommandBufferAllocateInfo cmdbuf_info = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, NULL, s_command_pool,
        VK_COMMAND_BUFFER_LEVEL_PRIMARY, k_Resource_Buffering
    };
    VKU_VR(vkAllocateCommandBuffers(s_gpu_device, &cmdbuf_info, s_cmdbuf_display));
    VKU_VR(vkAllocateCommandBuffers(s_gpu_device, &cmdbuf_info, s_cmdbuf_clear));

    /* render targets object pool */ {
        VkMemoryAllocateInfo alloc_info = {
            VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, VKU_ALIGN_PAGE(s_glob_state->width * s_glob_state->height * 32),
            Get_Mem_Type_Index(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
        };
        if (!VKU_Create_Buffer_Memory_Pool(s_gpu_device, s_gpu_queue, &alloc_info, &s_buffer_mempool_target))
            LOG_AND_RETURN0();
        if (!VKU_Create_Image_Memory_Pool(s_gpu_device, s_gpu_queue, &alloc_info, &s_image_mempool_target))
            LOG_AND_RETURN0();
    }
    /* state objects pool */ {
        VkMemoryAllocateInfo alloc_info = {
            VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, 1024 * 1024 * 8,
            Get_Mem_Type_Index(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
        };
        if (!VKU_Create_Buffer_Memory_Pool(s_gpu_device, s_gpu_queue, &alloc_info, &s_buffer_mempool_state))
            LOG_AND_RETURN0();
        if (!VKU_Create_Image_Memory_Pool(s_gpu_device, s_gpu_queue, &alloc_info, &s_image_mempool_state))
            LOG_AND_RETURN0();
    }
    /* texture pool */ {
        VkMemoryAllocateInfo alloc_info = {
            VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, 1024 * 1024 * 32,
            Get_Mem_Type_Index(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
        };
        if (!VKU_Create_Buffer_Memory_Pool(s_gpu_device, s_gpu_queue, &alloc_info, &s_buffer_mempool_texture))
            LOG_AND_RETURN0();
        if (!VKU_Create_Image_Memory_Pool(s_gpu_device, s_gpu_queue, &alloc_info, &s_image_mempool_texture))
            LOG_AND_RETURN0();
    }

    VkSamplerCreateInfo sampler_info0 = {
        VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, NULL, 0,
        VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_NEAREST,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 0.0f, VK_FALSE, 0, VK_FALSE, VK_COMPARE_OP_ALWAYS, 0.0f, 0.0f,
        VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE, VK_FALSE
    };
    VKU_VR(vkCreateSampler(s_gpu_device, &sampler_info0, NO_ALLOC_CALLBACK, &s_sampler));

    VkSamplerCreateInfo sampler_info1 = {
        VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, NULL, 0,
        VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_NEAREST,
        VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT,
        0.0f, VK_FALSE, 0, VK_FALSE, VK_COMPARE_OP_ALWAYS, 0.0f, 0.0f,
        VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE, VK_FALSE
    };
    VKU_VR(vkCreateSampler(s_gpu_device, &sampler_info1, NO_ALLOC_CALLBACK, &s_sampler_repeat));

    VkSamplerCreateInfo sampler_info2 = {
        VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, NULL, 0,
        VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_NEAREST,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 0.0f, VK_FALSE, 0, VK_FALSE, VK_COMPARE_OP_ALWAYS, 0.0f, 0.0f,
        VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE, VK_FALSE
    };
    VKU_VR(vkCreateSampler(s_gpu_device, &sampler_info2, NO_ALLOC_CALLBACK, &s_sampler_nearest));

    if (!Create_Depth_Stencil()) LOG_AND_RETURN0();
    if (!Create_Common_Dset()) LOG_AND_RETURN0();
    if (!Create_Particles()) LOG_AND_RETURN0();
    if (!Create_Float_Renderpass()) LOG_AND_RETURN0();
    if (!Init_Dynamic_States()) LOG_AND_RETURN0();
    if (!Create_Particle_Pipeline()) LOG_AND_RETURN0();
    if (!Create_Display_Renderpass()) LOG_AND_RETURN0();
    if (!Create_Display_Pipeline()) LOG_AND_RETURN0();
    if (!Create_Graph_Pipeline(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, &s_graph_tri_strip_pipe)) LOG_AND_RETURN0();
    if (!Create_Graph_Pipeline(VK_PRIMITIVE_TOPOLOGY_LINE_STRIP, &s_graph_line_strip_pipe)) LOG_AND_RETURN0();
    if (!Create_Graph_Pipeline(VK_PRIMITIVE_TOPOLOGY_LINE_LIST, &s_graph_line_list_pipe)) LOG_AND_RETURN0();
    if (!Create_Window_Framebuffer()) LOG_AND_RETURN0();
    if (!Create_Constant_Memory()) LOG_AND_RETURN0();
    if (!Create_Float_Image_And_Framebuffer()) LOG_AND_RETURN0();
    if (!Create_Copy_Renderpass()) LOG_AND_RETURN0();
    if (!Create_Copy_Pipeline()) LOG_AND_RETURN0();
    if (!Create_Font_Resources()) LOG_AND_RETURN0();
    if (!Create_Font_Pipeline()) LOG_AND_RETURN0();
    if (!Create_Skybox_Geometry()) LOG_AND_RETURN0();
    if (!Create_Skybox_Pipeline()) LOG_AND_RETURN0();
    if (!Create_Skybox_Generate_Pipeline()) LOG_AND_RETURN0();
    if (!Create_Skybox_Image()) LOG_AND_RETURN0();
    if (!Create_Palette_Images()) LOG_AND_RETURN0();
    if (!Render_To_Skybox_Image()) LOG_AND_RETURN0();
    if (!Render_To_Palette_Images()) LOG_AND_RETURN0();
    if (!Create_Common_Graph_Resources()) LOG_AND_RETURN0();

    float green[4] = { 0.0f, 0.85f, 0.0f, 1.0f };

    // cpu load graphs
    for (int i = 0; i < s_glob_state->cpu_core_count; ++i) {
        if (!Graph_Init(&s_graph[i], &s_glob_state->graph_data[i], s_glob_state->width - 10 - k_Graph_Width, 36 + i * 92,
                        k_Graph_Width, 88, green, 1)) return 0;
    }

    s_glob_state->palette_image_idx %= 5;

    for (int i = 0; i < s_glob_state->cpu_core_count; ++i) {
        s_cmdgen_sem[i] = SDL_CreateSemaphore(0);
        if (!s_cmdgen_sem[i]) LOG_AND_RETURN0();
    }
    for (int i = 0; i < s_glob_state->cpu_core_count; ++i) {
        s_thread[i].tid = i;
        if (!Init_Particle_Thread(&s_thread[i])) LOG_AND_RETURN0();
    }

    return 1;
}
//=============================================================================
static int Demo_Shutdown(void)
{
    if (s_gpu_device) vkDeviceWaitIdle(s_gpu_device);

    for (int i = 0; i < s_glob_state->cpu_core_count; ++i) {
        Release_Particle_Thread(&s_thread[i]);
    }
    for (int i = 0; i < s_glob_state->cpu_core_count; ++i) {
        SDL_DestroySemaphore(s_cmdgen_sem[i]);
        s_cmdgen_sem[i] = NULL;
    }
    VKU_Free_Buffer_Memory_Pool(s_buffer_mempool_state);
    VKU_Free_Buffer_Memory_Pool(s_buffer_mempool_target);
    VKU_Free_Buffer_Memory_Pool(s_buffer_mempool_texture);
    s_buffer_mempool_state = NULL;
    s_buffer_mempool_target = NULL;
    s_buffer_mempool_texture = NULL;

    VKU_Free_Image_Memory_Pool(s_image_mempool_state);
    VKU_Free_Image_Memory_Pool(s_image_mempool_target);
    VKU_Free_Image_Memory_Pool(s_image_mempool_texture);
    s_image_mempool_state = NULL;
    s_image_mempool_target = NULL;
    s_image_mempool_texture = NULL;

    VKU_DESTROY(vkDestroyBuffer, s_particle_seed_buf);
    VKU_FREE_MEM(s_particle_seed_mem);

    for (int i = 0; i < k_Resource_Buffering; ++i) {
        VKU_DESTROY(vkDestroyBuffer, s_constant_buf[i]);
        VKU_FREE_MEM(s_constant_mem[i]);

        VKU_DESTROY(vkDestroyFence, s_fence[i]);
        VKU_FREE_CMD_BUF(s_command_pool, k_Resource_Buffering, s_cmdbuf_clear);
        VKU_FREE_CMD_BUF(s_command_pool, k_Resource_Buffering, s_cmdbuf_display);
    }
    VKU_DESTROY(vkDestroyPipelineLayout, s_common_pipeline_layout);
    VKU_DESTROY(vkDestroyDescriptorSetLayout, s_common_dset_layout);
    VKU_DESTROY(vkDestroyDescriptorPool, s_common_dpool);

    VKU_DESTROY(vkDestroyRenderPass, s_copy_renderpass);
    VKU_DESTROY(vkDestroyRenderPass, s_float_renderpass);
    VKU_DESTROY(vkDestroyFramebuffer, s_float_framebuffer);
    VKU_DESTROY(vkDestroyImageView, s_float_image_view);
    VKU_DESTROY(vkDestroyImage, s_float_image);
    VKU_DESTROY(vkDestroyPipeline, s_particle_pipe);
    VKU_DESTROY(vkDestroyPipeline, s_display_pipe);
    VKU_DESTROY(vkDestroyPipeline, s_graph_tri_strip_pipe);
    VKU_DESTROY(vkDestroyPipeline, s_graph_line_strip_pipe);
    VKU_DESTROY(vkDestroyPipeline, s_graph_line_list_pipe);
    VKU_DESTROY(vkDestroySampler, s_sampler);
    VKU_DESTROY(vkDestroySampler, s_sampler_repeat);
    VKU_DESTROY(vkDestroySampler, s_sampler_nearest);
    VKU_DESTROY(vkDestroyImageView, s_skybox_image_view);
    VKU_DESTROY(vkDestroyImage, s_skybox_image);
    VKU_DESTROY(vkDestroyBuffer, s_skybox_buf);
    VKU_DESTROY(vkDestroyPipeline, s_skybox_pipe);
    VKU_DESTROY(vkDestroyPipeline, s_skybox_generate_pipe);
    VKU_FREE_MEM(s_skybox_mem);
    for (int i = 0; i < SDL_arraysize(s_palette_image); ++i) {
        VKU_DESTROY(vkDestroyImageView, s_palette_image_view[i]);
        VKU_DESTROY(vkDestroyImage, s_palette_image[i]);
    }

    VKU_DESTROY(vkDestroyImageView, s_font_image_view);
    VKU_DESTROY(vkDestroyImage, s_font_image);
    VKU_FREE_MEM(s_font_image_mem);
    VKU_DESTROY(vkDestroyPipeline, s_font_pipe);

    for (int i = 0; i < k_Resource_Buffering; ++i) {
        VKU_DESTROY(vkDestroyBuffer, s_graph_buffer[i]);
        VKU_FREE_MEM(s_graph_buffer_mem[i]);
        VKU_DESTROY(vkDestroyBuffer, s_font_buffer[i]);
        VKU_FREE_MEM(s_font_buffer_mem[i]);
    }
    for (int i = 0; i < s_glob_state->cpu_core_count; ++i) Graph_Release(&s_graph[i]);

    for (int i = 0; i < k_Window_Buffering; ++i) {
        VKU_DESTROY(vkDestroyFramebuffer, s_win_framebuffer[i]);
        VKU_DESTROY(vkDestroyImageView, s_win_image_view[i]);
    }
    VKU_DESTROY(vkDestroyRenderPass, s_win_renderpass);
    VKU_DESTROY(vkDestroyImageView, s_depth_stencil_view);
    VKU_DESTROY(vkDestroyImage, s_depth_stencil_image);

    VKU_DESTROY(vkDestroyCommandPool, s_command_pool);
    VKU_DESTROY(vkDestroySemaphore, s_swap_chain_image_ready_semaphore);

    return 1;
}
//=============================================================================
static int Demo_Update(void)
{
    if (s_fence[s_res_idx]) {
        VKU_VR(vkWaitForFences(s_gpu_device, 1, &s_fence[s_res_idx], VK_TRUE, 100000000));
    }

    Update_Camera();
    if (!Update_Constant_Memory()) LOG_AND_RETURN0();
    Update_Common_Dset();

    for (int i = 0; i < s_glob_state->cpu_core_count; ++i) {
        if (!Graph_Update_Buffer(&s_graph[i], &s_glob_state->graph_data[i])) LOG_AND_RETURN0();
    }
    if (!Update_Common_Graph_Resources()) LOG_AND_RETURN0();
    if (!Generate_Text()) LOG_AND_RETURN0();

#ifdef MT_UPDATE
    for (int i = 1; i < s_glob_state->cpu_core_count; ++i) {
        SDL_SemPost(s_cmdgen_sem[i]);
    }
#endif

    VkCommandBufferBeginInfo begin_info = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL, 0, NULL
    };
    VKU_VR(vkBeginCommandBuffer(s_cmdbuf_clear[s_res_idx], &begin_info));
    Cmd_Clear(s_cmdbuf_clear[s_res_idx]);
    Cmd_Render_Skybox(s_cmdbuf_clear[s_res_idx]);
    VKU_VR(vkEndCommandBuffer(s_cmdbuf_clear[s_res_idx]));

    VKU_VR(vkBeginCommandBuffer(s_cmdbuf_display[s_res_idx], &begin_info));
    Cmd_Begin_Win_RenderPass(s_cmdbuf_display[s_res_idx]);
    Cmd_Display_Fractal(s_cmdbuf_display[s_res_idx]);
    for (int i = 0; i < s_glob_state->cpu_core_count; ++i) {
        Graph_Draw(&s_graph[i], s_cmdbuf_display[s_res_idx]);
    }
    Cmd_Draw_Text(s_cmdbuf_display[s_res_idx]);
    Cmd_End_Win_RenderPass(s_cmdbuf_display[s_res_idx]);
    VKU_VR(vkEndCommandBuffer(s_cmdbuf_display[s_res_idx]));

#ifdef MT_UPDATE
    Update_Particle_Thread(&s_thread[0]);
    for (int i = 1; i < s_glob_state->cpu_core_count; ++i) {
        SDL_SemWait(s_thread[i].sem);
    }
#else
    for (int i = 0; i < s_glob_state->cpu_core_count; ++i) {
        Update_Particle_Thread(&s_thread[i]);
    }
#endif

    if (!s_fence[s_res_idx]) {
        VkFenceCreateInfo fence_info = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, NULL, 0 };
        VKU_VR(vkCreateFence(s_gpu_device, &fence_info, NO_ALLOC_CALLBACK, &s_fence[s_res_idx]));
    }

    VkCommandBuffer cmdbuf[2 + MAX_CPU_CORES];
    int cmdbuf_count = 0;

    cmdbuf[cmdbuf_count++] = s_cmdbuf_clear[s_res_idx];
    for (int i = 0; i < s_glob_state->cpu_core_count; ++i) {
        cmdbuf[cmdbuf_count++] = s_thread[i].cmdbuf[s_res_idx];
    }
    cmdbuf[cmdbuf_count++] = s_cmdbuf_display[s_res_idx];

    VKU_VR(vkResetFences(s_gpu_device, 1, &s_fence[s_res_idx]));

    VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_HOST_BIT };
    VkSubmitInfo submit_info;
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.pNext = NULL;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &s_swap_chain_image_ready_semaphore;
    submit_info.pWaitDstStageMask = wait_stages;
    submit_info.commandBufferCount = cmdbuf_count;
    submit_info.pCommandBuffers = cmdbuf;
    submit_info.signalSemaphoreCount = 0;
    submit_info.pSignalSemaphores = NULL;
    VKU_VR(vkQueueSubmit(s_gpu_queue, 1, &submit_info, s_fence[s_res_idx]));

    return 1;
}
//=============================================================================
static void Update_Camera(void)
{
    const float move_scale = 10.0f * s_time_delta;

    VmathVector3 forward, up;
    vmathV3MakeFromElems(&forward, 0.0f, 0.0f, -1.0f);
    vmathV3MakeFromElems(&up, 0.0f, 1.0f, 0.0f);

    VmathVector3 rot;
    vmathV3MakeFromElems(&rot, s_camera_pitch, s_camera_yaw, 0.0f);

    VmathMatrix3 trans;
    vmathM3MakeRotationZYX(&trans, &rot);

    vmathM3MulV3(&s_camera_direction, &trans, &forward);
    vmathV3Cross(&s_camera_right, &s_camera_direction, &up);
    vmathV3Normalize(&s_camera_right, &s_camera_right);
}
//=============================================================================
static int Update_Constant_Memory(void)
{
    typedef struct CONSTANT
    {
        VmathMatrix4 viewproj;
        unsigned int data[48];
        float palette_factor;
    } CONSTANT;
    CONSTANT *ptr;
    VKU_VR(vkMapMemory(s_gpu_device, s_constant_mem[s_res_idx], 0, sizeof(CONSTANT), 0, (void **)&ptr));

    VmathMatrix4 rot, view, proj;
    VmathVector3 up, atv;
    VmathPoint3 eye, at;
    vmathP3MakeFromV3(&eye, &s_camera_position);
    vmathV3Add(&atv, &s_camera_position, &s_camera_direction);
    vmathP3MakeFromV3(&at, &atv);
    vmathV3MakeFromElems(&up, 0.0f, 1.0f, 0.0f);
    vmathM4MakeRotationY(&rot, (float)s_time * 0.04f);
    vmathM4MakeLookAt(&view, &eye, &at, &up);
    vmathM4MakePerspective(&proj, VM_PI / 3, (float)s_glob_state->width / s_glob_state->height, 0.1f, 100.0f);

    VmathMatrix4 m;
    vmathM4Mul(&m, &view, &rot);
    vmathM4Mul(&ptr->viewproj, &proj, &m);

    float tt = s_glob_state->transform_time * s_glob_state->transform_time * (3.0f - 2.0f * s_glob_state->transform_time);
    if (s_glob_state->transform_animate) {
        s_glob_state->palette_factor += s_time_delta * 0.25f;
        if (s_glob_state->palette_factor > 1.0f) {
            for (int i = 0; i < 9; ++i) {
                RND_GEN(s_glob_state->seed);
            }
            s_glob_state->palette_factor = 0.0f;
            s_glob_state->palette_image_idx = (s_glob_state->palette_image_idx + 1) % 5;
        }
    }
    ptr->palette_factor = s_glob_state->palette_factor * s_glob_state->palette_factor * (3.0f - 2.0f * s_glob_state->palette_factor);

    if (s_glob_state->transform_animate) {
        s_glob_state->transform_time += s_time_delta * 0.2f;

        if (s_glob_state->transform_time > 1.0f) {
            s_glob_state->transform_time = 0.0f;
        }
    }

    ptr->data[0] = s_glob_state->seed;
    vkUnmapMemory(s_gpu_device, s_constant_mem[s_res_idx]);

    return 1;
}
//=============================================================================
static int Create_Depth_Stencil(void)
{
    VkImageCreateInfo ds_info = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, NULL, 0, VK_IMAGE_TYPE_2D,
        VK_FORMAT_D24_UNORM_S8_UINT, { s_glob_state->width, s_glob_state->height, 1 }, 1, 1, VK_SAMPLE_COUNT_1_BIT,
        VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        VK_SHARING_MODE_EXCLUSIVE, 0, NULL, VK_IMAGE_LAYOUT_UNDEFINED
    };
    VKU_VR(vkCreateImage(s_gpu_device, &ds_info, NO_ALLOC_CALLBACK, &s_depth_stencil_image));
    if (!VKU_Alloc_Image_Object(s_image_mempool_target, s_depth_stencil_image, NULL, Get_Mem_Type_Index(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))) LOG_AND_RETURN0();

    VkComponentMapping channels = {
        VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A 
    };
    VkImageSubresourceRange subres_range = {
        VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1
    };
    VkImageViewCreateInfo ds_view_info = {
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, NULL, 0,
        s_depth_stencil_image, VK_IMAGE_VIEW_TYPE_2D,
        VK_FORMAT_D24_UNORM_S8_UINT, channels, subres_range
    };
    VKU_VR(vkCreateImageView(s_gpu_device, &ds_view_info, NO_ALLOC_CALLBACK, &s_depth_stencil_view));

    return 1;
}
//=============================================================================
static int Create_Common_Dset(void)
{
    VkDescriptorSetLayoutBinding desc6_info = {
       6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, NULL
    };
    VkDescriptorSetLayoutBinding desc5_info = {
       5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL
    };
    VkDescriptorSetLayoutBinding desc4_info = {
       4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL
    };
    VkDescriptorSetLayoutBinding desc3_info = {
       3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL
    };
    VkDescriptorSetLayoutBinding desc2_info = {
       2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL
    };
    VkDescriptorSetLayoutBinding desc1_info = {
       1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL
    };
    VkDescriptorSetLayoutBinding desc0_info = {
       0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, NULL
    };
    VkDescriptorSetLayoutBinding infos[7];
    infos[0] = desc0_info;
    infos[1] = desc1_info;
    infos[2] = desc2_info;
    infos[3] = desc3_info;
    infos[4] = desc4_info;
    infos[5] = desc5_info;
    infos[6] = desc6_info;

    VkDescriptorSetLayoutCreateInfo set_info = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, NULL, 0,
        SDL_arraysize(infos), infos
    };

    VKU_VR(vkCreateDescriptorSetLayout(s_gpu_device, &set_info, NO_ALLOC_CALLBACK, &s_common_dset_layout));

    VkPipelineLayoutCreateInfo pipeline_layout_info; 
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.pNext = NULL;
    pipeline_layout_info.flags = 0;
    pipeline_layout_info.setLayoutCount = 1;
    pipeline_layout_info.pSetLayouts = &s_common_dset_layout;
    pipeline_layout_info.pushConstantRangeCount = 0;
    pipeline_layout_info.pPushConstantRanges = NULL;

    VKU_VR(vkCreatePipelineLayout(s_gpu_device, &pipeline_layout_info, NO_ALLOC_CALLBACK, &s_common_pipeline_layout));

    VkDescriptorPoolSize desc_type_count[] = {
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2 * k_Resource_Buffering },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 5 * k_Resource_Buffering }
    };
    VkDescriptorPoolCreateInfo pool_info = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, NULL, 0,
        k_Resource_Buffering, SDL_arraysize(desc_type_count), desc_type_count
    };
    VKU_VR(vkCreateDescriptorPool(s_gpu_device, &pool_info, NO_ALLOC_CALLBACK, &s_common_dpool));

    const VkDescriptorSetAllocateInfo desc_set_alloc_info = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, NULL,
        s_common_dpool, 1, &s_common_dset_layout
    };

    for (int i = 0; i < k_Resource_Buffering; i++)
        VKU_VR(vkAllocateDescriptorSets(s_gpu_device, &desc_set_alloc_info, &s_common_dset[i]));

    return 1;
}
//=============================================================================
static void Update_Common_Dset(void)
{
    VkDescriptorImageInfo font_image_sampler_info = {
        s_sampler_nearest, s_font_image_view, VK_IMAGE_LAYOUT_GENERAL
    };
    VkDescriptorImageInfo palette1_image_sampler_info = {
        s_sampler_repeat, s_palette_image_view[(s_glob_state->palette_image_idx + 1) % 5], VK_IMAGE_LAYOUT_GENERAL
    };
    VkDescriptorImageInfo palette0_image_sampler_info = {
        s_sampler_repeat, s_palette_image_view[s_glob_state->palette_image_idx], VK_IMAGE_LAYOUT_GENERAL
    };
    VkDescriptorImageInfo skybox_image_sampler_info = {
        s_sampler, s_skybox_image_view, VK_IMAGE_LAYOUT_GENERAL
    };
    VkDescriptorImageInfo float_image_sampler_info = {
        s_sampler, s_float_image_view, VK_IMAGE_LAYOUT_GENERAL
    };

    VkDescriptorBufferInfo constant_buf_info = {
        s_constant_buf[s_res_idx], 0, 4 * sizeof(VmathMatrix4) + sizeof(float)
    };

    VkDescriptorBufferInfo skybox_buf_info = {
        s_skybox_buf, 0, 14 * sizeof(VmathVector4)
    };

    VkWriteDescriptorSet update_skybox_buffers = {
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, s_common_dset[s_res_idx],
        6, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_NULL_HANDLE,
        &skybox_buf_info, VK_NULL_HANDLE
    };
    VkWriteDescriptorSet update_sampler_font_image = {
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, s_common_dset[s_res_idx],
        5, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &font_image_sampler_info,
        VK_NULL_HANDLE, VK_NULL_HANDLE
    };
    VkWriteDescriptorSet update_sampler_palette1_image = {
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, s_common_dset[s_res_idx],
        4, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &palette1_image_sampler_info,
        VK_NULL_HANDLE, VK_NULL_HANDLE
    };
    VkWriteDescriptorSet update_sampler_palette0_image = {
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, s_common_dset[s_res_idx],
        3, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &palette0_image_sampler_info,
        VK_NULL_HANDLE, VK_NULL_HANDLE
    };
    VkWriteDescriptorSet update_sampler_skybox_image = {
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, s_common_dset[s_res_idx],
        2, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &skybox_image_sampler_info,
        VK_NULL_HANDLE, VK_NULL_HANDLE
    };
    VkWriteDescriptorSet update_sampler_float_image = {
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, s_common_dset[s_res_idx],
        1, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &float_image_sampler_info,
        VK_NULL_HANDLE, VK_NULL_HANDLE
    };
    VkWriteDescriptorSet update_buffers = {
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, s_common_dset[s_res_idx],
        0, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_NULL_HANDLE,
        &constant_buf_info, VK_NULL_HANDLE
    };

    VkWriteDescriptorSet write_descriptors[7];
    write_descriptors[0] = update_buffers;
    write_descriptors[1] = update_sampler_float_image;
    write_descriptors[2] = update_sampler_skybox_image;
    write_descriptors[3] = update_sampler_palette0_image;
    write_descriptors[4] = update_sampler_palette1_image;
    write_descriptors[5] = update_sampler_font_image;
    write_descriptors[6] = update_skybox_buffers;

    vkUpdateDescriptorSets(s_gpu_device, SDL_arraysize(write_descriptors), write_descriptors, 0, NULL);
}
//=============================================================================
static int Create_Constant_Memory(void)
{
    VkBufferCreateInfo buffer_info = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, NULL, 0, 4 * sizeof(VmathMatrix4) + sizeof(float),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE, 0, NULL
    };
    VkMemoryAllocateInfo alloc_info = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, 0,
        Get_Mem_Type_Index(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
    };

    for (int i = 0; i < k_Resource_Buffering; ++i) {
        VKU_VR(vkCreateBuffer(s_gpu_device, &buffer_info, NO_ALLOC_CALLBACK, &s_constant_buf[i]));
        VkMemoryRequirements mem_reqs;
        vkGetBufferMemoryRequirements(s_gpu_device, s_constant_buf[i], &mem_reqs);
        alloc_info.allocationSize = mem_reqs.size;
        VKU_VR(vkAllocateMemory(s_gpu_device, &alloc_info, NO_ALLOC_CALLBACK, &s_constant_mem[i]));
        VKU_VR(vkBindBufferMemory(s_gpu_device, s_constant_buf[i], s_constant_mem[i], 0));
    }

    return 1;
}
//=============================================================================
static int Create_Particles(void)
{
    VkBufferCreateInfo buffer_info = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, NULL, 0, s_glob_state->point_count * sizeof(uint32_t),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE, 0, NULL
    };
    VKU_VR(vkCreateBuffer(s_gpu_device, &buffer_info, NO_ALLOC_CALLBACK, &s_particle_seed_buf));

    VkMemoryRequirements mem_reqs;
    vkGetBufferMemoryRequirements(s_gpu_device, s_particle_seed_buf, &mem_reqs);
    if (mem_reqs.size < s_glob_state->point_count * sizeof(uint32_t))
    {
        return 0;
    }
    VkMemoryAllocateInfo alloc_info = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL,
        mem_reqs.size,
        Get_Mem_Type_Index(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
    };
    VKU_VR(vkAllocateMemory(s_gpu_device, &alloc_info, NO_ALLOC_CALLBACK, &s_particle_seed_mem));
    VKU_VR(vkBindBufferMemory(s_gpu_device, s_particle_seed_buf, s_particle_seed_mem, 0));


    unsigned int seed = 23232323;
    uint32_t *pseed = malloc(s_glob_state->point_count * sizeof(*pseed));
    if (!pseed) LOG_AND_RETURN0();

    for (int i = 0; i < s_glob_state->point_count; ++i) {
        RND_GEN(seed);
        pseed[i] = seed;
    }

    void *ptr;
    VKU_VR(vkMapMemory(s_gpu_device, s_particle_seed_mem, 0, s_glob_state->point_count * sizeof(*pseed), 0, &ptr));
    memcpy(ptr, pseed, s_glob_state->point_count * sizeof(*pseed));
    vkUnmapMemory(s_gpu_device, s_particle_seed_mem);
    free(pseed);

    return 1;
}
//=============================================================================
static int Create_Skybox_Geometry(void)
{
    VkDeviceSize size = 14 * sizeof(VmathVector4);
    VkBufferCreateInfo buffer_info = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, NULL, 0, size,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_SHARING_MODE_EXCLUSIVE, 0, NULL
    };
    VKU_VR(vkCreateBuffer(s_gpu_device, &buffer_info, NO_ALLOC_CALLBACK, &s_skybox_buf));

    VkMemoryRequirements mem_reqs;
    vkGetBufferMemoryRequirements(s_gpu_device, s_skybox_buf, &mem_reqs);
    if (mem_reqs.size < size)
    {
        return 0;
    }
    VkMemoryAllocateInfo alloc_info = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, mem_reqs.size,
        Get_Mem_Type_Index(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
    };
    VKU_VR(vkAllocateMemory(s_gpu_device, &alloc_info, NO_ALLOC_CALLBACK, &s_skybox_mem));
    VKU_VR(vkBindBufferMemory(s_gpu_device, s_skybox_buf, s_skybox_mem, 0));

    return 1;
}
//=============================================================================
static int Create_Float_Renderpass(void)
{
    VkAttachmentLoadOp color_ld_op = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    VkAttachmentStoreOp color_st_op = VK_ATTACHMENT_STORE_OP_STORE;
    VkFormat colorFormat = VK_FORMAT_R8G8B8A8_UNORM;
    VkImageLayout layout = VK_IMAGE_LAYOUT_GENERAL;

    VkAttachmentDescription color_attachment_desc;
    color_attachment_desc.flags = 0;
    color_attachment_desc.format = colorFormat;
    color_attachment_desc.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment_desc.loadOp = color_ld_op;
    color_attachment_desc.storeOp = color_st_op;
    color_attachment_desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment_desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment_desc.initialLayout = layout;
    color_attachment_desc.finalLayout = layout;

    VkAttachmentDescription ds_attachment_desc;
    ds_attachment_desc.flags = 0;
    ds_attachment_desc.format = VK_FORMAT_D24_UNORM_S8_UINT;
    ds_attachment_desc.samples = VK_SAMPLE_COUNT_1_BIT;
    ds_attachment_desc.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    ds_attachment_desc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    ds_attachment_desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    ds_attachment_desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    ds_attachment_desc.initialLayout = layout;
    ds_attachment_desc.finalLayout = layout;

    VkAttachmentReference color_attachment_ref = { 0, layout };
    VkAttachmentReference ds_attachment_ref = { 1, layout };

    VkSubpassDescription subpass_desc;
    subpass_desc.flags = 0;
    subpass_desc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass_desc.inputAttachmentCount = 0;
    subpass_desc.pInputAttachments = NULL;
    subpass_desc.colorAttachmentCount = 1;
    subpass_desc.pColorAttachments = &color_attachment_ref;
    subpass_desc.pResolveAttachments = NULL;
    subpass_desc.pDepthStencilAttachment = &ds_attachment_ref;
    subpass_desc.preserveAttachmentCount = 0;
    subpass_desc.pPreserveAttachments = NULL;

    VkAttachmentDescription attachment_descs[2];
    attachment_descs[0] = color_attachment_desc;
    attachment_descs[1] = ds_attachment_desc;

    VkRenderPassCreateInfo render_info;
    render_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_info.pNext = NULL;
    render_info.flags = 0;
    render_info.attachmentCount = 2;
    render_info.pAttachments = attachment_descs;
    render_info.subpassCount = 1;
    render_info.pSubpasses = &subpass_desc;
    render_info.dependencyCount = 0;
    render_info.pDependencies = NULL;

    VKU_VR(vkCreateRenderPass(s_gpu_device, &render_info, NO_ALLOC_CALLBACK, &s_float_renderpass));

    return 1;
}
//=============================================================================
static int Create_Skybox_Pipeline(void)
{
    VkShaderModule vs, fs;
    vs = VK_NULL_HANDLE;
    fs = VK_NULL_HANDLE;

    VKU_Load_Shader(s_gpu_device, "Data/Shader_GLSL/VS_Skybox.bil", &vs);
    VKU_Load_Shader(s_gpu_device, "Data/Shader_GLSL/FS_Skybox.bil", &fs);

    if (vs == VK_NULL_HANDLE || fs == VK_NULL_HANDLE)
    {
        VKU_DESTROY(vkDestroyShaderModule, vs);
        VKU_DESTROY(vkDestroyShaderModule, fs);
        LOG_AND_RETURN0();
    }

    VkPipelineShaderStageCreateInfo vs_info = {
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        NULL, 0, VK_SHADER_STAGE_VERTEX_BIT, vs, "main", NULL
    };
    VkPipelineShaderStageCreateInfo fs_info = {
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        NULL, 0, VK_SHADER_STAGE_FRAGMENT_BIT, fs, "main", NULL
    };
    VkPipelineInputAssemblyStateCreateInfo ia_info = {
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, NULL, 0,
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, VK_FALSE
    };
    VkPipelineTessellationStateCreateInfo tess_info = {
        VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO, NULL, 0, 0
    };
    VkPipelineViewportStateCreateInfo vp_info = {
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, NULL, 0, s_vp_state.viewportCount,
        &s_vp_state.viewport, s_vp_state.scissorCount, &s_vp_state.scissors
    };
    VkPipelineRasterizationStateCreateInfo rs_info = {
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, NULL, 0, VK_FALSE,
        VK_FALSE, VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE,
        VK_FALSE, s_rs_state.depthBias, s_rs_state.depthBiasClamp,
        s_rs_state.slopeScaledDepthBias, s_rs_state.lineWidth
    };
    VkPipelineMultisampleStateCreateInfo ms_info = {
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, NULL, 0, 1, VK_FALSE,
        1.0f, NULL, VK_FALSE, VK_FALSE
    };
    VkPipelineColorBlendAttachmentState attachment = {
        VK_FALSE, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD,
        VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD, 0x0f
    };
    VkPipelineColorBlendStateCreateInfo cb_info = {
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, NULL, 0, VK_FALSE,
        VK_LOGIC_OP_COPY, 1, &attachment, s_cb_state.blendConst[0]
    };
    VkPipelineDepthStencilStateCreateInfo db_info = {
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, NULL, 0,
        VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL, VK_FALSE, VK_FALSE,
        { VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 0, 0, 0 },
        { VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 0, 0, 0 },
        s_ds_state.minDepthBounds, s_ds_state.maxDepthBounds
    };

    VkVertexInputBindingDescription vf_binding_desc = {
        0, sizeof(VmathVector4), VK_VERTEX_INPUT_RATE_VERTEX
    };
    VkVertexInputAttributeDescription vf_attribute_desc[] = {
        {
            0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 0
        },
    };
    VkPipelineVertexInputStateCreateInfo vf_info = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, NULL, 0,
        1, &vf_binding_desc, SDL_arraysize(vf_attribute_desc), vf_attribute_desc
    };

    VkPipelineShaderStageCreateInfo shader_stage_infos[2];
    shader_stage_infos[0] = vs_info;
    shader_stage_infos[1] = fs_info;

    VkGraphicsPipelineCreateInfo pi_info = {
        VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, NULL, 0, 2, shader_stage_infos,
        &vf_info, &ia_info, &tess_info, &vp_info, &rs_info, &ms_info, &db_info, &cb_info,
        NULL, s_common_pipeline_layout, s_float_renderpass, 0, VK_NULL_HANDLE, 0
    };

    VkResult r = vkCreateGraphicsPipelines(s_gpu_device, VK_NULL_HANDLE, 1, &pi_info, NO_ALLOC_CALLBACK, &s_skybox_pipe);

    VKU_DESTROY(vkDestroyShaderModule, vs);
    VKU_DESTROY(vkDestroyShaderModule, fs);

    if (r != VK_SUCCESS) LOG_AND_RETURN0();

    return 1;
}
//=============================================================================
static int Create_Skybox_Generate_Pipeline(void)
{
    VkShaderModule cs;
    cs = VK_NULL_HANDLE;
    VKU_Load_Shader(s_gpu_device, "Data/Shader_GLSL/CS_Skybox_Generate.bil", &cs);

    if (cs == VK_NULL_HANDLE)
    {
        VKU_DESTROY(vkDestroyShaderModule, cs);
        LOG_AND_RETURN0();
    }

    VkPipelineShaderStageCreateInfo stage_info;
    stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage_info.pNext = NULL;
    stage_info.flags = 0;
    stage_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stage_info.module = cs;
    stage_info.pName = "main";
    stage_info.pSpecializationInfo = NULL;

    VkComputePipelineCreateInfo infoPipe;
    infoPipe.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    infoPipe.pNext = NULL;
    infoPipe.flags = 0;
    infoPipe.stage = stage_info;
    infoPipe.layout = s_common_pipeline_layout;
    infoPipe.basePipelineHandle = VK_NULL_HANDLE;
    infoPipe.basePipelineIndex = 0;

    VkResult r = vkCreateComputePipelines(s_gpu_device, VK_NULL_HANDLE, 1, &infoPipe, NO_ALLOC_CALLBACK, &s_skybox_generate_pipe);
    VKU_DESTROY(vkDestroyShaderModule, cs);
    if (r != VK_SUCCESS) LOG_AND_RETURN0();

    return 1;
}
//=============================================================================
static int Create_Particle_Pipeline(void)
{
    VkShaderModule vs, fs;
    vs = VK_NULL_HANDLE;
    fs = VK_NULL_HANDLE;

    VKU_Load_Shader(s_gpu_device, "Data/Shader_GLSL/VS_Particle_Draw.bil", &vs);
    VKU_Load_Shader(s_gpu_device, "Data/Shader_GLSL/FS_Particle_Draw.bil", &fs);

    if (vs == VK_NULL_HANDLE || fs == VK_NULL_HANDLE)
    {
        VKU_DESTROY(vkDestroyShaderModule, vs);
        VKU_DESTROY(vkDestroyShaderModule, fs);
        LOG_AND_RETURN0();
    }

    VkPipelineShaderStageCreateInfo vs_info = {
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        NULL, 0, VK_SHADER_STAGE_VERTEX_BIT, vs, "main", NULL
    };
    VkPipelineShaderStageCreateInfo fs_info = {
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        NULL, 0, VK_SHADER_STAGE_FRAGMENT_BIT, fs, "main", NULL
    };
    VkPipelineInputAssemblyStateCreateInfo ia_info = {
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, NULL, 0,
        VK_PRIMITIVE_TOPOLOGY_POINT_LIST, VK_FALSE
    };
    VkPipelineTessellationStateCreateInfo tess_info = {
        VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO, NULL, 0, 0
    };
    VkPipelineViewportStateCreateInfo vp_info = {
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, NULL, 0, s_vp_state.viewportCount,
        &s_vp_state.viewport, s_vp_state.scissorCount, &s_vp_state.scissors
    };
    VkPipelineRasterizationStateCreateInfo rs_info = {
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, NULL, 0, VK_FALSE,
        VK_FALSE, VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_FALSE,
        s_rs_state.depthBias, s_rs_state.depthBiasClamp,
        s_rs_state.slopeScaledDepthBias, s_rs_state.lineWidth
    };
    VkPipelineMultisampleStateCreateInfo ms_info = {
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, NULL, 0, 1, VK_FALSE,
        1.0f, NULL, VK_FALSE, VK_FALSE
    };
    VkPipelineColorBlendAttachmentState attachment = {
        VK_TRUE, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE, VK_BLEND_OP_ADD,
        VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE, VK_BLEND_OP_ADD, 0x0f
    };
    VkPipelineColorBlendStateCreateInfo cb_info = {
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, NULL, 0, VK_FALSE,
        VK_LOGIC_OP_COPY, 1, &attachment, s_cb_state.blendConst[0]
    };
    VkPipelineDepthStencilStateCreateInfo db_info = {
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, NULL, 0,
        VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL, VK_FALSE, VK_FALSE,
        { VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 0, 0, 0 },
        { VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 0, 0, 0 },
        s_ds_state.minDepthBounds, s_ds_state.maxDepthBounds
    };

    VkVertexInputBindingDescription vf_binding_desc = {
        0, sizeof(uint32_t), VK_VERTEX_INPUT_RATE_VERTEX
    };
    VkVertexInputAttributeDescription vf_attribute_desc[] = {
        {
            0, 0, VK_FORMAT_R32_UINT, 0
        },
    };
    VkPipelineVertexInputStateCreateInfo vf_info = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, NULL, 0,
        1, &vf_binding_desc, SDL_arraysize(vf_attribute_desc), vf_attribute_desc
    };

    VkPipelineShaderStageCreateInfo shader_stage_infos[2];
    shader_stage_infos[0] = vs_info;
    shader_stage_infos[1] = fs_info;

    VkGraphicsPipelineCreateInfo pi_info = {
        VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, NULL, 0, 2, shader_stage_infos,
        &vf_info, &ia_info, &tess_info, &vp_info, &rs_info, &ms_info, &db_info, &cb_info,
        NULL, s_common_pipeline_layout, s_float_renderpass, 0, VK_NULL_HANDLE, 0
    };

    VkResult r = vkCreateGraphicsPipelines(s_gpu_device, VK_NULL_HANDLE, 1, &pi_info, NO_ALLOC_CALLBACK, &s_particle_pipe);

    VKU_DESTROY(vkDestroyShaderModule, vs);
    VKU_DESTROY(vkDestroyShaderModule, fs);

    if (r != VK_SUCCESS) LOG_AND_RETURN0();

    return 1;
}
//=============================================================================
static int Create_Display_Renderpass(void)
{
    VkAttachmentLoadOp color_ld_op = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    VkAttachmentStoreOp color_st_op = VK_ATTACHMENT_STORE_OP_STORE;
    VkFormat colorFormat = VK_FORMAT_R8G8B8A8_UNORM;
    VkImageLayout layout = VK_IMAGE_LAYOUT_GENERAL;

    VkAttachmentDescription color_attachment_desc;
    color_attachment_desc.flags = 0;
    color_attachment_desc.format = colorFormat;
    color_attachment_desc.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment_desc.loadOp = color_ld_op;
    color_attachment_desc.storeOp = color_st_op;
    color_attachment_desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment_desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment_desc.initialLayout = layout;
    color_attachment_desc.finalLayout = layout;

    VkAttachmentDescription ds_attachment_desc;
    ds_attachment_desc.flags = 0;
    ds_attachment_desc.format = VK_FORMAT_D24_UNORM_S8_UINT;
    ds_attachment_desc.samples = VK_SAMPLE_COUNT_1_BIT;
    ds_attachment_desc.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    ds_attachment_desc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    ds_attachment_desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    ds_attachment_desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    ds_attachment_desc.initialLayout = layout;
    ds_attachment_desc.finalLayout = layout;

    VkAttachmentReference color_attachment_ref = { 0, layout };
    VkAttachmentReference ds_attachment_ref = { 1, layout };

    VkSubpassDescription subpass_desc;
    subpass_desc.flags = 0;
    subpass_desc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass_desc.inputAttachmentCount = 0;
    subpass_desc.pInputAttachments = NULL;
    subpass_desc.colorAttachmentCount = 1;
    subpass_desc.pColorAttachments = &color_attachment_ref;
    subpass_desc.pResolveAttachments = NULL;
    subpass_desc.pDepthStencilAttachment = &ds_attachment_ref;
    subpass_desc.preserveAttachmentCount = 0;
    subpass_desc.pPreserveAttachments = NULL;

    VkAttachmentDescription attachment_descs[2];
    attachment_descs[0] = color_attachment_desc;
    attachment_descs[1] = ds_attachment_desc;

    VkRenderPassCreateInfo render_info;
    render_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_info.pNext = NULL;
    render_info.flags = 0;
    render_info.attachmentCount = 2;
    render_info.pAttachments = attachment_descs;
    render_info.subpassCount = 1;
    render_info.pSubpasses = &subpass_desc;
    render_info.dependencyCount = 0;
    render_info.pDependencies = NULL;

    VKU_VR(vkCreateRenderPass(s_gpu_device, &render_info, NO_ALLOC_CALLBACK, &s_win_renderpass));

    return 1;
}
//=============================================================================
static int Create_Display_Pipeline(void)
{
    VkShaderModule vs, fs;
    vs = VK_NULL_HANDLE;
    fs = VK_NULL_HANDLE;

    VKU_Load_Shader(s_gpu_device, "Data/Shader_GLSL/VS_Quad_UL.bil", &vs);
    VKU_Load_Shader(s_gpu_device, "Data/Shader_GLSL/FS_Display.bil", &fs);

    if (vs == VK_NULL_HANDLE || fs == VK_NULL_HANDLE)
    {
        VKU_DESTROY(vkDestroyShaderModule, vs);
        VKU_DESTROY(vkDestroyShaderModule, fs);
        LOG_AND_RETURN0();
    }

    VkPipelineShaderStageCreateInfo vs_info = {
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        NULL, 0, VK_SHADER_STAGE_VERTEX_BIT, vs, "main", NULL
    };
    VkPipelineShaderStageCreateInfo fs_info = {
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        NULL, 0, VK_SHADER_STAGE_FRAGMENT_BIT, fs, "main", NULL
    };
    VkPipelineVertexInputStateCreateInfo vf_info = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, NULL, 0,
        0, NULL, 0, NULL
    };
    VkPipelineInputAssemblyStateCreateInfo ia_info = {
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, NULL, 0,
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, VK_FALSE
    };
    VkPipelineTessellationStateCreateInfo tess_info = {
        VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO, NULL, 0, 0
    };
    VkPipelineViewportStateCreateInfo vp_info = {
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, NULL, 0, s_vp_state.viewportCount,
        &s_vp_state.viewport, s_vp_state.scissorCount, &s_vp_state.scissors
    };
    VkPipelineRasterizationStateCreateInfo rs_info = {
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, NULL, 0, VK_FALSE,
        VK_FALSE, VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_FALSE,
        s_rs_state.depthBias, s_rs_state.depthBiasClamp,
        s_rs_state.slopeScaledDepthBias, s_rs_state.lineWidth
    };
    VkPipelineMultisampleStateCreateInfo ms_info = {
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, NULL, 0, 1, VK_FALSE,
        1.0f, NULL, VK_FALSE, VK_FALSE
    };
    VkPipelineColorBlendAttachmentState attachment = {
        VK_FALSE, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD, 
        VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD, 0x0f
    };
    VkPipelineColorBlendStateCreateInfo cb_info = {
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, NULL, 0, VK_FALSE,
        VK_LOGIC_OP_COPY, 1, &attachment, s_cb_state.blendConst[0]
    };
    VkPipelineDepthStencilStateCreateInfo db_info = {
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, NULL, 0,
        VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL, VK_FALSE, VK_FALSE,
        { VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 0, 0, 0 },
        { VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 0, 0, 0 },
        s_ds_state.minDepthBounds, s_ds_state.maxDepthBounds
    };

    VkPipelineShaderStageCreateInfo shader_stage_infos[2];
    shader_stage_infos[0] = vs_info;
    shader_stage_infos[1] = fs_info;

    VkGraphicsPipelineCreateInfo pi_info = {
        VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, NULL, 0, 2, shader_stage_infos,
        &vf_info, &ia_info, &tess_info, &vp_info, &rs_info, &ms_info, &db_info, &cb_info,
        NULL, s_common_pipeline_layout, s_win_renderpass, 0, VK_NULL_HANDLE, 0
    };

    VkResult r = vkCreateGraphicsPipelines(s_gpu_device, VK_NULL_HANDLE, 1, &pi_info, NO_ALLOC_CALLBACK, &s_display_pipe);

    VKU_DESTROY(vkDestroyShaderModule , vs);
    VKU_DESTROY(vkDestroyShaderModule, fs);

    if (r != VK_SUCCESS) LOG_AND_RETURN0();

    return 1;
}
//=============================================================================
static int Create_Copy_Renderpass(void)
{
    VkAttachmentLoadOp color_ld_op = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    VkAttachmentStoreOp color_st_op = VK_ATTACHMENT_STORE_OP_STORE;
    VkFormat colorFormat = VK_FORMAT_R8G8B8A8_UNORM;
    VkImageLayout layout = VK_IMAGE_LAYOUT_GENERAL;

    VkAttachmentDescription color_attachment_desc;
    color_attachment_desc.flags = 0;
    color_attachment_desc.format = colorFormat;
    color_attachment_desc.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment_desc.loadOp = color_ld_op;
    color_attachment_desc.storeOp = color_st_op;
    color_attachment_desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment_desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment_desc.initialLayout = layout;
    color_attachment_desc.finalLayout = layout;

    VkAttachmentReference color_attachment_refs[6];
    for (int i = 0; i < 6; ++i) {
        color_attachment_refs[i].attachment = i;
        color_attachment_refs[i].layout = layout;
    }
    VkAttachmentReference ds_attachment_ref = { VK_ATTACHMENT_UNUSED, layout };

    VkSubpassDescription subpass_desc;
    subpass_desc.flags = 0;
    subpass_desc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass_desc.inputAttachmentCount = 0;
    subpass_desc.pInputAttachments = NULL;
    subpass_desc.colorAttachmentCount = 6;
    subpass_desc.pColorAttachments = color_attachment_refs;
    subpass_desc.pResolveAttachments = NULL;
    subpass_desc.pDepthStencilAttachment = &ds_attachment_ref;
    subpass_desc.preserveAttachmentCount = 0;
    subpass_desc.pPreserveAttachments = NULL;

    VkAttachmentDescription color_attachment_descs[6];
    for (int i = 0; i < 6; ++i) {
        color_attachment_descs[i] = color_attachment_desc;
    }

    VkRenderPassCreateInfo render_info;
    render_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_info.pNext = NULL;
    render_info.flags = 0;
    render_info.attachmentCount = 6;
    render_info.pAttachments = color_attachment_descs;
    render_info.subpassCount = 1;
    render_info.pSubpasses = &subpass_desc;
    render_info.dependencyCount = 0;
    render_info.pDependencies = NULL;

    VKU_VR(vkCreateRenderPass(s_gpu_device, &render_info, NO_ALLOC_CALLBACK, &s_copy_renderpass));

    return 1;
}
//=============================================================================
static int Create_Copy_Pipeline(void)
{
    VkShaderModule vs, fs;
    vs = VK_NULL_HANDLE;
    fs = VK_NULL_HANDLE;

    VKU_Load_Shader(s_gpu_device, "Data/Shader_GLSL/VS_Quad_UL_INV.bil", &vs);
    VKU_Load_Shader(s_gpu_device, "Data/Shader_GLSL/FS_Copy_Images.bil", &fs);

    if (vs == VK_NULL_HANDLE || fs == VK_NULL_HANDLE)
    {
        VKU_DESTROY(vkDestroyShaderModule, vs);
        VKU_DESTROY(vkDestroyShaderModule, fs);
        LOG_AND_RETURN0();
    }

    VkPipelineShaderStageCreateInfo vs_info = {
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        NULL, 0, VK_SHADER_STAGE_VERTEX_BIT, vs, "main", NULL
    };
    VkPipelineShaderStageCreateInfo fs_info = {
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        NULL, 0, VK_SHADER_STAGE_FRAGMENT_BIT, fs, "main", NULL
    };
    VkPipelineVertexInputStateCreateInfo vf_info = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, NULL, 0,
        0, NULL, 0, NULL
    };
    VkPipelineInputAssemblyStateCreateInfo ia_info = {
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, NULL, 0,
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, VK_FALSE
    };
    VkPipelineTessellationStateCreateInfo tess_info = {
        VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO, NULL, 0, 0
    };
    VkPipelineViewportStateCreateInfo vp_info = {
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, NULL, 0,
        s_vp_state_copy_skybox.viewportCount, &s_vp_state_copy_skybox.viewport,
        s_vp_state_copy_skybox.scissorCount, &s_vp_state_copy_skybox.scissors
    };
    VkPipelineRasterizationStateCreateInfo rs_info = {
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, NULL, 0, VK_FALSE,
        VK_FALSE, VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_FALSE,
        s_rs_state.depthBias, s_rs_state.depthBiasClamp,
        s_rs_state.slopeScaledDepthBias, s_rs_state.lineWidth
    };
    VkPipelineMultisampleStateCreateInfo ms_info = {
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, NULL, 0, 1, VK_FALSE,
        1.0f, NULL, VK_FALSE, VK_FALSE
    };
    VkPipelineColorBlendAttachmentState attachment[6] = {
        {
            VK_FALSE, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD,
            VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD, 0x0f
        }, {
            VK_FALSE, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD,
            VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD, 0x0f
        }, {
            VK_FALSE, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD,
            VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD, 0x0f
        }, {
            VK_FALSE, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD,
            VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD, 0x0f
        }, {
            VK_FALSE, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD,
            VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD, 0x0f
        }, {
            VK_FALSE, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD,
            VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD, 0x0f
        }
    };
    VkPipelineColorBlendStateCreateInfo cb_info = {
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, NULL, 0, VK_FALSE,
        VK_LOGIC_OP_COPY, 6, attachment, s_cb_state.blendConst[0]
    };
    VkPipelineDepthStencilStateCreateInfo db_info = {
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, NULL, 0,
        VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL, VK_FALSE, VK_FALSE,
        { VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 0, 0, 0 },
        { VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 0, 0, 0 },
        s_ds_state.minDepthBounds, s_ds_state.maxDepthBounds
    };

    VkPipelineShaderStageCreateInfo shader_stage_infos[2];
    shader_stage_infos[0] = vs_info;
    shader_stage_infos[1] = fs_info;

    VkDynamicState dynamic_states[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo ds_info;
    ds_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    ds_info.pNext = NULL;
    ds_info.flags = 0;
    ds_info.dynamicStateCount = SDL_arraysize(dynamic_states);
    ds_info.pDynamicStates = dynamic_states;

    VkGraphicsPipelineCreateInfo pi_info = {
        VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, NULL, 0, 2, shader_stage_infos,
        &vf_info, &ia_info, &tess_info, &vp_info, &rs_info, &ms_info, &db_info, &cb_info,
        &ds_info, s_common_pipeline_layout, s_copy_renderpass, 0, VK_NULL_HANDLE, 0
    };

    VkResult r = vkCreateGraphicsPipelines(s_gpu_device, VK_NULL_HANDLE, 1, &pi_info, NO_ALLOC_CALLBACK, &s_copy_image_pipe);

    VKU_DESTROY(vkDestroyShaderModule, vs);
    VKU_DESTROY(vkDestroyShaderModule, fs);

    if (r != VK_SUCCESS) LOG_AND_RETURN0();

    return 1;
}
//=============================================================================
static int Create_Graph_Pipeline(VkPrimitiveTopology topology, VkPipeline *pipe)
{
    VkShaderModule vs, fs;
    vs = VK_NULL_HANDLE;
    fs = VK_NULL_HANDLE;

    VKU_Load_Shader(s_gpu_device, "Data/Shader_GLSL/VS_Graph.bil", &vs);
    VKU_Load_Shader(s_gpu_device, "Data/Shader_GLSL/FS_Graph.bil", &fs);

    if (vs == VK_NULL_HANDLE || fs == VK_NULL_HANDLE)
    {
        VKU_DESTROY(vkDestroyShaderModule, vs);
        VKU_DESTROY(vkDestroyShaderModule, fs);
        LOG_AND_RETURN0();
    }

    VkPipelineShaderStageCreateInfo vs_info = {
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        NULL, 0, VK_SHADER_STAGE_VERTEX_BIT, vs, "main", NULL
    };
    VkPipelineShaderStageCreateInfo fs_info = {
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        NULL, 0, VK_SHADER_STAGE_FRAGMENT_BIT, fs, "main", NULL
    };
    VkPipelineInputAssemblyStateCreateInfo ia_info = {
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, NULL, 0,
        topology, VK_FALSE
    };
    VkPipelineTessellationStateCreateInfo tess_info = {
        VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO, NULL, 0, 0
    };
    VkPipelineViewportStateCreateInfo vp_info = {
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, NULL, 0, s_vp_state.viewportCount,
        &s_vp_state.viewport, s_vp_state.scissorCount, &s_vp_state.scissors
    };
    VkPipelineRasterizationStateCreateInfo rs_info = {
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, NULL, 0, VK_FALSE,
        VK_FALSE, VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_FALSE,
        s_rs_state.depthBias, s_rs_state.depthBiasClamp,
        s_rs_state.slopeScaledDepthBias, s_rs_state.lineWidth
    };
    VkPipelineMultisampleStateCreateInfo ms_info = {
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, NULL, 0, 1, VK_FALSE,
        1.0f, NULL, VK_FALSE, VK_FALSE
    };
    VkPipelineColorBlendAttachmentState attachment = {
        VK_TRUE, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_OP_ADD,
        VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_OP_ADD, 0x0f
    };
    VkPipelineColorBlendStateCreateInfo cb_info = {
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, NULL, 0, VK_FALSE,
        VK_LOGIC_OP_COPY, 1, &attachment, s_cb_state.blendConst[0]
    };
    VkPipelineDepthStencilStateCreateInfo db_info = {
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, NULL, 0,
        VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL, VK_FALSE, VK_FALSE,
        { VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 0, 0, 0 },
        { VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 0, 0, 0 },
        s_ds_state.minDepthBounds, s_ds_state.maxDepthBounds
    };

    VkVertexInputBindingDescription vf_binding_desc[] = {
        { 0, sizeof(GRAPH_SHADER_IN), VK_VERTEX_INPUT_RATE_VERTEX },
        { 1, sizeof(GRAPH_SHADER_IN), VK_VERTEX_INPUT_RATE_VERTEX }
    };
    VkVertexInputAttributeDescription vf_attribute_desc[] = {
        {
            0, 0, VK_FORMAT_R32G32_SFLOAT, 0
        },
        {
            1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(float) * 2
        }
    };
    VkPipelineVertexInputStateCreateInfo vf_info = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, NULL, 0,
        SDL_arraysize(vf_binding_desc), vf_binding_desc, SDL_arraysize(vf_attribute_desc), vf_attribute_desc
    };

    VkPipelineShaderStageCreateInfo shader_stage_infos[2];
    shader_stage_infos[0] = vs_info;
    shader_stage_infos[1] = fs_info;

    VkDynamicState dynamic_states[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo ds_info;
    ds_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    ds_info.pNext = NULL;
    ds_info.flags = 0;
    ds_info.dynamicStateCount = SDL_arraysize(dynamic_states);
    ds_info.pDynamicStates = dynamic_states;
    
    VkGraphicsPipelineCreateInfo pi_info = {
        VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, NULL, 0, 2, shader_stage_infos,
        &vf_info, &ia_info, &tess_info, &vp_info, &rs_info, &ms_info, &db_info, &cb_info,
        &ds_info, s_common_pipeline_layout, s_win_renderpass, 0, VK_NULL_HANDLE, 0
    };

    VkResult r = vkCreateGraphicsPipelines(s_gpu_device, VK_NULL_HANDLE, 1, &pi_info, NO_ALLOC_CALLBACK, pipe);

    VKU_DESTROY(vkDestroyShaderModule, vs);
    VKU_DESTROY(vkDestroyShaderModule, fs);

    if (r != VK_SUCCESS) LOG_AND_RETURN0();

    return 1;
}
//=============================================================================
static int Init_Dynamic_States(void)
{
    VkViewport vp = { 0.0f, 0.0f, (float)s_glob_state->width, (float)s_glob_state->height, 0.0f, 1.0f };
    VkRect2D scissor = { { 0, 0 }, { s_glob_state->width, s_glob_state->height } };

    s_vp_state.viewportCount = 1;
    s_vp_state.viewport = vp;
    s_vp_state.scissorCount = 1;
    s_vp_state.scissors = scissor;

    {
        VkViewport vp = { 0.0f, 0.0f, 1024.0f, 1024.0f, 0.0f, 1.0f };
        VkRect2D scissor = { { 0, 0 }, { 1024, 1024 } };
        s_vp_state_copy_skybox.viewportCount = 1;
        s_vp_state_copy_skybox.viewport = vp;
        s_vp_state_copy_skybox.scissorCount = 1;
        s_vp_state_copy_skybox.scissors = scissor;
    }
    {
        VkViewport vp = { 0.0f, 0.0f, 256.0f, 1.0f, 0.0f, 1.0f };
        VkRect2D scissor = { { 0, 0 }, { 256, 1 } };
        s_vp_state_copy_palette.viewportCount = 1;
        s_vp_state_copy_palette.viewport = vp;
        s_vp_state_copy_palette.scissorCount = 1;
        s_vp_state_copy_palette.scissors = scissor;
    }
    {
        VkViewport vp = { 16.0f, 180.0f, 40.0f, 20.0f, 0.0f, 1.0f };
        VkRect2D scissor = { { 0, 0 }, { s_glob_state->width, s_glob_state->height } };
        s_vp_state_legend_cpu.viewportCount = 1;
        s_vp_state_legend_cpu.viewport = vp;
        s_vp_state_legend_cpu.scissorCount = 1;
        s_vp_state_legend_cpu.scissors = scissor;
    }
    {
        VkViewport vp = { 16.0f, 210.0f, 40.0f, 20.0f, 0.0f, 1.0f };
        VkRect2D scissor = { { 0, 0 }, { s_glob_state->width, s_glob_state->height } };
        s_vp_state_legend_gpu.viewportCount = 1;
        s_vp_state_legend_gpu.viewport = vp;
        s_vp_state_legend_gpu.scissorCount = 1;
        s_vp_state_legend_gpu.scissors = scissor;
    }

    s_rs_state.depthBias = 0.0f;
    s_rs_state.depthBiasClamp = 0.0f;
    s_rs_state.slopeScaledDepthBias = 0.0f;
    s_rs_state.lineWidth = 1.0f;

    s_cb_state.blendConst[0] = 0.0f;
    s_cb_state.blendConst[1] = 0.0f;
    s_cb_state.blendConst[2] = 0.0f;
    s_cb_state.blendConst[3] = 0.0f;

    s_ds_state.minDepthBounds = 0.0f;
    s_ds_state.maxDepthBounds = 1.0f;

    return 1;
}
//=============================================================================
static int Create_Window_Framebuffer(void)
{
    for (int i = 0; i < k_Window_Buffering; ++i) {
        VkFormat colorFormat = VK_FORMAT_R8G8B8A8_UNORM;

        VkComponentMapping channels = {
            VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A
        };
        VkImageSubresourceRange subres_range = {
            VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1
        };
        VkImageViewCreateInfo view_info = {
            VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, NULL, 0, s_win_images[i],
            VK_IMAGE_VIEW_TYPE_2D, colorFormat, channels, subres_range
        };
        VKU_VR(vkCreateImageView(s_gpu_device, &view_info, NO_ALLOC_CALLBACK, &s_win_image_view[i]));

        VkImageView view_infos[2];
        view_infos[0] = s_win_image_view[i];
        view_infos[1] = s_depth_stencil_view;

        VkFramebufferCreateInfo fb_info = {
            VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, NULL, 0, s_win_renderpass,
            2, view_infos, s_glob_state->width, s_glob_state->height, 1
        };
        VKU_VR(vkCreateFramebuffer(s_gpu_device, &fb_info, NO_ALLOC_CALLBACK, &s_win_framebuffer[i]));
    }

    return 1;
}
//=============================================================================
static int Create_Float_Image_And_Framebuffer(void)
{
    VkImageCreateInfo image_info = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, NULL, 0, VK_IMAGE_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM,
        { s_glob_state->width, s_glob_state->height, 1 }, 1, 1, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        VK_SHARING_MODE_EXCLUSIVE, 0, NULL, VK_IMAGE_LAYOUT_UNDEFINED
    };
    VKU_VR(vkCreateImage(s_gpu_device, &image_info, NO_ALLOC_CALLBACK, &s_float_image));
    if (!VKU_Alloc_Image_Object(s_image_mempool_target, s_float_image, NULL, Get_Mem_Type_Index(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))) LOG_AND_RETURN0();

    VkImageViewCreateInfo image_view = {
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, NULL, 0, s_float_image, VK_IMAGE_VIEW_TYPE_2D,
        VK_FORMAT_R8G8B8A8_UNORM,
        {
            VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B,
            VK_COMPONENT_SWIZZLE_A
        },
        { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };
    VKU_VR(vkCreateImageView(s_gpu_device, &image_view, NO_ALLOC_CALLBACK, &s_float_image_view));

    VkImageView view_infos[2];
    view_infos[0] = s_float_image_view;
    view_infos[1] = s_depth_stencil_view;

    VkFramebufferCreateInfo fb_info = {
        VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, NULL, 0, s_float_renderpass,
        2, view_infos, s_glob_state->width, s_glob_state->height, 1
    };
    VKU_VR(vkCreateFramebuffer(s_gpu_device, &fb_info, NO_ALLOC_CALLBACK, &s_float_framebuffer));

    return 1;
}
//=============================================================================
static int Create_Skybox_Image(void)
{
    VkImageCreateInfo image_info = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, NULL, VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
        VK_IMAGE_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM, { 1024, 1024, 1 }, 1, 6, VK_SAMPLE_COUNT_1_BIT,
        VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT |
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_SHARING_MODE_EXCLUSIVE, 0,
        NULL, VK_IMAGE_LAYOUT_UNDEFINED
    };
    VKU_VR(vkCreateImage(s_gpu_device, &image_info, NO_ALLOC_CALLBACK, &s_skybox_image));
    if (!VKU_Alloc_Image_Object(s_image_mempool_texture, s_skybox_image, NULL, Get_Mem_Type_Index(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))) LOG_AND_RETURN0();

    VkImageViewCreateInfo image_view_info = {
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, NULL, 0,
        s_skybox_image, VK_IMAGE_VIEW_TYPE_CUBE, VK_FORMAT_R8G8B8A8_UNORM,
        { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A },
        { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6 }
    };
    VKU_VR(vkCreateImageView(s_gpu_device, &image_view_info, NO_ALLOC_CALLBACK, &s_skybox_image_view));

    return 1;
}
//=============================================================================
static int Create_Palette_Images(void)
{
    for (int i = 0; i < 6; ++i) {
        VkImageCreateInfo image_info = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, NULL, 0,
            VK_IMAGE_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM, { 256, 1, 1 }, 1, 1, VK_SAMPLE_COUNT_1_BIT,
            VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT |
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_SHARING_MODE_EXCLUSIVE, 0,
            NULL, VK_IMAGE_LAYOUT_UNDEFINED
        };
        VKU_VR(vkCreateImage(s_gpu_device, &image_info, NO_ALLOC_CALLBACK, &s_palette_image[i]));
        if (!VKU_Alloc_Image_Object(s_image_mempool_texture, s_palette_image[i], NULL, Get_Mem_Type_Index(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))) return 0;

        VkImageViewCreateInfo image_view_info = {
            VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, NULL, 0,
            s_palette_image[i], VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM,
            {
                VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B,
                VK_COMPONENT_SWIZZLE_A
            },
            { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
        };
        VKU_VR(vkCreateImageView(s_gpu_device, &image_view_info, NO_ALLOC_CALLBACK, &s_palette_image_view[i]));
    }

    return 1;
}
//=============================================================================
static int Render_To_Skybox_Image(void)
{
    VkImageView image_view[6];
    VkFramebuffer framebuffer;

    for (int i = 0; i < 6; ++i) {
        VkComponentMapping channels = {
            VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A
        };
        VkImageSubresourceRange subres_range = {
            VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, i, 1
        };
        VkImageViewCreateInfo image_info = {
            VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, NULL, 0,
            s_skybox_image, VK_IMAGE_VIEW_TYPE_2D,
            VK_FORMAT_R8G8B8A8_UNORM, channels, subres_range
        };
        VKU_VR(vkCreateImageView(s_gpu_device, &image_info, NO_ALLOC_CALLBACK, &image_view[i]));
    }

    VkFramebufferCreateInfo fb_info = {
        VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, NULL, 0,
        s_copy_renderpass, 6, image_view, 1024, 1024, 1
    };
    VKU_VR(vkCreateFramebuffer(s_gpu_device, &fb_info, NO_ALLOC_CALLBACK, &framebuffer));

	struct {
		VkBuffer buffer;
		VkDeviceMemory memory;
	} staging_res;

	VkBufferCreateInfo buffer_info = {
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, NULL, 0,
		1024 * 1024 * 4 * 6,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_SHARING_MODE_EXCLUSIVE, 0, NULL
	};
	VKU_VR(vkCreateBuffer(s_gpu_device, &buffer_info, NO_ALLOC_CALLBACK, &staging_res.buffer));

	VkMemoryRequirements mreq_buffer = { 0 };
	vkGetBufferMemoryRequirements(s_gpu_device, staging_res.buffer, &mreq_buffer);

	VkMemoryAllocateInfo alloc_info_buffer = {
		VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, mreq_buffer.size,
		Get_Mem_Type_Index(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
	};
	VKU_VR(vkAllocateMemory(s_gpu_device, &alloc_info_buffer, NO_ALLOC_CALLBACK, &staging_res.memory));
	VKU_VR(vkBindBufferMemory(s_gpu_device, staging_res.buffer, staging_res.memory, 0));

    uint8_t *ptr;
    VKU_VR(vkMapMemory(s_gpu_device, staging_res.memory, 0, VK_WHOLE_SIZE, 0, (void **)&ptr));

    const char *name[6] = {
        "Data/Texture/Skybox_right1.png", "Data/Texture/Skybox_left2.png",
        "Data/Texture/Skybox_top3.png", "Data/Texture/Skybox_bottom4.png",
        "Data/Texture/Skybox_front5.png", "Data/Texture/Skybox_back6.png"
    };

	VkDeviceSize offset = 0;
	VkBufferImageCopy buffer_copy_regions[6] = { 0 };
	for (int i = 0; i < 6; ++i) {
		int w, h, comp;
		stbi_uc *data = Load_Image(name[i], &w, &h, &comp, 4);
		if (!data) LOG_AND_RETURN0();

		size_t size = w * h * comp;
		memcpy(ptr + offset, data, size);

		buffer_copy_regions[i].bufferOffset = offset;
		buffer_copy_regions[i].imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		buffer_copy_regions[i].imageSubresource.baseArrayLayer = i;
		buffer_copy_regions[i].imageSubresource.layerCount = 1;
		buffer_copy_regions[i].imageExtent.width = w;
		buffer_copy_regions[i].imageExtent.height = h;
		buffer_copy_regions[i].imageExtent.depth = 1;

		offset += size;
		stbi_image_free(data);
	}
	vkUnmapMemory(s_gpu_device, staging_res.memory);

	VkImage optimal_image;
	VkImageCreateInfo image_info = {
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, NULL, 0,
		VK_IMAGE_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM,{ 1024, 1024, 1 }, 1, 6, VK_SAMPLE_COUNT_1_BIT,
		VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_SHARING_MODE_EXCLUSIVE, 0,
		NULL, VK_IMAGE_LAYOUT_UNDEFINED
	};

	VKU_VR(vkCreateImage(s_gpu_device, &image_info, NO_ALLOC_CALLBACK, &optimal_image));

	VkMemoryRequirements mreq_image = { 0 };
	vkGetImageMemoryRequirements(s_gpu_device, optimal_image, &mreq_image);

	VkDeviceMemory optimal_image_mem;
	VkMemoryAllocateInfo alloc_info_image = {
		VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, mreq_image.size,
		Get_Mem_Type_Index(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
	};
	VKU_VR(vkAllocateMemory(s_gpu_device, &alloc_info_image, NO_ALLOC_CALLBACK, &optimal_image_mem));
	VKU_VR(vkBindImageMemory(s_gpu_device, optimal_image, optimal_image_mem, 0));

	VkImageView optimal_image_view;
	VkImageViewCreateInfo image_view_info = {
		VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, NULL, 0,
		optimal_image, VK_IMAGE_VIEW_TYPE_2D_ARRAY, VK_FORMAT_R8G8B8A8_UNORM,
		{ VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A },
		{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6 }
	};
	VKU_VR(vkCreateImageView(s_gpu_device, &image_view_info, NO_ALLOC_CALLBACK, &optimal_image_view));
    Update_Common_Dset();
    VkDescriptorImageInfo image_sampler_attach = {
        s_sampler, optimal_image_view, VK_IMAGE_LAYOUT_GENERAL
    };

    VkWriteDescriptorSet update_sampler_font_image = {
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, s_common_dset[s_res_idx],
        1, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &image_sampler_attach,
        VK_NULL_HANDLE, VK_NULL_HANDLE
    };

    vkUpdateDescriptorSets(s_gpu_device, 1, &update_sampler_font_image, 0, NULL);

    VkCommandBufferBeginInfo begin_info = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL, 0, NULL
    };

	VkSubmitInfo submit_info;
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.pNext = NULL;
	submit_info.waitSemaphoreCount = 0;
	submit_info.pWaitSemaphores = NULL;
	submit_info.pWaitDstStageMask = NULL;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = s_cmdbuf_display;
	submit_info.signalSemaphoreCount = 0;
	submit_info.pSignalSemaphores = NULL;

	VKU_VR(vkBeginCommandBuffer(s_cmdbuf_display[0], &begin_info));
	vkCmdCopyBufferToImage(s_cmdbuf_display[0], staging_res.buffer, optimal_image, VK_IMAGE_LAYOUT_GENERAL, 6, buffer_copy_regions);
	VKU_VR(vkEndCommandBuffer(s_cmdbuf_display[0]));

	if (vkQueueSubmit(s_gpu_queue, 1, &submit_info, VK_NULL_HANDLE) != VK_SUCCESS) {
		LOG_AND_RETURN0();
	}
	if (vkQueueWaitIdle(s_gpu_queue) != VK_SUCCESS) {
		LOG_AND_RETURN0();
	}
	
	vkFreeMemory(s_gpu_device, staging_res.memory, NULL);
	vkDestroyBuffer(s_gpu_device, staging_res.buffer, NULL);
	
    VkImageSubresourceRange image_subresource_range;
    image_subresource_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_subresource_range.baseMipLevel = 0;
    image_subresource_range.levelCount = 1;
    image_subresource_range.baseArrayLayer = 0;
    image_subresource_range.layerCount = 6;

    VkImageMemoryBarrier linear_image_memory_barrier;
    linear_image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    linear_image_memory_barrier.pNext = NULL;
    linear_image_memory_barrier.srcAccessMask = 0;
    linear_image_memory_barrier.dstAccessMask = 0;
    linear_image_memory_barrier.oldLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
    linear_image_memory_barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    linear_image_memory_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    linear_image_memory_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    linear_image_memory_barrier.image = optimal_image;
    linear_image_memory_barrier.subresourceRange = image_subresource_range;

    VKU_VR(vkBeginCommandBuffer(s_cmdbuf_display[0], &begin_info));

    VkImageSubresourceRange skybox_subresource_range;
    skybox_subresource_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    skybox_subresource_range.baseMipLevel = 0;
    skybox_subresource_range.levelCount = 1;
    skybox_subresource_range.baseArrayLayer = 0;
    skybox_subresource_range.layerCount = 6;

    VkImageMemoryBarrier skybox_image_memory_barrier;
    skybox_image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    skybox_image_memory_barrier.pNext = NULL;
    skybox_image_memory_barrier.srcAccessMask = 0;
    skybox_image_memory_barrier.dstAccessMask = 0;
    skybox_image_memory_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    skybox_image_memory_barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    skybox_image_memory_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    skybox_image_memory_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    skybox_image_memory_barrier.image = s_skybox_image;
    skybox_image_memory_barrier.subresourceRange = skybox_subresource_range;

    vkCmdPipelineBarrier(s_cmdbuf_display[0], 0, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, NULL, 0, NULL, 1, &skybox_image_memory_barrier);
    vkCmdPipelineBarrier(s_cmdbuf_display[0], 0, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, NULL, 0, NULL, 1, &linear_image_memory_barrier);

    VkRect2D render_area = { { 0, 0 }, { 1024, 1024 } };
    VkClearValue clear_color = { 0 };
    VkRenderPassBeginInfo rpBegin;
    rpBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpBegin.pNext = NULL;
    rpBegin.renderPass = s_copy_renderpass;
    rpBegin.framebuffer = framebuffer;
    rpBegin.renderArea = render_area;
    rpBegin.clearValueCount = 6;
    rpBegin.pClearValues = &clear_color;
    vkCmdBeginRenderPass(s_cmdbuf_display[0], &rpBegin, VK_SUBPASS_CONTENTS_INLINE);
    
    vkCmdBindPipeline(s_cmdbuf_display[0], VK_PIPELINE_BIND_POINT_GRAPHICS, s_copy_image_pipe);
    vkCmdSetViewport(s_cmdbuf_display[0], 0, s_vp_state_copy_skybox.viewportCount, &s_vp_state_copy_skybox.viewport);
    vkCmdSetScissor(s_cmdbuf_display[0], 0, s_vp_state_copy_skybox.scissorCount, &s_vp_state_copy_skybox.scissors);
    vkCmdBindDescriptorSets(s_cmdbuf_display[0], VK_PIPELINE_BIND_POINT_GRAPHICS, s_common_pipeline_layout, 0, 1, &s_common_dset[0], 0, NULL);

    vkCmdDraw(s_cmdbuf_display[0], 4, 1, 0, 0);

    vkCmdEndRenderPass(s_cmdbuf_display[0]);
    VKU_VR(vkEndCommandBuffer(s_cmdbuf_display[0]));

    if (vkQueueSubmit(s_gpu_queue, 1, &submit_info, VK_NULL_HANDLE) != VK_SUCCESS) {
        LOG_AND_RETURN0();
    }
    if (vkQueueWaitIdle(s_gpu_queue) != VK_SUCCESS) {
        LOG_AND_RETURN0();
    }

    VKU_DESTROY(vkDestroyImageView, optimal_image_view);
    VKU_DESTROY(vkDestroyImage, optimal_image);
    VKU_FREE_MEM(optimal_image_mem);
    for (int i = 0; i < 6; ++i) VKU_DESTROY(vkDestroyImageView, image_view[i]);
    VKU_DESTROY(vkDestroyFramebuffer, framebuffer);

    return 1;
}
//=============================================================================
static int Render_To_Palette_Images(void)
{
    VkImageView image_view[6];
    VkFramebuffer framebuffer;

    for (int i = 0; i < 6; ++i) {
        VkComponentMapping channels = {
            VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A
        };
        VkImageSubresourceRange subres_range = {
            VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1
        };
        VkImageViewCreateInfo image_info = {
            VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, NULL, 0,
            s_palette_image[i], VK_IMAGE_VIEW_TYPE_2D,
            VK_FORMAT_R8G8B8A8_UNORM, channels, subres_range
        };
        VKU_VR(vkCreateImageView(s_gpu_device, &image_info, NO_ALLOC_CALLBACK, &image_view[i]));
    }

    VkFramebufferCreateInfo fb_info = {
        VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, NULL, 0,
        s_copy_renderpass, 6, image_view, 256, 1, 1
    };

    VKU_VR(vkCreateFramebuffer(s_gpu_device, &fb_info, NO_ALLOC_CALLBACK, &framebuffer));

	struct {
		VkBuffer buffer;
		VkDeviceMemory memory;
	} staging_res;

	VkBufferCreateInfo buffer_info = {
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, NULL, 0,
		256 * 4 * 6,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_SHARING_MODE_EXCLUSIVE, 0, NULL
	};
	VKU_VR(vkCreateBuffer(s_gpu_device, &buffer_info, NO_ALLOC_CALLBACK, &staging_res.buffer));

	VkMemoryRequirements mreq_buffer = { 0 };
	vkGetBufferMemoryRequirements(s_gpu_device, staging_res.buffer, &mreq_buffer);

	VkMemoryAllocateInfo alloc_info_buffer = {
		VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, mreq_buffer.size,
		Get_Mem_Type_Index(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
	};
	VKU_VR(vkAllocateMemory(s_gpu_device, &alloc_info_buffer, NO_ALLOC_CALLBACK, &staging_res.memory));
	VKU_VR(vkBindBufferMemory(s_gpu_device, staging_res.buffer, staging_res.memory, 0));

	uint8_t *ptr;
	VKU_VR(vkMapMemory(s_gpu_device, staging_res.memory, 0, VK_WHOLE_SIZE, 0, (void **)&ptr));

	const char *name[6] = {
		"Data/Texture/Palette_Fire.png", "Data/Texture/Palette_Purple.png",
		"Data/Texture/Palette_Muted.png", "Data/Texture/Palette_Rainbow.png",
		"Data/Texture/Palette_Sky.png", "Data/Texture/Palette_Sky.png"
	};

	VkDeviceSize offset = 0;
	VkBufferImageCopy buffer_copy_regions[6] = { 0 };
	for (int i = 0; i < 6; ++i) {
		int w, h, comp;
		stbi_uc *data = Load_Image(name[i], &w, &h, &comp, 4);
		if (!data) LOG_AND_RETURN0();

		size_t size = w * h * comp;
		memcpy(ptr + offset, data, size);

		buffer_copy_regions[i].bufferOffset = offset;
		buffer_copy_regions[i].imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		buffer_copy_regions[i].imageSubresource.baseArrayLayer = i;
		buffer_copy_regions[i].imageSubresource.layerCount = 1;
		buffer_copy_regions[i].imageExtent.width = w;
		buffer_copy_regions[i].imageExtent.height = h;
		buffer_copy_regions[i].imageExtent.depth = 1;

		offset += size;
		stbi_image_free(data);
	}
	vkUnmapMemory(s_gpu_device, staging_res.memory);

	VkImage optimal_image;
	VkImageCreateInfo image_info = {
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, NULL, 0,
		VK_IMAGE_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM,{ 256, 1, 1 }, 1, 6, VK_SAMPLE_COUNT_1_BIT,
		VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_SHARING_MODE_EXCLUSIVE, 0,
		NULL, VK_IMAGE_LAYOUT_UNDEFINED
	};

	VKU_VR(vkCreateImage(s_gpu_device, &image_info, NO_ALLOC_CALLBACK, &optimal_image));

	VkMemoryRequirements mreq_image = { 0 };
	vkGetImageMemoryRequirements(s_gpu_device, optimal_image, &mreq_image);

	VkDeviceMemory optimal_image_mem;
	VkMemoryAllocateInfo alloc_info_image = {
		VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, mreq_image.size,
		Get_Mem_Type_Index(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
	};
	VKU_VR(vkAllocateMemory(s_gpu_device, &alloc_info_image, NO_ALLOC_CALLBACK, &optimal_image_mem));
	VKU_VR(vkBindImageMemory(s_gpu_device, optimal_image, optimal_image_mem, 0));

	VkImageView optimal_image_view;
	VkImageViewCreateInfo image_view_info = {
		VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, NULL, 0,
		optimal_image, VK_IMAGE_VIEW_TYPE_2D_ARRAY, VK_FORMAT_R8G8B8A8_UNORM,
		{ VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A },
		{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6 }
	};
	VKU_VR(vkCreateImageView(s_gpu_device, &image_view_info, NO_ALLOC_CALLBACK, &optimal_image_view));
	Update_Common_Dset();
	VkDescriptorImageInfo image_sampler_attach = {
		s_sampler, optimal_image_view, VK_IMAGE_LAYOUT_GENERAL
	};

	VkWriteDescriptorSet update_sampler_font_image = {
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, s_common_dset[s_res_idx],
		1, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &image_sampler_attach,
		VK_NULL_HANDLE, VK_NULL_HANDLE
	};

	vkUpdateDescriptorSets(s_gpu_device, 1, &update_sampler_font_image, 0, NULL);

	VkCommandBufferBeginInfo begin_info = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL, 0, NULL
	};

	VkSubmitInfo submit_info;
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.pNext = NULL;
	submit_info.waitSemaphoreCount = 0;
	submit_info.pWaitSemaphores = NULL;
	submit_info.pWaitDstStageMask = NULL;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = s_cmdbuf_display;
	submit_info.signalSemaphoreCount = 0;
	submit_info.pSignalSemaphores = NULL;

	VKU_VR(vkBeginCommandBuffer(s_cmdbuf_display[0], &begin_info));
	vkCmdCopyBufferToImage(s_cmdbuf_display[0], staging_res.buffer, optimal_image, VK_IMAGE_LAYOUT_GENERAL, 6, buffer_copy_regions);
	VKU_VR(vkEndCommandBuffer(s_cmdbuf_display[0]));

	if (vkQueueSubmit(s_gpu_queue, 1, &submit_info, VK_NULL_HANDLE) != VK_SUCCESS) {
		LOG_AND_RETURN0();
	}
	if (vkQueueWaitIdle(s_gpu_queue) != VK_SUCCESS) {
		LOG_AND_RETURN0();
	}

	vkFreeMemory(s_gpu_device, staging_res.memory, NULL);
	vkDestroyBuffer(s_gpu_device, staging_res.buffer, NULL);

    VkImageSubresourceRange image_subresource_range;
    image_subresource_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_subresource_range.baseMipLevel = 0;
    image_subresource_range.levelCount = 1;
    image_subresource_range.baseArrayLayer = 0;
    image_subresource_range.layerCount = 6;

    VkImageMemoryBarrier linear_image_memory_barrier;
    linear_image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    linear_image_memory_barrier.pNext = NULL;
    linear_image_memory_barrier.srcAccessMask = 0;
    linear_image_memory_barrier.dstAccessMask = 0;
    linear_image_memory_barrier.oldLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
    linear_image_memory_barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    linear_image_memory_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    linear_image_memory_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    linear_image_memory_barrier.image = optimal_image;
    linear_image_memory_barrier.subresourceRange = image_subresource_range;

    VkImageSubresourceRange palette_subresource_range[6];
    VkImageMemoryBarrier palette_image_memory_barrier[6];

    for (int i = 0; i < 6; ++i) {
        palette_subresource_range[i].aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        palette_subresource_range[i].baseMipLevel = 0;
        palette_subresource_range[i].levelCount = 1;
        palette_subresource_range[i].baseArrayLayer = 0;
        palette_subresource_range[i].layerCount = 1;

        palette_image_memory_barrier[i].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        palette_image_memory_barrier[i].pNext = NULL;
        palette_image_memory_barrier[i].srcAccessMask = 0;
        palette_image_memory_barrier[i].dstAccessMask = 0;
        palette_image_memory_barrier[i].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        palette_image_memory_barrier[i].newLayout = VK_IMAGE_LAYOUT_GENERAL;
        palette_image_memory_barrier[i].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        palette_image_memory_barrier[i].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        palette_image_memory_barrier[i].image = s_palette_image[i];
        palette_image_memory_barrier[i].subresourceRange = palette_subresource_range[i];
    }

    VKU_VR(vkBeginCommandBuffer(s_cmdbuf_display[0], &begin_info));

    vkCmdPipelineBarrier(s_cmdbuf_display[0], 0, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, NULL, 0, NULL,
                         SDL_arraysize(palette_image_memory_barrier), palette_image_memory_barrier);
    vkCmdPipelineBarrier(s_cmdbuf_display[0], 0, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, NULL, 0, NULL,
                         1, &linear_image_memory_barrier);

    VkRect2D render_area = { { 0, 0 }, { 256, 1 } };
    VkClearValue clear_color = { 0 };
    VkRenderPassBeginInfo rpBegin;
    rpBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpBegin.pNext = NULL;
    rpBegin.renderPass = s_copy_renderpass;
    rpBegin.framebuffer = framebuffer;
    rpBegin.renderArea = render_area;
    rpBegin.clearValueCount = 6;
    rpBegin.pClearValues = &clear_color;

    vkCmdBeginRenderPass(s_cmdbuf_display[0], &rpBegin, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(s_cmdbuf_display[0], VK_PIPELINE_BIND_POINT_GRAPHICS, s_copy_image_pipe);
    vkCmdSetViewport(s_cmdbuf_display[0], 0, s_vp_state_copy_palette.viewportCount, &s_vp_state_copy_palette.viewport);
    vkCmdSetScissor(s_cmdbuf_display[0], 0, s_vp_state_copy_palette.scissorCount, &s_vp_state_copy_palette.scissors);
    vkCmdBindDescriptorSets(s_cmdbuf_display[0], VK_PIPELINE_BIND_POINT_GRAPHICS, s_common_pipeline_layout, 0, 1, &s_common_dset[0], 0, NULL);

    vkCmdDraw(s_cmdbuf_display[0], 4, 1, 0, 0);

    vkCmdEndRenderPass(s_cmdbuf_display[0]);
    VKU_VR(vkEndCommandBuffer(s_cmdbuf_display[0]));

    if (vkQueueSubmit(s_gpu_queue, 1, &submit_info, VK_NULL_HANDLE) != VK_SUCCESS) {
        LOG_AND_RETURN0();
    }
    if (vkQueueWaitIdle(s_gpu_queue) != VK_SUCCESS) {
        LOG_AND_RETURN0();
    }

    VKU_DESTROY(vkDestroyImageView, optimal_image_view);
    VKU_DESTROY(vkDestroyImage, optimal_image);
    VKU_FREE_MEM(optimal_image_mem);
    for (int i = 0; i < 6; ++i) VKU_DESTROY(vkDestroyImageView, image_view[i]);
    VKU_DESTROY(vkDestroyFramebuffer, framebuffer);

    return 1;
}
//=============================================================================
static int Init_Particle_Thread(THREAD_DATA *thrd)
{
    VkCommandPoolCreateInfo command_pool_info;
    command_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    command_pool_info.pNext = NULL;
    command_pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    command_pool_info.queueFamilyIndex = s_queue_family_index;
    VKU_VR(vkCreateCommandPool(s_gpu_device, &command_pool_info, NO_ALLOC_CALLBACK, &thrd->cmdpool));

    VkCommandBufferAllocateInfo cmdbuf_info = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, NULL, thrd->cmdpool,
        VK_COMMAND_BUFFER_LEVEL_PRIMARY, k_Resource_Buffering
    };
    VKU_VR(vkAllocateCommandBuffers(s_gpu_device, &cmdbuf_info, thrd->cmdbuf));

    thrd->sem = SDL_CreateSemaphore(0);
    if (!thrd->sem) LOG_AND_RETURN0();

    return 1;
}
//=============================================================================
static int Finish_Particle_Thread(THREAD_DATA *thrd)
{
    if (s_exit_code == STARDUST_ERROR) {
        SDL_SemPost(s_cmdgen_sem[thrd->tid]);
    }
    SDL_WaitThread(thrd->thread, NULL);

    return 1;
}
//=============================================================================
static int Release_Particle_Thread(THREAD_DATA *thrd)
{
    SDL_DestroySemaphore(thrd->sem);
    thrd->sem = NULL;

    VKU_FREE_CMD_BUF(thrd->cmdpool, k_Resource_Buffering, thrd->cmdbuf);
    VKU_DESTROY(vkDestroyCommandPool, thrd->cmdpool);

    return 1;
}
//=============================================================================
static int Update_Particle_Thread(THREAD_DATA *thrd)
{
    VkCommandBufferBeginInfo begin_info = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL, 0, NULL
    };

    VKU_VR(vkBeginCommandBuffer(thrd->cmdbuf[s_res_idx], &begin_info));

    VkRect2D render_area = { { 0, 0 }, { s_glob_state->width, s_glob_state->height } };
    VkClearValue clear_color = { 0 };
    VkRenderPassBeginInfo rpBegin;
    rpBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpBegin.pNext = NULL;
    rpBegin.renderPass = s_float_renderpass;
    rpBegin.framebuffer = s_float_framebuffer;
    rpBegin.renderArea = render_area;
    rpBegin.clearValueCount = 2;
    rpBegin.pClearValues = &clear_color;
    vkCmdBeginRenderPass(thrd->cmdbuf[s_res_idx], &rpBegin, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(thrd->cmdbuf[s_res_idx], VK_PIPELINE_BIND_POINT_GRAPHICS, s_particle_pipe);
    vkCmdBindDescriptorSets(thrd->cmdbuf[s_res_idx], VK_PIPELINE_BIND_POINT_GRAPHICS, s_common_pipeline_layout,
                            0, 1, &s_common_dset[s_res_idx], 0, NULL);

    VkDeviceSize offsets = 0;
    vkCmdBindVertexBuffers(thrd->cmdbuf[s_res_idx], 0, 1, &s_particle_seed_buf, &offsets);
    for (int i = 0; i < DRAW_PER_THREAD; ++i) {
        uint32_t firstVertex = (i + thrd->tid * DRAW_PER_THREAD) * s_glob_state->batch_size;
        vkCmdDraw(thrd->cmdbuf[s_res_idx], s_glob_state->batch_size, 1, firstVertex, 0);
    }

    vkCmdEndRenderPass(thrd->cmdbuf[s_res_idx]);
    VKU_VR(vkEndCommandBuffer(thrd->cmdbuf[s_res_idx]));

    return 1;
}
//=============================================================================
static int SDLCALL Particle_Thread(void *data)
{
    THREAD_DATA *thrd = data;

#if defined(_WIN32)
    SetThreadAffinityMask(GetCurrentThread(), ((DWORD_PTR)1) << thrd->tid);
#endif

    do {
        SDL_SemWait(s_cmdgen_sem[thrd->tid]);
        Update_Particle_Thread(thrd);
        SDL_SemPost(thrd->sem);
    } while (s_exit_code == STARDUST_CONTINUE);

    return 1;
}
//=============================================================================
static void Cmd_Clear(VkCommandBuffer cmdbuf)
{
    VkImageSubresourceRange color_range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    VkImageSubresourceRange depth_stencil_range[2] = {
        { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 },
        { VK_IMAGE_ASPECT_STENCIL_BIT, 0, 1, 0, 1 },
    };

    if (!s_glob_state->frame) {
        for (int i = 0; i < k_Resource_Buffering; ++i) {
            VkImageSubresourceRange color_subresource_range;
            color_subresource_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            color_subresource_range.baseMipLevel = 0;
            color_subresource_range.levelCount = 1;
            color_subresource_range.baseArrayLayer = 0;
            color_subresource_range.layerCount = 1;

            VkImageMemoryBarrier color_image_memory_barrier;
            color_image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            color_image_memory_barrier.pNext = NULL;
            color_image_memory_barrier.srcAccessMask = 0;
            color_image_memory_barrier.dstAccessMask = 0;
            color_image_memory_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            color_image_memory_barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
            color_image_memory_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            color_image_memory_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            color_image_memory_barrier.image = s_win_images[i];
            color_image_memory_barrier.subresourceRange = color_subresource_range;

            vkCmdPipelineBarrier(cmdbuf, 0, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, NULL, 0, NULL, 1, &color_image_memory_barrier);
        }
    }

    VkClearColorValue clear_color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
    vkCmdClearColorImage(cmdbuf, s_win_images[s_win_idx], VK_IMAGE_LAYOUT_GENERAL, &clear_color, 1, &color_range);

    if (!s_glob_state->frame) {
        VkImageSubresourceRange color_subresource_range;
        color_subresource_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        color_subresource_range.baseMipLevel = 0;
        color_subresource_range.levelCount = 1;
        color_subresource_range.baseArrayLayer = 0;
        color_subresource_range.layerCount = 1;

        VkImageMemoryBarrier color_image_memory_barrier;
        color_image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        color_image_memory_barrier.pNext = NULL;
        color_image_memory_barrier.srcAccessMask = 0;
        color_image_memory_barrier.dstAccessMask = 0;
        color_image_memory_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        color_image_memory_barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        color_image_memory_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        color_image_memory_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        color_image_memory_barrier.image = s_float_image;
        color_image_memory_barrier.subresourceRange = color_subresource_range;

        vkCmdPipelineBarrier(cmdbuf, 0, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, NULL, 0, NULL, 1, &color_image_memory_barrier);
    }

    vkCmdClearColorImage(cmdbuf, s_float_image, VK_IMAGE_LAYOUT_GENERAL, &clear_color, 1, &color_range);

    if (!s_glob_state->frame) {
        VkImageSubresourceRange ds_subresource_range;
        ds_subresource_range.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        ds_subresource_range.baseMipLevel = 0;
        ds_subresource_range.levelCount = 1;
        ds_subresource_range.baseArrayLayer = 0;
        ds_subresource_range.layerCount = 1;

        VkImageMemoryBarrier ds_image_memory_barrier;
        ds_image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        ds_image_memory_barrier.pNext = NULL;
        ds_image_memory_barrier.srcAccessMask = 0;
        ds_image_memory_barrier.dstAccessMask = 0;
        ds_image_memory_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        ds_image_memory_barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        ds_image_memory_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        ds_image_memory_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        ds_image_memory_barrier.image = s_depth_stencil_image;
        ds_image_memory_barrier.subresourceRange = ds_subresource_range;

        vkCmdPipelineBarrier(cmdbuf, 0, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, NULL, 0, NULL, 1, &ds_image_memory_barrier);
    }

    VkClearDepthStencilValue ds_value = { 1.0f, 0 };
    vkCmdClearDepthStencilImage(cmdbuf, s_depth_stencil_image, VK_IMAGE_LAYOUT_GENERAL,
                                &ds_value, 2, depth_stencil_range);
}
//=============================================================================
static void Cmd_Begin_Win_RenderPass(VkCommandBuffer cmdbuf)
{

    VkRect2D render_area = { { 0, 0 }, { s_glob_state->width, s_glob_state->height } };
    VkClearValue clear_color = { 0 };
    VkRenderPassBeginInfo rpBegin;
    rpBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpBegin.pNext = NULL;
    rpBegin.renderPass = s_win_renderpass;
    rpBegin.framebuffer = s_win_framebuffer[s_win_idx];
    rpBegin.renderArea = render_area;
    rpBegin.clearValueCount = 2;
    rpBegin.pClearValues = &clear_color;
    vkCmdBeginRenderPass(cmdbuf, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);
}
//=============================================================================
static void Cmd_End_Win_RenderPass(VkCommandBuffer cmdbuf)
{
    vkCmdEndRenderPass(cmdbuf);
}
//=============================================================================
static void Cmd_Display_Fractal(VkCommandBuffer cmdbuf)
{

    vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, s_display_pipe);
    vkCmdBindDescriptorSets(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, s_common_pipeline_layout, 0, 1, &s_common_dset[s_res_idx], 0, NULL);

    vkCmdDraw(cmdbuf, 4, 1, 0, 0);
}
//=============================================================================
static void Cmd_Render_Skybox(VkCommandBuffer cmdbuf)
{
    vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, s_skybox_generate_pipe);
    vkCmdBindDescriptorSets(cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, s_common_pipeline_layout, 0, 1, &s_common_dset[s_res_idx], 0, NULL);
    vkCmdDispatch(cmdbuf, 1, 1, 1);

    vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, NULL, 0, NULL, 0, NULL);

    VkCommandBufferBeginInfo begin_info = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL, 0, NULL
    };

    VkRect2D render_area = { { 0, 0 }, { s_glob_state->width, s_glob_state->height } };
    VkClearValue clear_color = { 0 };
    VkRenderPassBeginInfo rpBegin;
    rpBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpBegin.pNext = NULL;
    rpBegin.renderPass = s_float_renderpass;
    rpBegin.framebuffer = s_float_framebuffer;
    rpBegin.renderArea = render_area;
    rpBegin.clearValueCount = 2;
    rpBegin.pClearValues = &clear_color;
    vkCmdBeginRenderPass(cmdbuf, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, s_skybox_pipe);
    vkCmdBindDescriptorSets(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, s_common_pipeline_layout, 0, 1, &s_common_dset[s_res_idx], 0, NULL);

    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmdbuf, 0, 1, &s_skybox_buf, &offset);

    vkCmdDraw(cmdbuf, 14, 1, 0, 0);

    vkCmdEndRenderPass(cmdbuf);
}
//===========================================================================
static int Graph_Init(GRAPH *graph, struct graph_data_t *data, int x, int y, int w, int h, float color[4], int draw_background)
{
    if (!graph) return 0;

    graph->x = x;
    graph->y = y;
    graph->w = w;
    graph->h = h;
    graph->color[0] = color[0];
    graph->color[1] = color[1];
    graph->color[2] = color[2];
    graph->color[3] = color[3];
    graph->draw_background = draw_background;

    for (int i = 0; i < k_Resource_Buffering; ++i) {
        const VkDeviceSize size = k_Graph_Samples * sizeof(GRAPH_SHADER_IN) * 3;
        VkBufferCreateInfo buffer_info = {
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, NULL, 0, size,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE, 0, NULL
        };
        VKU_VR(vkCreateBuffer(s_gpu_device, &buffer_info, NO_ALLOC_CALLBACK, &graph->buffer[i]));

        VkMemoryRequirements mem_reqs;
        vkGetBufferMemoryRequirements(s_gpu_device, graph->buffer[i], &mem_reqs);
        VkMemoryAllocateInfo alloc_info = {
            VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, mem_reqs.size,
            Get_Mem_Type_Index(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
        };
        VKU_VR(vkAllocateMemory(s_gpu_device, &alloc_info, NO_ALLOC_CALLBACK, &graph->buffer_mem[i]));
        VKU_VR(vkBindBufferMemory(s_gpu_device, graph->buffer[i], graph->buffer_mem[i], 0));
    }

    VkViewport vp = { (float)x, (float)y, (float)w, (float)h, 0.0f, 1.0f };
    VkRect2D scissor = { { x, y }, { w, h } };

    graph->viewport.viewportCount = 1;
    graph->viewport.viewport = vp;
    graph->viewport.scissorCount = 1;
    graph->viewport.scissors = scissor;

    if (data->empty_flag) {
        Graph_Add_Sample(data, 0.0f);
        data->empty_flag = 0;
    }

    return 1;
}
//---------------------------------------------------------------------------
static void Graph_Release(GRAPH *graph)
{
    if (!graph) return;

    for (int i = 0; i < k_Resource_Buffering; ++i) {
        VKU_DESTROY(vkDestroyBuffer, graph->buffer[i]);
        VKU_FREE_MEM(graph->buffer_mem[i]);
    }
}
//---------------------------------------------------------------------------
static void Graph_Draw(GRAPH *graph, VkCommandBuffer cmdbuf)
{
    VkCommandBufferBeginInfo begin_info = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL, 0, NULL
    };

    VkDeviceSize offset = 0;

    if (graph->draw_background) {
        vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, s_graph_tri_strip_pipe);

        vkCmdSetViewport(cmdbuf, 0, graph->viewport.viewportCount, &graph->viewport.viewport);
        vkCmdSetScissor(cmdbuf, 0, graph->viewport.scissorCount, &graph->viewport.scissors);

        vkCmdBindVertexBuffers(cmdbuf, 0, 1, &s_graph_buffer[s_res_idx], &offset);
        vkCmdBindVertexBuffers(cmdbuf, 1, 1, &s_graph_buffer[s_res_idx], &offset);
        vkCmdDraw(cmdbuf, 4, 1, 0, 0);

        vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, s_graph_line_list_pipe);
        vkCmdBindVertexBuffers(cmdbuf, 0, 1, &s_graph_buffer[s_res_idx], &offset);
        vkCmdBindVertexBuffers(cmdbuf, 1, 1, &s_graph_buffer[s_res_idx], &offset);
        vkCmdDraw(cmdbuf, 38, 1, 4, 0);
    }

    // fill
    vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, s_graph_tri_strip_pipe);

    vkCmdSetViewport(cmdbuf, 0, graph->viewport.viewportCount, &graph->viewport.viewport);
    vkCmdSetScissor(cmdbuf, 0, graph->viewport.scissorCount, &graph->viewport.scissors);

    vkCmdBindVertexBuffers(cmdbuf, 0, 1, &graph->buffer[s_res_idx], &offset);
    vkCmdBindVertexBuffers(cmdbuf, 1, 1, &graph->buffer[s_res_idx], &offset);
    vkCmdDraw(cmdbuf, k_Graph_Samples * 2, 1, 0, 0);

    // line
    vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, s_graph_line_strip_pipe);
    vkCmdBindVertexBuffers(cmdbuf, 0, 1, &graph->buffer[s_res_idx], &offset);
    vkCmdBindVertexBuffers(cmdbuf, 1, 1, &graph->buffer[s_res_idx], &offset);
    vkCmdDraw(cmdbuf, k_Graph_Samples, 1, k_Graph_Samples * 2, 0);
}
//-----------------------------------------------------------------------------
static int Graph_Update_Buffer(GRAPH *graph, struct graph_data_t* data)
{
    GRAPH_SHADER_IN *ptr;
    VKU_VR(vkMapMemory(s_gpu_device, graph->buffer_mem[s_res_idx], 0, VK_WHOLE_SIZE, 0, (void **)&ptr));

    // fill
    int idx = data->sampleidx;
    for (int i = 0; i < k_Graph_Samples; ++i) {
        if (--idx < 0) idx = k_Graph_Samples - 1;
        ptr->p.x = (1.0f - i * (2.0f / k_Graph_Samples)) * 0.999f - 0.02f;
        ptr->p.y = -(2.0f * (data->sampley[idx] * data->scale - 0.5f)) * 0.97f;
        ptr->c.x = graph->color[0] * 0.5f;
        ptr->c.y = graph->color[1] * 0.5f;
        ptr->c.z = graph->color[2] * 0.5f;
        ptr->c.w = graph->color[3] * 0.1f;
        ptr++;

        ptr->p.x = (1.0f - i * (2.0f / k_Graph_Samples)) * 0.999f - 0.02f;
        ptr->p.y = -(2.0f * ((data->sample[idx] + data->sampley[idx]) * data->scale - 0.5f)) * 0.97f;
        ptr->c.x = graph->color[0] * 0.5f;
        ptr->c.y = graph->color[1] * 0.5f;
        ptr->c.z = graph->color[2] * 0.5f;
        ptr->c.w = graph->color[3] * 0.1f;
        ptr++;
    }

    // line
    idx = data->sampleidx;
    for (int i = 0; i < k_Graph_Samples; ++i) {
        if (--idx < 0) idx = k_Graph_Samples - 1;
        ptr->p.x = (1.0f - i * (2.0f / k_Graph_Samples)) * 0.999f - 0.02f;
        ptr->p.y = -(2.0f * ((data->sample[idx] + data->sampley[idx]) * data->scale - 0.5f)) * 0.97f;
        ptr->c.x = graph->color[0];
        ptr->c.y = graph->color[1];
        ptr->c.z = graph->color[2];
        ptr->c.w = graph->color[3];
        ptr++;
    }

    vkUnmapMemory(s_gpu_device, graph->buffer_mem[s_res_idx]);
    return 1;
}
//===========================================================================
static int Create_Common_Graph_Resources(void)
{
    for (int i = 0; i < k_Resource_Buffering; ++i) {
        const VkDeviceSize size = 64 * sizeof(GRAPH_SHADER_IN);
        VkBufferCreateInfo buffer_info = {
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, NULL, 0, size,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE, 0, NULL
        };
        VKU_VR(vkCreateBuffer(s_gpu_device, &buffer_info, NO_ALLOC_CALLBACK, &s_graph_buffer[i]));

        VkMemoryRequirements mem_reqs;
        vkGetBufferMemoryRequirements(s_gpu_device, s_graph_buffer[i], &mem_reqs);
        VkMemoryAllocateInfo alloc_info = {
            VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, mem_reqs.size,
            Get_Mem_Type_Index(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
        };
        VKU_VR(vkAllocateMemory(s_gpu_device, &alloc_info, NO_ALLOC_CALLBACK, &s_graph_buffer_mem[i]));
        VKU_VR(vkBindBufferMemory(s_gpu_device, s_graph_buffer[i], s_graph_buffer_mem[i], 0));
    }

    return 1;
}
//===========================================================================
static int Update_Common_Graph_Resources(void)
{
    GRAPH_SHADER_IN *ptr;
    VKU_VR(vkMapMemory(s_gpu_device, s_graph_buffer_mem[s_res_idx], 0, VK_WHOLE_SIZE, 0, (void **)&ptr));

    // background
    ptr->p.x = -1.0f; ptr->p.y = -1.0f;
    ptr->c.x = 0.0f; ptr->c.y = 0.0f; ptr->c.z = 0.0f; ptr->c.w = 0.3f;
    ptr++;
    ptr->p.x = 1.0f; ptr->p.y = -1.0f;
    ptr->c.x = 0.0f; ptr->c.y = 0.0f; ptr->c.z = 0.0f; ptr->c.w = 0.3f;
    ptr++;
    ptr->p.x = -1.0f; ptr->p.y = 1.0f;
    ptr->c.x = 0.0f; ptr->c.y = 0.0f; ptr->c.z = 0.0f; ptr->c.w = 0.3f;
    ptr++;
    ptr->p.x = 1.0f; ptr->p.y = 1.0f;
    ptr->c.x = 0.0f; ptr->c.y = 0.0f; ptr->c.z = 0.0f; ptr->c.w = 0.3f;
    ptr++;

    const float woff = 2.0f / k_Graph_Width;
    const float hoff = 2.0f / k_Graph_Height;
    // frame
    ptr->p.x = -1.0f + woff; ptr->p.y = -1.0f + hoff;
    ptr->c.x = 0.0f; ptr->c.y = 0.3f; ptr->c.z = 0.55f; ptr->c.w = 0.25f;
    ptr++;
    ptr->p.x = 1.0f - woff; ptr->p.y = -1.0f + hoff;
    ptr->c.x = 0.0f; ptr->c.y = 0.3f; ptr->c.z = 0.55f; ptr->c.w = 0.25f;
    ptr++;
    ptr->p.x = 1.0f - woff; ptr->p.y = -1.0f + hoff;
    ptr->c.x = 0.0f; ptr->c.y = 0.3f; ptr->c.z = 0.55f; ptr->c.w = 0.25f;
    ptr++;
    ptr->p.x = 1.0f - woff; ptr->p.y = 1.0f - hoff;
    ptr->c.x = 0.0f; ptr->c.y = 0.3f; ptr->c.z = 0.55f; ptr->c.w = 0.25f;
    ptr++;
    ptr->p.x = 1.0f - woff; ptr->p.y = 1.0f - hoff;
    ptr->c.x = 0.0f; ptr->c.y = 0.3f; ptr->c.z = 0.55f; ptr->c.w = 0.25f;
    ptr++;
    ptr->p.x = -1.0f + woff; ptr->p.y = 1.0f - hoff;
    ptr->c.x = 0.0f; ptr->c.y = 0.3f; ptr->c.z = 0.55f; ptr->c.w = 0.25f;
    ptr++;
    ptr->p.x = -1.0f + woff; ptr->p.y = 1.0f - hoff;
    ptr->c.x = 0.0f; ptr->c.y = 0.3f; ptr->c.z = 0.55f; ptr->c.w = 0.25f;
    ptr++;
    ptr->p.x = -1.0f + woff; ptr->p.y = -1.0f + hoff;
    ptr->c.x = 0.0f; ptr->c.y = 0.3f; ptr->c.z = 0.55f; ptr->c.w = 0.25f;
    ptr++;

    for (int i = 1; i <= 6; ++i) {
        ptr->p.x = -1.0f + i * (2.0f / 6.0f) - 2.0f * ((s_glob_state->graph_data[0].sampleidx % (k_Graph_Samples / 6)) / (float)k_Graph_Samples);
        ptr->p.y = -1.0f;
        ptr->c.x = 0.0f; ptr->c.y = 0.1f; ptr->c.z = 0.2f; ptr->c.w = 0.25f;
        ptr++;
        ptr->p.x = -1.0f + i * (2.0f / 6.0f) - 2.0f * ((s_glob_state->graph_data[0].sampleidx % (k_Graph_Samples / 6)) / (float)k_Graph_Samples);
        ptr->p.y = 1.0f;
        ptr->c.x = 0.0f; ptr->c.y = 0.1f; ptr->c.z = 0.2f; ptr->c.w = 0.25f;
        ptr++;
    }
    for (int i = 1; i <= 9; ++i) {
        ptr->p.y = -1.0f + i * (2.0f / 10.0f);
        ptr->p.x = -1.0f;
        ptr->c.x = 0.0f; ptr->c.y = 0.1f; ptr->c.z = 0.2f; ptr->c.w = 0.25f;
        ptr++;
        ptr->p.y = -1.0f + i * (2.0f / 10.0f);
        ptr->p.x = 1.0f;
        ptr->c.x = 0.0f; ptr->c.y = 0.1f; ptr->c.z = 0.2f; ptr->c.w = 0.25f;
        ptr++;
    }

    // combined power graph legend
    ptr->p.x = -1.0f; ptr->p.y = -1.0f;
    ptr->c.x = 0.85f; ptr->c.y = 0.9f; ptr->c.z = 0.0f; ptr->c.w = 0.25f;
    ptr++;
    ptr->p.x = 1.0f; ptr->p.y = -1.0f;
    ptr->c.x = 0.85f; ptr->c.y = 0.9f; ptr->c.z = 0.0f; ptr->c.w = 0.25f;
    ptr++;
    ptr->p.x = -1.0f; ptr->p.y = 1.0f;
    ptr->c.x = 0.85f; ptr->c.y = 0.9f; ptr->c.z = 0.0f; ptr->c.w = 0.25f;
    ptr++;
    ptr->p.x = 1.0f; ptr->p.y = 1.0f;
    ptr->c.x = 0.85f; ptr->c.y = 0.9f; ptr->c.z = 0.0f; ptr->c.w = 0.25f;
    ptr++;

    ptr->p.x = -1.0f; ptr->p.y = -1.0f;
    ptr->c.x = 0.85f; ptr->c.y = 0.0f; ptr->c.z = 0.0f; ptr->c.w = 0.25f;
    ptr++;
    ptr->p.x = 1.0f; ptr->p.y = -1.0f;
    ptr->c.x = 0.85f; ptr->c.y = 0.0f; ptr->c.z = 0.0f; ptr->c.w = 0.25f;
    ptr++;
    ptr->p.x = -1.0f; ptr->p.y = 1.0f;
    ptr->c.x = 0.85f; ptr->c.y = 0.0f; ptr->c.z = 0.0f; ptr->c.w = 0.25f;
    ptr++;
    ptr->p.x = 1.0f; ptr->p.y = 1.0f;
    ptr->c.x = 0.85f; ptr->c.y = 0.0f; ptr->c.z = 0.0f; ptr->c.w = 0.25f;
    ptr++;

    vkUnmapMemory(s_gpu_device, s_graph_buffer_mem[s_res_idx]);

    return 1;
}
//===========================================================================
static int Create_Font_Resources(void)
{
    static unsigned char font24pixels[STB_FONT_consolas_24_usascii_BITMAP_HEIGHT][STB_FONT_consolas_24_usascii_BITMAP_WIDTH];
    stb_font_consolas_24_usascii(s_font_24_data, font24pixels, STB_FONT_consolas_24_usascii_BITMAP_HEIGHT);

    for (int i = 0; i < k_Resource_Buffering; ++i) {
        const VkDeviceSize size = 1024 * sizeof(VmathVector4);
        VkBufferCreateInfo buffer_info = {
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, NULL, 0, size,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE, 0, NULL
        };
        VKU_VR(vkCreateBuffer(s_gpu_device, &buffer_info, NO_ALLOC_CALLBACK, &s_font_buffer[i]));
        VkMemoryRequirements mem_reqs;
        vkGetBufferMemoryRequirements(s_gpu_device, s_font_buffer[i], &mem_reqs);

        VkMemoryAllocateInfo alloc_info = {
            VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, mem_reqs.size,
            Get_Mem_Type_Index(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
        };
        VKU_VR(vkAllocateMemory(s_gpu_device, &alloc_info, NO_ALLOC_CALLBACK, &s_font_buffer_mem[i]));
        VKU_VR(vkBindBufferMemory(s_gpu_device, s_font_buffer[i], s_font_buffer_mem[i], 0));
    }

    VkImageCreateInfo image_info = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, NULL, 0,
        VK_IMAGE_TYPE_2D, VK_FORMAT_R8_UNORM,
        { STB_FONT_consolas_24_usascii_BITMAP_WIDTH, STB_FONT_consolas_24_usascii_BITMAP_HEIGHT, 1 },
        1, 1, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_LINEAR, VK_IMAGE_USAGE_SAMPLED_BIT, VK_SHARING_MODE_EXCLUSIVE, 0,
        NULL, VK_IMAGE_LAYOUT_PREINITIALIZED
    };
    VKU_VR(vkCreateImage(s_gpu_device, &image_info, NO_ALLOC_CALLBACK, &s_font_image));

    VkMemoryRequirements mem_reqs;
    vkGetImageMemoryRequirements(s_gpu_device, s_font_image, &mem_reqs);
    VkMemoryAllocateInfo alloc_info = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL,
        mem_reqs.size,
        Get_Mem_Type_Index(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
    };
    VKU_VR(vkAllocateMemory(s_gpu_device, &alloc_info, NO_ALLOC_CALLBACK, &s_font_image_mem));
    VKU_VR(vkBindImageMemory(s_gpu_device, s_font_image, s_font_image_mem, 0));

    VkImageViewCreateInfo image_view_info = {
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, NULL, 0,
        s_font_image, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R8_UNORM,
        {
            VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B,
            VK_COMPONENT_SWIZZLE_A
        },
        { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };
    VKU_VR(vkCreateImageView(s_gpu_device, &image_view_info, NO_ALLOC_CALLBACK, &s_font_image_view));

    unsigned char *ptr;
    VKU_VR(vkMapMemory(s_gpu_device, s_font_image_mem, 0, VK_WHOLE_SIZE, 0, (void **)&ptr));
    memcpy(ptr, &font24pixels[0][0], STB_FONT_consolas_24_usascii_BITMAP_WIDTH * STB_FONT_consolas_24_usascii_BITMAP_HEIGHT);
    vkUnmapMemory(s_gpu_device, s_font_image_mem);

    return 1;
}
//----------------------------------------------------------------------------
static int Create_Font_Pipeline(void)
{
    VkShaderModule vs, fs;
    vs = VK_NULL_HANDLE;
    fs = VK_NULL_HANDLE;

    VKU_Load_Shader(s_gpu_device, "Data/Shader_GLSL/VS_Font.bil", &vs);
    VKU_Load_Shader(s_gpu_device, "Data/Shader_GLSL/FS_Font.bil", &fs);

    if (vs == VK_NULL_HANDLE || fs == VK_NULL_HANDLE) {
        VKU_DESTROY(vkDestroyShaderModule, vs);
        VKU_DESTROY(vkDestroyShaderModule, fs);
        LOG_AND_RETURN0();
    }

    VkPipelineShaderStageCreateInfo vs_info = {
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        NULL, 0, VK_SHADER_STAGE_VERTEX_BIT, vs, "main", NULL
    };
    VkPipelineShaderStageCreateInfo fs_info = {
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        NULL, 0, VK_SHADER_STAGE_FRAGMENT_BIT, fs, "main", NULL
    };
    VkPipelineInputAssemblyStateCreateInfo ia_info = {
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, NULL, 0,
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, VK_FALSE
    };
    VkPipelineTessellationStateCreateInfo tess_info = {
        VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO, NULL, 0, 0
    };
    VkPipelineViewportStateCreateInfo vp_info = {
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, NULL, 0, s_vp_state.viewportCount,
        &s_vp_state.viewport, s_vp_state.scissorCount, &s_vp_state.scissors
    };
    VkPipelineRasterizationStateCreateInfo rs_info = {
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, NULL, 0, VK_FALSE,
        VK_FALSE, VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_FALSE,
        s_rs_state.depthBias, s_rs_state.depthBiasClamp,
        s_rs_state.slopeScaledDepthBias, s_rs_state.lineWidth
    };
    VkPipelineMultisampleStateCreateInfo ms_info = {
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, NULL, 0, 1, VK_FALSE,
        1.0f, NULL, VK_FALSE, VK_FALSE
    };
    VkPipelineColorBlendAttachmentState attachment = {
        VK_TRUE, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE, VK_BLEND_OP_ADD,
        VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE, VK_BLEND_OP_ADD, 0x0f
    };
    VkPipelineColorBlendStateCreateInfo cb_info = {
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, NULL, 0, VK_FALSE, 
        VK_LOGIC_OP_COPY, 1, &attachment, s_cb_state.blendConst[0]
    };
    VkPipelineDepthStencilStateCreateInfo db_info = {
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, NULL, 0,
        VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL, VK_FALSE, VK_FALSE,
        { VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 0, 0, 0 },
        { VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 0, 0, 0 },
        s_ds_state.minDepthBounds, s_ds_state.maxDepthBounds
    };

    VkVertexInputBindingDescription vf_binding_desc[] = {
        { 0, sizeof(VmathVector4), VK_VERTEX_INPUT_RATE_VERTEX },
        { 1, sizeof(VmathVector4), VK_VERTEX_INPUT_RATE_VERTEX }
    };
    VkVertexInputAttributeDescription vf_attribute_desc[] = {
        {
            0, 0, VK_FORMAT_R32G32_SFLOAT, 0
        },
        {
            1, 1, VK_FORMAT_R32G32_SFLOAT, sizeof(VmathVector2)
        }
    };
    VkPipelineVertexInputStateCreateInfo vf_info = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, NULL, 0,
        SDL_arraysize(vf_binding_desc), vf_binding_desc, SDL_arraysize(vf_attribute_desc), vf_attribute_desc
    };

    VkPipelineShaderStageCreateInfo shader_stage_infos[2];
    shader_stage_infos[0] = vs_info;
    shader_stage_infos[1] = fs_info;

    VkGraphicsPipelineCreateInfo pi_info = {
        VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, NULL, 0, 2, shader_stage_infos,
        &vf_info, &ia_info, &tess_info, &vp_info, &rs_info, &ms_info, &db_info, &cb_info,
        NULL, s_common_pipeline_layout, s_win_renderpass, 0, VK_NULL_HANDLE, 0
    };

    VkResult r = vkCreateGraphicsPipelines(s_gpu_device, VK_NULL_HANDLE, 1, &pi_info, NO_ALLOC_CALLBACK, &s_font_pipe);

    VKU_DESTROY(vkDestroyShaderModule, vs);
    VKU_DESTROY(vkDestroyShaderModule, fs);

    if (r != VK_SUCCESS) LOG_AND_RETURN0();

    return 1;
}
//-----------------------------------------------------------------------------
static int Add_Text(VmathVector4 **ptr, const char *str, int x, int y)
{
    float recip_width = 1.0f / s_glob_state->width;
    float recip_height = 1.0f / s_glob_state->height;
    int letters = 0;

    while (*str) {
        int char_codepoint = *str++;
        stb_fontchar *cd = &s_font_24_data[char_codepoint - STB_FONT_consolas_24_usascii_FIRST_CHAR];

        const float ydir = -1.0;

        (*ptr)->x = (x + cd->x0) * 2.0f * recip_width - 1.0f;
        (*ptr)->y = ydir * (2.0f - (y + cd->y0) * 2.0f * recip_height - 1.0f);
        (*ptr)->z = cd->s0;
        (*ptr)->w = cd->t0;
        (*ptr)++;

        (*ptr)->x = (x + cd->x1) * 2.0f * recip_width - 1.0f;
        (*ptr)->y = ydir * (2.0f - (y + cd->y0) * 2.0f * recip_height - 1.0f);
        (*ptr)->z = cd->s1;
        (*ptr)->w = cd->t0;
        (*ptr)++;

        (*ptr)->x = (x + cd->x0) * 2.0f * recip_width - 1.0f;
        (*ptr)->y = ydir * (2.0f - (y + cd->y1) * 2.0f * recip_height - 1.0f);
        (*ptr)->z = cd->s0;
        (*ptr)->w = cd->t1;
        (*ptr)++;

        (*ptr)->x = (x + cd->x1) * 2.0f * recip_width - 1.0f;
        (*ptr)->y = ydir * (2.0f - (y + cd->y1) * 2.0f * recip_height - 1.0f);
        (*ptr)->z = cd->s1;
        (*ptr)->w = cd->t1;
        (*ptr)++;

        x += cd->advance_int;
        letters++;
    }

    return letters;
}
//-----------------------------------------------------------------------------
static int Generate_Text(void)
{
    s_font_letter_count = 0;

    VmathVector4 *ptr;
    VKU_VR(vkMapMemory(s_gpu_device, s_font_buffer_mem[s_res_idx], 0, VK_WHOLE_SIZE, 0, (void **)&ptr));

    char str[128];
    sprintf(str, "CPU Load");
    s_font_letter_count += Add_Text(&ptr, str, s_glob_state->width - 10 - k_Graph_Width, 10);

    sprintf(str, "FPS %.1f (%.3f ms)", s_fps, s_ms);
 
    s_font_letter_count += Add_Text(&ptr, str, 10, s_glob_state->height - 90);
    s_font_letter_count += Add_Text(&ptr, "Stardust 1.1", 10, s_glob_state->height - 60);
    s_font_letter_count += Add_Text(&ptr, s_gpu_properties.deviceName, 10, s_glob_state->height - 30);

    vkUnmapMemory(s_gpu_device, s_font_buffer_mem[s_res_idx]);

    return 1;
}
//-----------------------------------------------------------------------------
static void Cmd_Draw_Text(VkCommandBuffer cmdbuf)
{
    vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, s_font_pipe);
    vkCmdBindDescriptorSets(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, s_common_pipeline_layout, 0, 1, &s_common_dset[s_res_idx], 0, NULL);

    VkDeviceSize offsets = 0;
    vkCmdBindVertexBuffers(cmdbuf, 0, 1, &s_font_buffer[s_res_idx], &offsets);
    vkCmdBindVertexBuffers(cmdbuf, 1, 1, &s_font_buffer[s_res_idx], &offsets);
    for (int i = 0; i < s_font_letter_count; ++i) {
        vkCmdDraw(cmdbuf, 4, 1, i * 4, 0);
    }
}
//-----------------------------------------------------------------------------
static uint32_t Get_Mem_Type_Index(VkMemoryPropertyFlagBits bit)
{
    VkPhysicalDeviceMemoryProperties physMemProperties;
    vkGetPhysicalDeviceMemoryProperties(s_gpu, &physMemProperties);

    for (uint32_t i = 0; i < physMemProperties.memoryTypeCount; ++i) {
        if ((physMemProperties.memoryTypes[i].propertyFlags & bit) == bit) {
            return  i;
        }
    }
    LOG_AND_RETURN0();
}
//=============================================================================
int VK_Init(struct glob_state_t* state)
{
    s_glob_state = state;

    if (!VKU_Create_Device(state->hwnd, state->width, state->height, k_Window_Buffering, state->windowed,
                           &s_queue_family_index, &s_gpu_device, &s_gpu_queue,
                           s_win_images, &s_swap_chain, &s_gpu)) {
        Log("VKU_Create_Device failed\n");
        return STARDUST_NOT_SUPPORTED;
    }
    Log("VK Device initialized\n");

	vkGetPhysicalDeviceProperties(s_gpu, &s_gpu_properties);

    if (!Demo_Init()) {
        Log("Demo_Init failed\n");
        return STARDUST_ERROR;
    } else {
        Log("Demo initialized\n");
    }

    return STARDUST_CONTINUE;
}
//=============================================================================
int VK_Run(struct glob_state_t *state)
{
    s_glob_state = state;
    Set_Exit_Code(STARDUST_CONTINUE);
    s_glob_state->width = state->width;
    s_glob_state->height = state->height;
    s_glob_state->frame = 0;
    s_res_idx = 0;

    char title[256];
    Get_Window_Title(title, "Vulkan");
    SDL_SetWindowTitle(state->window, title);

#ifdef MT_UPDATE
    Set_Exit_Code(STARDUST_CONTINUE);
    for (int i = 1; i < s_glob_state->cpu_core_count; ++i) {
        s_thread[i].thread = SDL_CreateThread(Particle_Thread, "thrd", &s_thread[i]);
        if (!s_thread[i].thread) LOG_AND_RETURN0();
    }
#endif

    while (s_exit_code == STARDUST_CONTINUE) {
        int recalculate_fps = Update_Frame_Stats(&s_time, &s_time_delta, s_glob_state->frame,
                                                 0, &s_fps, &s_ms);

        Set_Exit_Code(Handle_Events(s_glob_state));

        VkSemaphoreCreateInfo semaphoreInfo;
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        semaphoreInfo.pNext = NULL;
        semaphoreInfo.flags = 0;
        VKU_VR(vkCreateSemaphore(s_gpu_device, &semaphoreInfo, NO_ALLOC_CALLBACK,
                                 &s_swap_chain_image_ready_semaphore));

        uint32_t swap_image_index;
        VKU_VR(vkAcquireNextImageKHR(s_gpu_device, s_swap_chain, UINT64_MAX,
                                     s_swap_chain_image_ready_semaphore, VK_NULL_HANDLE, &swap_image_index));

        s_res_idx = swap_image_index;
        s_win_idx = swap_image_index;

        if (!Demo_Update()) {
            Log("Demo_Update failed\n");
            Set_Exit_Code(STARDUST_ERROR);
        }
        s_glob_state->frame++;

        if (recalculate_fps) {
            const float *cpu_load = Metrics_GetCPUData();

            for (int i = 0; i < s_glob_state->cpu_core_count; i++) {
                s_glob_state->graph_data[i].scale = 1.0f;
                float s = cpu_load[i];
                Graph_Add_Sample(&s_glob_state->graph_data[i], s / 100.0f);
                Log("CPU %d: %f", i, cpu_load[i]);
            }
        }

        swap_image_index = s_win_idx;
        if (!VKU_Present(&swap_image_index)) {
            Log("VKU_Present failed\n");
            Set_Exit_Code(STARDUST_ERROR);
        }
        VKU_DESTROY(vkDestroySemaphore, s_swap_chain_image_ready_semaphore);
    }

	if (s_exit_code != STARDUST_EXIT)
	{
		uint32_t swap_image_index = 0xffffffff;
		VKU_Present(&swap_image_index);
	}

    s_res_idx = 0;
    s_win_idx = 0;

#ifdef MT_UPDATE
    for (int i = 1; i < s_glob_state->cpu_core_count; ++i) {
        Finish_Particle_Thread(&s_thread[i]);
    }
#endif

    return s_exit_code;
}
//=============================================================================
int VK_Shutdown(struct glob_state_t *state)
{
    if (!Demo_Shutdown()) {
        return STARDUST_ERROR;
    }
    VKU_Quit();
    return STARDUST_CONTINUE;
}
//=============================================================================
