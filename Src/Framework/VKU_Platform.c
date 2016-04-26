// Copyright 2016 Intel Corporation All Rights Reserved
// 
// Intel makes no representations about the suitability of this software for any purpose.
// THIS SOFTWARE IS PROVIDED ""AS IS."" INTEL SPECIFICALLY DISCLAIMS ALL WARRANTIES,
// EXPRESS OR IMPLIED, AND ALL LIABILITY, INCLUDING CONSEQUENTIAL AND OTHER INDIRECT DAMAGES,
// FOR THE USE OF THIS SOFTWARE, INCLUDING LIABILITY FOR INFRINGEMENT OF ANY PROPRIETARY
// RIGHTS, AND INCLUDING THE WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
// Intel does not assume any responsibility for any errors which may appear in this software
// nor any responsibility to update it.

#include "VKU.h"
#include "SDL.h"

#ifdef __ANDROID__
#   include <dlfcn.h>
#else
#   include <windows.h>
#endif

//=============================================================================
typedef void               *(*PFNLoader)(const char *func);
static void                *Loader(const char *func);
static void                *VKLoader(const char *func);
static int                 Load_WsiWin_Entry_Points(PFNLoader);
static int                 Init(void *hwnd, int width, int height, VkBool32 windowed, uint32_t image_count, VkImage *images);
static void                Deinit(void);
static int                 Init_Device(void);
static int                 Init_Framebuffer(void *hwnd, int width, int height, VkBool32 windowed,
                                            uint32_t image_count, VkImage *images);
//=============================================================================
static void                *s_vk_dll = NULL;
static VkInstance          s_instance = VK_NULL_HANDLE;
static VkPhysicalDevice    s_gpu = VK_NULL_HANDLE;
static VkDevice            s_device = VK_NULL_HANDLE;
static VkQueue             s_queue = VK_NULL_HANDLE;
static uint32_t            s_queue_family_index = 0;
static VkSwapchainKHR      s_swap_chain = { VK_NULL_HANDLE };
static int                 s_back_buffer = 0;
static VkSurfaceKHR        s_surface = VK_NULL_HANDLE;
//=============================================================================
static void *Loader(const char *func)
{
#ifdef __ANDROID__
    return dlsym(s_vk_dll, func);
#else
    return GetProcAddress(s_vk_dll, func);
#endif
}
//-----------------------------------------------------------------------------
static void *VKLoader(const char *func)
{
    if (vkGetDeviceProcAddr == NULL || s_device == VK_NULL_HANDLE) {
        return Loader(func);
    }
    return vkGetDeviceProcAddr(s_device, func);
}
//=============================================================================
static int Init(void *hwnd, int width, int height, VkBool32 windowed,
                uint32_t image_count, VkImage *images)
{
#ifdef __ANDROID__
    s_vk_dll = dlopen("libigvk.so", RTLD_NOW);
#else
    s_vk_dll = LoadLibrary("vulkan-1.dll");
    if (!s_vk_dll) {
        MessageBox((HWND)hwnd, "Failed to load Vulkan loader.", "Error", MB_ICONERROR);
    }
#endif
    if (!s_vk_dll) return 0;

    int VK__Load_Api(void *(*)(const char *));
    int VK__Load_Init_Api(void *(*)(const char *));
    if (!VK__Load_Init_Api(VKLoader)) return 0;
    if (!Init_Device()) return 0;
    if (!VK__Load_Api(VKLoader)) return 0;

    vkGetDeviceQueue(s_device, s_queue_family_index, 0, &s_queue);

    if (!Init_Framebuffer(hwnd, width, height, windowed, image_count, images))
        return 0;

    return 1;
}
//=============================================================================
static void Deinit(void)
{
    if (s_device) vkDeviceWaitIdle(s_device);

    if (s_swap_chain) {
        vkDestroySwapchainKHR(s_device, s_swap_chain, NO_ALLOC_CALLBACK);
        s_swap_chain = VK_NULL_HANDLE;
    }
    if (s_device) {
        vkDestroyDevice(s_device, NO_ALLOC_CALLBACK);
        s_device = VK_NULL_HANDLE;
    }
    if (s_instance) {
        vkDestroyInstance(s_instance, NO_ALLOC_CALLBACK);
        s_instance = VK_NULL_HANDLE;
    }
    if (s_vk_dll) {
#ifdef __ANDROID__
        dlclose(s_vk_dll);
#else
        FreeLibrary(s_vk_dll);
#endif
        s_vk_dll = NULL;
    }
}
//=============================================================================
static int Init_Device(void)
{
    VkApplicationInfo app_info  = { 0 };
    app_info.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pNext              = NULL;
    app_info.pApplicationName   = "Stardust";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName        = "Starduster1";
    app_info.engineVersion      = VK_MAKE_VERSION(1, 1, 0);
    app_info.apiVersion         = VK_API_VERSION;

    const char* wsi_extensions[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_PLATFORM_SPECIFIC_SURFACE_EXTENSION_NAME
    };

    VkInstanceCreateInfo instance_info = { 0 };
    instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_info.pNext = NULL;
    instance_info.flags = 0;
    instance_info.pApplicationInfo = &app_info;
    instance_info.enabledLayerCount = 0;
    instance_info.ppEnabledLayerNames = NULL;
    instance_info.enabledExtensionCount = SDL_arraysize(wsi_extensions);
    instance_info.ppEnabledExtensionNames = wsi_extensions;

    VKU_VR(vkCreateInstance(&instance_info, NO_ALLOC_CALLBACK, &s_instance));

    uint32_t count = 1;
    VKU_VR(vkEnumeratePhysicalDevices(s_instance, &count, &s_gpu));

    const char *extensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queue_info;
    queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    queue_info.pNext = NULL;
    queue_info.flags = 0;
    queue_info.queueFamilyIndex = 0;
    queue_info.queueCount = 1;
    queue_info.pQueuePriorities = &queuePriority;

    VkDeviceCreateInfo device_info = { 0 };
    device_info.sType                       = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_info.pNext                       = NULL;
    device_info.flags                       = 0;
    device_info.queueCreateInfoCount        = 1;
    device_info.pQueueCreateInfos           = &queue_info;
    device_info.enabledLayerCount           = 0;
    device_info.ppEnabledLayerNames         = NULL;
    device_info.enabledExtensionCount       = SDL_arraysize(extensions);
    device_info.ppEnabledExtensionNames     = extensions;
    device_info.pEnabledFeatures            = NULL;

    VKU_VR(vkCreateDevice(s_gpu, &device_info, NO_ALLOC_CALLBACK, &s_device));

    uint32_t queuesCount;
    vkGetPhysicalDeviceQueueFamilyProperties(s_gpu, &queuesCount, NULL);

    VkQueueFamilyProperties *queueFamilyProperties =
        (VkQueueFamilyProperties*)malloc(sizeof(VkQueueFamilyProperties) * queuesCount);
    vkGetPhysicalDeviceQueueFamilyProperties(s_gpu, &queuesCount, queueFamilyProperties);

    for (uint32_t i = 0; i < queuesCount; i++) {
        if ((queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0 &&
            (queueFamilyProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT) != 0)
            s_queue_family_index = i;
    }

    return 1;
}
//=============================================================================
static int Init_Framebuffer(void *hwnd, int width, int height, VkBool32 windowed,
                            uint32_t image_count, VkImage *images)
{
    VkFormat image_format = VK_FORMAT_UNDEFINED;

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
    const VkAndroidSurfaceCreateInfoKHR androidSurfaceCreateInfo = {
        VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR, NULL, 0, hwnd
    };
    VKU_VR(vkCreateAndroidSurfaceKHR(s_instance, &androidSurfaceCreateInfo, NO_ALLOC_CALLBACK, &s_surface));
#elif defined(VK_USE_PLATFORM_WIN32_KHR)
    if (!vkGetPhysicalDeviceWin32PresentationSupportKHR(s_gpu, s_queue_family_index)) {
        Log("vkGetPhysicalDeviceWin32PresentationSupportKHR returned FALSE, %d queue index does not support presentation\n", s_queue_family_index);
        LOG_AND_RETURN0();
    }
    HINSTANCE hinstance = (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE);
    const VkWin32SurfaceCreateInfoKHR win32SurfaceCreateInfo = {
        VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR, NULL, 0, hinstance, hwnd
    };
    VKU_VR(vkCreateWin32SurfaceKHR(s_instance, &win32SurfaceCreateInfo, NO_ALLOC_CALLBACK, &s_surface));
#endif

    VkBool32 surface_supported;
    VKU_VR(vkGetPhysicalDeviceSurfaceSupportKHR(s_gpu, s_queue_family_index, s_surface, &surface_supported));
    if (!surface_supported) {
        Log("vkGetPhysicalDeviceSurfaceSupportKHR returned FALSE, %d queue index does not support given surface\n", s_queue_family_index);
        LOG_AND_RETURN0();
    }

    // Surface capabilities
    VkSurfaceCapabilitiesKHR surface_caps;
    VKU_VR(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(s_gpu, s_surface, &surface_caps));

    if (surface_caps.maxImageCount != 0 && image_count > surface_caps.maxImageCount) {
        image_count = surface_caps.maxImageCount;
        Log("Surface could not support requested image count. Got maximum possilble: %d\n", surface_caps.maxImageCount);
    }

    if (!(surface_caps.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)) LOG_AND_RETURN0();

    // Surface formats
    uint32_t formats_count;
    VKU_VR(vkGetPhysicalDeviceSurfaceFormatsKHR(s_gpu, s_surface, &formats_count, NULL));
    VkSurfaceFormatKHR *surface_formats = (VkSurfaceFormatKHR *)malloc(sizeof(VkSurfaceFormatKHR) * formats_count);
    VKU_VR(vkGetPhysicalDeviceSurfaceFormatsKHR(s_gpu, s_surface, &formats_count, surface_formats));

    VkSurfaceFormatKHR *chosen_surface_format = NULL;
    for (uint32_t i = 0; i < formats_count; i++) {
        if (surface_formats[i].format == VK_FORMAT_R8G8B8A8_UNORM && surface_formats[i].colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR) {
            chosen_surface_format = &surface_formats[i];
            break;
        }
    }

    if (!chosen_surface_format && formats_count) {
        Log("VK_FORMAT_R8G8B8A8_UNORM (SRGB_NONLINEAR) not found in supported surface formats, will use any available\n");
        chosen_surface_format = &surface_formats[0];
    }
    if (!chosen_surface_format) {
        Log("No supported VkSurface format\n");
        LOG_AND_RETURN0();
    }

    image_format = chosen_surface_format->format;
    if (!windowed && !(image_format >= VK_FORMAT_R8G8B8A8_UNORM && image_format <= VK_FORMAT_R8G8B8A8_SRGB)) {
        Log("Swap chain image format's Bits Per Pixel != 32, fullscreen not supported\n");
        LOG_AND_RETURN0();
    }

    // Surface present modes
    VkPresentModeKHR *chosen_present_mode = NULL;
    uint32_t present_mode_count = 0;
    VKU_VR(vkGetPhysicalDeviceSurfacePresentModesKHR(s_gpu, s_surface, &present_mode_count, NULL));
    VkPresentModeKHR *present_modes = (VkPresentModeKHR *)malloc(sizeof(VkPresentModeKHR) * present_mode_count);
    VKU_VR(vkGetPhysicalDeviceSurfacePresentModesKHR(s_gpu, s_surface, &present_mode_count, present_modes));

    for (uint32_t i = 0; i< present_mode_count; i++) {
        if (present_modes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR || present_modes[i] == VK_PRESENT_MODE_FIFO_KHR) {
            chosen_present_mode = &present_modes[i];
            break;
        }
    }

    if (!chosen_present_mode && present_mode_count) {
        Log("No supported present modes found, will use any available\n");
        chosen_present_mode = &present_modes[0];
    }
    if (!chosen_present_mode) {
        Log("No supported present modes\n");
        LOG_AND_RETURN0();
    }

    // Create VkSwapChain
    VkSwapchainCreateInfoKHR create_info = { 0 };
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.pNext = NULL;
    create_info.flags = 0;
    create_info.surface = s_surface;
    create_info.minImageCount = image_count;
    create_info.imageFormat = image_format;
    create_info.imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
    create_info.imageExtent.width = (int32_t)width;
    create_info.imageExtent.height = (int32_t)height;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    create_info.queueFamilyIndexCount = 1;
    create_info.pQueueFamilyIndices = &s_queue_family_index;
    create_info.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create_info.presentMode = *chosen_present_mode;
    create_info.clipped = VK_FALSE;
    create_info.oldSwapchain = VK_NULL_HANDLE;

    VKU_VR(vkCreateSwapchainKHR(s_device, &create_info, NO_ALLOC_CALLBACK, &s_swap_chain));
    uint32_t swapchain_image_count = 0;
    VKU_VR(vkGetSwapchainImagesKHR(s_device, s_swap_chain, &swapchain_image_count, NULL));
    VKU_VR(vkGetSwapchainImagesKHR(s_device, s_swap_chain, &swapchain_image_count, images));
    if (image_count != swapchain_image_count) {
        Log("Number of images returned in vkGetSwapchainImagesKHR does not match requested number\n");
        LOG_AND_RETURN0();
    }

    return 1;
}
//=============================================================================
// vk
//=============================================================================
int VKU_Create_Device(void             *hwnd,
                      int              width,
                      int              height,
                      int              win_image_count,
                      VkBool32         windowed,
                      uint32_t         *queue_family_index,
                      VkDevice         *device,
                      VkQueue          *queue,
                      VkImage          *images,
                      VkSwapchainKHR   *swap_chain,
                      VkPhysicalDevice *gpu)
{
    if (!Init(hwnd, width, height, windowed, win_image_count, images)) {
        Deinit();
        return 0;
    }

    *device = s_device;
    *queue = s_queue;
    *gpu = s_gpu;
    *swap_chain = s_swap_chain;
    *queue_family_index = s_queue_family_index;

    return 1;
}
//-----------------------------------------------------------------------------
void VKU_Quit(void)
{
    Deinit();
}
//-----------------------------------------------------------------------------
int VKU_Present(uint32_t *image_indice)
{
	VkResult result[1] = { 0 };
    VkPresentInfoKHR present_info = { 0 };
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.pNext = NULL;
    present_info.waitSemaphoreCount = 0;
    present_info.pWaitSemaphores = NULL;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &s_swap_chain;
    present_info.pImageIndices = image_indice;
    present_info.pResults = result;

    VKU_VR(vkQueuePresentKHR(s_queue, &present_info));
    VKU_VR(result[0]);

    return 1;
}
//=============================================================================
