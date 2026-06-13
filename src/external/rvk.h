/* 
    rvk - v1.0.0 - MIT license - https://github.com/satchelfrost/rvk

    A C99 stb-style header-only library for Vulkan.

Usage:

    #define RVK_IMPLEMENTATION
    #include "rvk.h"

    int main()
    {
        // TODO:
    }

API Conventions:
    r_*:

        The functions beginning with "r_" are helper/lazy functions.
        Meant for quick prototyping, but if you need to be more explicit then use vk_*.

    vk_*:

        Thin wrapper macros named the same as to the Vulkan-proper functions, but snake_case.
        They allow the members of Vk*Info structs to be treated as optional arguments.
        Here are the main differences from the Vulkan-proper functions:

            1) They have optional "." arguments for Vk*Info struct members,
               e.g.

               vk_create_instance(NULL, &inst, .ppEnabledLayerNames = layers, .enabledLayerCount = 1);
                                   ^      ^                 ^                         ^
                                   |      |                 |                         |
                                   +--+---+                 +------------+------------+
                                      |                                  |
                              required arguments      optional arguments from VkInstanceCreateInfo;
                              are the pAllocator     if not set explicitly, then they will be
                            (NULL is valid), and            implicitly zero-initialized.
                                the pInstance

               Except for Vk*Info struct members which must now go last, The order of the parameters are
               preserved from the Vulkan-proper function.

            2) Instead of returning a VkResult, they return true on VK_SUCCESS and false otherwise.

            3) The ".sType" field in Vk*Info structs do not need to be set
               e.g.

               vk_create_instance(
                   NULL,
                   &inst,
                   .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO <--- unnecessary, can be left out
                );

               ALSO,

               VkApplicationInfo app_info = {
                   .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO, <--- unnecessary, can be left out
                   .pApplicationName = APP_NAME,
                   .applicationVersion = VK_MAKE_VERSION(0, 0, 1),
                   .pEngineName = "Cool Vulkan Renderer",
                   .engineVersion = VK_MAKE_VERSION(0, 0, 1),
                   .apiVersion = VK_API_VERSION_1_3,
               };
               vk_create_instance(NULL, &inst, .pApplicationInfo = &app_info);

               ***WARNING***: the exception to this rule is a structure in a pNext chain (i.e. linked list).

    RVK_*:

        defines/macros are prefixed with all caps "RVK_".
        e.g.

        RVK_INFO <--- log level
        RVK_IMPLEMENTATION <--- #define

    Rvk_*:

        typedefed structures/enums are prefixed with "Rvk_"
        e.g.

        Rvk_Swapchain <--- custom structure for swapchain
        Rvk_Log_Level <--- the name of the enum for logging

Other Notes:

    * There are no vk_* macros for Vulkan functions that do not pass in Vk*Info structs (e.g. vkDestroy* etc.)
    * There is no vk_create_graphics_pipelines (with an "s"), because this passes a pointer to a list of
      VkGraphicsPipelineCreateInfo structs, and this breaks the __VA_ARGS__ macro trick.
      As a compromise there is "vk_create_graphics_pipeline" (without an "S"), which allows the
      macro trick to work, but means you can only create one graphics pipeline at a time.
    * r_temp_default_* functions allocate temporary memory using r_temp_alloc. This is a temporary
      allocator which can be reset/rewound if desired.
*/

#ifndef RVK_H_
#define RVK_H_

#include <stdint.h>
#include <sys/types.h>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan.h>
#include <assert.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <stdbool.h>

#ifndef RVK_ASSERT
#define RVK_ASSERT assert
#endif // RVK_ASSERT

#ifndef RVK_REALLOC
#define RVK_REALLOC realloc
#endif // RVK_REALLOC

#ifndef RVK_FREE
#define RVK_FREE free
#endif // RVK_FREE

typedef struct {
    float x, y, z;
} Rvk_Vector3;

typedef struct {
    float x, y;
} Rvk_Vector2;

typedef struct {
    Rvk_Vector2 position;
    Rvk_Vector3 color;
} Rvk_Primitive_2D_Vertex;

typedef struct {
    Rvk_Vector3 position;
    uint32_t color;
} Rvk_Primitive_Point;

typedef struct {
    Rvk_Vector3 position;
    uint32_t color;
} Rvk_Line_Vertex;

typedef struct {
    VkPipeline handle;
    VkPipelineLayout layout;
} Rvk_Pipeline;

/***********************************************************************************
*  r_* API declarations
************************************************************************************/

/* logging and error handling */
typedef enum { RVK_VERBOSE, RVK_INFO, RVK_WARNING, RVK_ERROR, } Rvk_Log_Level;
void r_log(Rvk_Log_Level level, const char *fmt, ...);
const char *r_vk_res_to_str(VkResult res);
bool r_check_vk_result(VkResult result, const char* function);
#define RVK(func) r_check_vk_result(func, #func)
VkDebugUtilsMessengerCreateInfoEXT r_get_debug_messenger_info(void);

#if defined(__GNUC__) || defined(__clang__)
//   https://gcc.gnu.org/onlinedocs/gcc-4.7.2/gcc/Function-Attributes.html
#    ifdef __MINGW_PRINTF_FORMAT
#        define RVK_PRINTF_FORMAT(STRING_INDEX, FIRST_TO_CHECK) __attribute__ ((format (__MINGW_PRINTF_FORMAT, STRING_INDEX, FIRST_TO_CHECK)))
#    else
#        define RVK_PRINTF_FORMAT(STRING_INDEX, FIRST_TO_CHECK) __attribute__ ((format (printf, STRING_INDEX, FIRST_TO_CHECK)))
#    endif // __MINGW_PRINTF_FORMAT
#else
//   TODO: implement RVK_PRINTF_FORMAT for MSVC
#    define RVK_PRINTF_FORMAT(STRING_INDEX, FIRST_TO_CHECK)
#endif

#ifndef RVK_TEMP_CAPACITY
#define RVK_TEMP_CAPACITY (8*1024*1024)
#endif // RVK_TEMP_CAPACITY

char *r_temp_strdup(const char *cstr);
char *r_temp_strndup(const char *s, size_t n);
void *r_temp_alloc(size_t requested_size);
void *r_temp_calloc(size_t requested_size);
char *r_temp_sprintf(const char *format, ...) RVK_PRINTF_FORMAT(1, 2);
void r_temp_reset(void);
size_t r_temp_save(void);
void r_temp_rewind(size_t checkpoint);

bool r_instance_layers_supported(const char **requested_layers, uint32_t requested_layer_count);
bool r_instance_extensions_supported(const char **requested_extensions, uint32_t requested_extension_count);

/* tries to prefer discrete GPUs, set log level to RVK_INFO for more info */
VkPhysicalDevice r_pick_physical_device(VkInstance instance);

/* returns max unt32_t upon error, surface == NULL means we don't care about present support */
uint32_t r_find_queue_family(VkPhysicalDevice physical_device, VkSurfaceKHR surface, VkQueueFlags flags);
void r_log_queue_properties(VkPhysicalDevice physical_device, VkSurfaceKHR surface);

#define RVK_MAX_FRAMES_IN_FLIGHT 2
typedef struct {
    VkPhysicalDevice physical;
    VkDevice logical;
    uint32_t queue_family_index;
    VkQueue queue;
    VkCommandPool command_pool;
    VkSemaphore image_available_sems[RVK_MAX_FRAMES_IN_FLIGHT];
    VkSemaphore render_finished_sems[RVK_MAX_FRAMES_IN_FLIGHT];
    VkFence fences[RVK_MAX_FRAMES_IN_FLIGHT];
    VkCommandBuffer cmd_buffs[RVK_MAX_FRAMES_IN_FLIGHT];
} Rvk_Device;

typedef struct {
    uint32_t extension_count;
    const char **extensions;
    uint32_t layer_count;
    const char **layers;
    bool atomic_features;
} Rvk_Device_Config;

Rvk_Device r_create_rvk_device(VkInstance instance, VkSurfaceKHR surface, Rvk_Device_Config config);
void r_destroy_rvk_device(Rvk_Device device);

VkSurfaceFormatKHR r_choose_swapchain_format(VkPhysicalDevice physical_device, VkSurfaceKHR surface);
uint32_t r_get_suggested_image_count(VkPhysicalDevice physical_device, VkSurfaceKHR surface);
uint32_t r_get_current_transform(VkPhysicalDevice physical_device, VkSurfaceKHR surface);

/* unlikely, but possible to return extent thats not the same as width and height */
VkExtent2D r_suggest_swapchain_extent(VkPhysicalDevice physical_device, VkSurfaceKHR surface, int width, int height);

VkPresentModeKHR r_choose_present_mode(VkPhysicalDevice physical_device, VkSurfaceKHR surface);

/* create a basic render pass with color and depth attachments */
VkRenderPass r_create_render_pass(VkDevice device, VkFormat depth_format, VkFormat color_format);

#define RVK_MAX_SWAPCHAIN_IMAGES 10
typedef struct {
    VkSwapchainKHR handle;
    VkImage images[RVK_MAX_SWAPCHAIN_IMAGES];
    VkImageView image_views[RVK_MAX_SWAPCHAIN_IMAGES];
    VkFramebuffer framebuffers[RVK_MAX_SWAPCHAIN_IMAGES];
    VkSurfaceFormatKHR surface_format; // TODO: probably don't need to store
    VkImage depth_image;
    VkDeviceMemory depth_image_memory;
    VkImageView depth_image_view;
    VkFormat depth_format; // TODO: probably don't need to store
    uint32_t image_count;
    bool resized;
    VkExtent2D extent;
    VkRenderPass render_pass;
} Rvk_Swapchain;

/* create a basic swapchain */
Rvk_Swapchain r_create_rvk_swapchain(Rvk_Device device, VkSurfaceKHR surface, int width, int height);
void r_destroy_rvk_swapchain(VkDevice device, Rvk_Swapchain swapchain);

// TODO: I feel like these should be named r_create_*_shader_stage_ci()
VkPipelineShaderStageCreateInfo r_create_vertex_stage_ci(VkDevice device, size_t code_size, const uint32_t *code);
VkPipelineShaderStageCreateInfo r_create_fragment_stage_ci(VkDevice device, size_t code_size, const uint32_t *code);
VkPipelineShaderStageCreateInfo r_create_compute_shader_stage_ci(VkDevice device, size_t code_size, const uint32_t *code);
VkShaderModule r_create_shader_module(VkDevice device, size_t code_size, const uint32_t *code);

/* for depth ,you might try format = VK_FORMAT_D32_SFLOAT and usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
 * for color you might try format = VK_FORMAT_R8G8B8A8_SRGB and usage =
 * VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT
 * ...only suggestions */
VkImage r_create_2D_image(VkDevice device, VkFormat format, VkImageUsageFlags usage, VkExtent2D extent);

/* returns max unt32_t upon error */
uint32_t r_find_memory_type_index(VkPhysicalDevice physical_device, uint32_t type, VkMemoryPropertyFlags properties);

VkDeviceMemory r_allocate_and_bind_image_memory(Rvk_Device device, VkMemoryPropertyFlags mem_props, VkImage image);
VkDeviceMemory r_allocate_and_bind_buffer_memory(Rvk_Device device, VkMemoryPropertyFlags mem_props, VkBuffer buffer);

bool r_init_framebuffers(VkDevice device, Rvk_Swapchain *swapchain, VkRenderPass render_pass);

/* default state create info initializers for when you are creating pipelines */
VkPipelineDepthStencilStateCreateInfo *r_temp_default_depth_stencil_state_ci(void);
VkPipelineRasterizationStateCreateInfo *r_temp_default_rasterization_state_ci(void);
VkPipelineRasterizationStateCreateInfo *r_temp_default_point_rasterization_state_ci(void);
VkPipelineRasterizationStateCreateInfo *r_temp_default_line_rasterization_state_ci(void);
VkPipelineMultisampleStateCreateInfo *r_temp_default_multisample_state_ci(void);
VkPipelineViewportStateCreateInfo *r_temp_default_viewport_state_ci(VkExtent2D extent);
VkPipelineInputAssemblyStateCreateInfo *r_temp_default_input_assembly_state_ci(void);
VkPipelineInputAssemblyStateCreateInfo *r_temp_default_point_input_assembly_state_ci(void);
VkPipelineInputAssemblyStateCreateInfo *r_temp_default_line_input_assembly_state_ci(void);
VkPipelineVertexInputStateCreateInfo *r_temp_default_primitive_2D_vertex_input_state_ci(void);
VkPipelineVertexInputStateCreateInfo *r_temp_default_primitive_point_input_state_ci(void);
VkPipelineVertexInputStateCreateInfo *r_temp_default_primitive_line_input_state_ci(void);
VkPipelineVertexInputStateCreateInfo *r_temp_default_empty_input_state_ci(void);
VkPipelineColorBlendStateCreateInfo *r_temp_default_color_blend_state_ci(void);
VkPipelineDynamicStateCreateInfo *r_temp_default_dynamic_state_ci(void);

void r_cmd_set_viewport_scissor(VkCommandBuffer cmd_buff, VkExtent2D extent);

typedef struct {
    VkDescriptorBufferInfo info;
    VkDeviceMemory memory;
    void *mapped;
} Rvk_Buffer;

void r_cmd_draw_buffers(VkCommandBuffer cmd_buff, VkBuffer vtx_buff, VkBuffer idx_buff, size_t idx_count);
void r_cmd_draw_point_buffer(VkCommandBuffer cmd_buff, VkBuffer vtx_buff, uint32_t point_count);

/* This begin/end temporary command buffers are for one-off commands like copying,
 * it's not very efficient, but it's convenient for quick temporary commands. Note
 * that r_end_tmp_cmd_buff calls vkQueueuWaitIdle */
VkCommandBuffer r_begin_tmp_cmd_buff(VkCommandPool pool, VkDevice device);
void r_end_tmp_cmd_buff(VkQueue queue, VkDevice device, VkCommandPool pool, VkCommandBuffer tmp_cmd_buff);

Rvk_Buffer r_create_vertex_buffer(Rvk_Device device, size_t size, void *data);
Rvk_Buffer r_create_index_buffer(Rvk_Device device, size_t size, void *data);
Rvk_Buffer r_create_mapped_uniform_buffer(Rvk_Device device, size_t size);
Rvk_Buffer r_create_compute_buffer_from_host(Rvk_Device device, size_t size, void *data);
Rvk_Buffer r_create_compute_buffer(Rvk_Device device, size_t size);
void r_destroy_rvk_buffer(VkDevice device, Rvk_Buffer buffer);
bool r_wait_reset_fence(VkDevice device, VkFence *fence);
bool r_acquire_next_image(VkDevice device, VkSwapchainKHR swapchain, VkSemaphore image_available, uint32_t *image_index);
bool r_reset_begin_cmd_buff(VkCommandBuffer cmd_buff);
bool r_submit(Rvk_Device device, uint32_t current_frame);
bool r_present(VkQueue queue, VkSemaphore render_finished, uint32_t image_index, VkSwapchainKHR swapchain);
void r_cmd_begin_render_pass(VkCommandBuffer cb, VkRenderPass rp, VkFramebuffer fb, VkExtent2D extent, float r, float g, float b, float a);
bool r_transition_image_layout(Rvk_Device device, VkImage image, VkImageLayout old_layout, VkImageLayout new_layout);

/* returns null handle if call fails */
#define RVK_MAX_DESCRIPTOR_SETS 450
VkDescriptorPool r_create_default_descriptor_pool(VkDevice device);

typedef struct {
    VkDescriptorImageInfo info;
    VkDeviceMemory memory;
    VkImage image;
} Rvk_Texture;

Rvk_Texture r_create_texture(Rvk_Device device, VkExtent2D extent, void *data, VkFormat format);
void r_destroy_texture(VkDevice device, Rvk_Texture texture);

/***********************************************************************************
*  vk_* API declarations
************************************************************************************/

#define vk_create_instance(pAllocator, pInstance, ...) vk_create_instance_(pAllocator, pInstance, (VkInstanceCreateInfo){__VA_ARGS__})
bool vk_create_instance_(const VkAllocationCallbacks *pAllocator, VkInstance* pInstance, VkInstanceCreateInfo ci);

#define vk_create_device(physical_device, pAllocator, pDevice, ...) vk_create_device_(physical_device, pAllocator, pDevice, (VkDeviceCreateInfo){__VA_ARGS__})
bool vk_create_device_(VkPhysicalDevice physical_device, const VkAllocationCallbacks *pAllocator, VkDevice *pDevice, VkDeviceCreateInfo ci);

#define vk_create_swapchain_khr(device, pAllocator, pSwapchain, ...) vk_create_swapchain_khr_(device, pAllocator, pSwapchain, (VkSwapchainCreateInfoKHR){__VA_ARGS__})
bool vk_create_swapchain_khr_(VkDevice device, const VkAllocationCallbacks *pAllocator, VkSwapchainKHR *pSwapchain, VkSwapchainCreateInfoKHR ci);

#define vk_create_image_view(device, pAllocator, pView, ...) vk_create_image_view_(device, pAllocator, pView, (VkImageViewCreateInfo){__VA_ARGS__})
bool vk_create_image_view_(VkDevice device, const VkAllocationCallbacks *pAllocator, VkImageView *pView, VkImageViewCreateInfo ci);

#define vk_create_render_pass(device, pAllocator, pRenderPass, ...) vk_create_render_pass_(device, pAllocator, pRenderPass, (VkRenderPassCreateInfo){__VA_ARGS__})
bool vk_create_render_pass_(VkDevice device, const VkAllocationCallbacks *pAllocator, VkRenderPass *pRenderPass, VkRenderPassCreateInfo ci);

#define vk_create_image(device, pAllocator, pImage, ...) vk_create_image_(device, pAllocator, pImage, (VkImageCreateInfo){__VA_ARGS__})
bool vk_create_image_(VkDevice device, const VkAllocationCallbacks *pAllocator, VkImage *pImage, VkImageCreateInfo ci);

#define vk_create_command_pool(device, pAllocator, pCommandPool, ...) vk_create_command_pool_(device, pAllocator, pCommandPool, (VkCommandPoolCreateInfo){__VA_ARGS__})
bool vk_create_command_pool_(VkDevice device, const VkAllocationCallbacks *pAllocator, VkCommandPool *pCommandPool, VkCommandPoolCreateInfo ci);

#define vk_allocate_command_buffers(device, pCommandBuffers, ...) vk_allocate_command_buffers_(device, pCommandBuffers, (VkCommandBufferAllocateInfo){__VA_ARGS__})
bool vk_allocate_command_buffers_(VkDevice device, VkCommandBuffer *pCommandBuffers, VkCommandBufferAllocateInfo ci);

#define vk_create_semaphore(device, pAllocator, pSemaphore, ...) vk_create_semaphore_(device, pAllocator, pSemaphore, (VkSemaphoreCreateInfo){__VA_ARGS__})
bool vk_create_semaphore_(VkDevice device, const VkAllocationCallbacks *pAllocator, VkSemaphore *pSemaphore, VkSemaphoreCreateInfo ci);

#define vk_create_fence(device, pAllocator, pFence, ...) vk_create_fence_(device, pAllocator, pFence, (VkFenceCreateInfo){__VA_ARGS__})
bool vk_create_fence_(VkDevice device, const VkAllocationCallbacks *pAllocator, VkFence *pFence, VkFenceCreateInfo ci);

#define vk_create_pipeline_layout(device, pAllocator, pPipelineLayout, ...) vk_create_pipeline_layout_(device, pAllocator, pPipelineLayout, (VkPipelineLayoutCreateInfo){__VA_ARGS__})
bool vk_create_pipeline_layout_(VkDevice device, const VkAllocationCallbacks *pAllocator, VkPipelineLayout *pPipelineLayout, VkPipelineLayoutCreateInfo ci);

#define vk_create_graphics_pipeline(device, pipelineCache, pAllocator, pPipeline, ...) vk_create_graphics_pipeline_(device, pipelineCache, pAllocator, pPipeline, (VkGraphicsPipelineCreateInfo){__VA_ARGS__})
bool vk_create_graphics_pipeline_(VkDevice device, VkPipelineCache pipelineCache, const VkAllocationCallbacks *pAllocator, VkPipeline *pPipeline, VkGraphicsPipelineCreateInfo ci);

#define vk_create_compute_pipeline(device, pipelineCache, pAllocator, pPipeline, ...) vk_create_compute_pipeline_(device, pipelineCache, pAllocator, pPipeline, (VkComputePipelineCreateInfo){__VA_ARGS__})
bool vk_create_compute_pipeline_(VkDevice device, VkPipelineCache pipelineCache, const VkAllocationCallbacks *pAllocator, VkPipeline *pPipeline, VkComputePipelineCreateInfo ci);

#define vk_create_shader_module(device, pAllocator, pShaderModule, ...) vk_create_shader_module_(device, pAllocator, pShaderModule, (VkShaderModuleCreateInfo){__VA_ARGS__})
bool vk_create_shader_module_(VkDevice device, const VkAllocationCallbacks *pAllocator, VkShaderModule *pShaderModule, VkShaderModuleCreateInfo ci);

#define vk_create_buffer(device, pAllocator, pBuffer, ...) vk_create_buffer_(device, pAllocator, pBuffer, (VkBufferCreateInfo){__VA_ARGS__})
bool vk_create_buffer_(VkDevice device, const VkAllocationCallbacks *pAllocator, VkBuffer *pBuffer, VkBufferCreateInfo ci);

#define vk_create_descriptor_pool(device, pAllocator, pDescriptorPool, ...) vk_create_descriptor_pool_(device, pAllocator, pDescriptorPool, (VkDescriptorPoolCreateInfo ){__VA_ARGS__})
bool vk_create_descriptor_pool_(VkDevice device, const VkAllocationCallbacks *pAllocator, VkDescriptorPool *pDescriptorPool, VkDescriptorPoolCreateInfo ci);

#define vk_create_descriptor_set_layout(device, pAllocator, pSetLayout, ...) vk_create_descriptor_set_layout_(device, pAllocator, pSetLayout, (VkDescriptorSetLayoutCreateInfo){__VA_ARGS__})
bool vk_create_descriptor_set_layout_(VkDevice device, const VkAllocationCallbacks *pAllocator, VkDescriptorSetLayout *pSetLayout, VkDescriptorSetLayoutCreateInfo ci);

#define vk_allocate_descriptor_sets(device, pDescriptorSets, ...) vk_allocate_descriptor_sets_(device, pDescriptorSets, (VkDescriptorSetAllocateInfo){__VA_ARGS__})
bool vk_allocate_descriptor_sets_(VkDevice device, VkDescriptorSet *pDescriptorSets, VkDescriptorSetAllocateInfo ci);

#define vk_create_sampler(device, pAllocator, pSampler, ...) vk_create_sampler_(device, pAllocator, pSampler, (VkSamplerCreateInfo){__VA_ARGS__})
bool vk_create_sampler_(VkDevice device, const VkAllocationCallbacks *pAllocator, VkSampler *pSampler, VkSamplerCreateInfo ci);

#endif // RVK_H_

#ifdef RVK_IMPLEMENTATION

#ifndef APP_NAME
    #define APP_NAME "app"
#endif

#ifndef RVK_LOG_LEVEL
    #define RVK_LOG_LEVEL RVK_INFO
#endif

#define RVK_LOAD_PFN(pfn) PFN_ ## pfn pfn = (PFN_ ## pfn) vkGetInstanceProcAddr(instance, #pfn)
#define RVK_SUCCEEDED(x) ((x) == VK_SUCCESS)
#define CLAMP(val, min, max) ((val) < (min)) ? (min) : (((val) > (max)) ? (max) : (val))
#define RVK_ARRAY_LEN(array) (sizeof(array)/sizeof(array[0]))

static size_t r_temp_size = 0;
static char r_temp[RVK_TEMP_CAPACITY] = {0};

/***********************************************************************************
*  r_* API implementation
************************************************************************************/

bool r_check_vk_result(VkResult result, const char* function)
{
    if (!RVK_SUCCEEDED(result)) {
        r_log(RVK_ERROR, "Vulkan Error: %s : %s", function, r_vk_res_to_str(result));
        return false;
    }
    return true;
}

void r_log(Rvk_Log_Level level, const char *fmt, ...)
{
    if (level < RVK_LOG_LEVEL) return;
#if defined(PLATFORM_ANDROID)
    va_list args;
    va_start(args, fmt);
    switch (level) {
    case RVK_INFO:
         __android_log_vprint(ANDROID_LOG_INFO,  APP_NAME, fmt, args);
        break;
    case RVK_WARNING:
         __android_log_vprint(ANDROID_LOG_WARN,  APP_NAME, fmt, args);
        break;
    case RVK_ERROR:
         __android_log_vprint(ANDROID_LOG_ERROR,  APP_NAME, fmt, args);
        break;
    }
#else
    switch (level) {
    case RVK_INFO:
        fprintf(stderr, "[RVK][INFO] ");
        break;
    case RVK_WARNING:
        fprintf(stderr, "[RVK][WARNING] ");
        break;
    case RVK_ERROR:
        fprintf(stderr, "[RVK][ERROR] ");
        break;
    default:
    }

    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
#endif // end of platform defines
}

const char *r_vk_res_to_str(VkResult res)
{
    /* these aren't all of the results, but I don't feel like dealing with different vulkan versions */
    switch (res) {
    case VK_SUCCESS:                              return "VK_SUCCESS";
    case VK_NOT_READY:                            return "VK_NOT_READY";
    case VK_TIMEOUT:                              return "VK_TIMEOUT";
    case VK_EVENT_SET:                            return "VK_EVENT_SET";
    case VK_EVENT_RESET:                          return "VK_EVENT_RESET";
    case VK_INCOMPLETE:                           return "VK_INCOMPLETE";
    case VK_ERROR_OUT_OF_HOST_MEMORY:             return "VK_ERROR_OUT_OF_HOST_MEMORY";
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:           return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
    case VK_ERROR_INITIALIZATION_FAILED:          return "VK_ERROR_INITIALIZATION_FAILED";
    case VK_ERROR_DEVICE_LOST:                    return "VK_ERROR_DEVICE_LOST";
    case VK_ERROR_MEMORY_MAP_FAILED:              return "VK_ERROR_MEMORY_MAP_FAILED";
    case VK_ERROR_LAYER_NOT_PRESENT:              return "VK_ERROR_LAYER_NOT_PRESENT";
    case VK_ERROR_EXTENSION_NOT_PRESENT:          return "VK_ERROR_EXTENSION_NOT_PRESENT";
    case VK_ERROR_FEATURE_NOT_PRESENT:            return "VK_ERROR_FEATURE_NOT_PRESENT";
    case VK_ERROR_INCOMPATIBLE_DRIVER:            return "VK_ERROR_INCOMPATIBLE_DRIVER";
    case VK_ERROR_TOO_MANY_OBJECTS:               return "VK_ERROR_TOO_MANY_OBJECTS";
    case VK_ERROR_FORMAT_NOT_SUPPORTED:           return "VK_ERROR_FORMAT_NOT_SUPPORTED";
    case VK_ERROR_FRAGMENTED_POOL:                return "VK_ERROR_FRAGMENTED_POOL";
    case VK_ERROR_UNKNOWN:                        return "VK_ERROR_UNKNOWN";
    case VK_ERROR_OUT_OF_POOL_MEMORY:             return "VK_ERROR_OUT_OF_POOL_MEMORY";
    case VK_ERROR_INVALID_EXTERNAL_HANDLE:        return "VK_ERROR_INVALID_EXTERNAL_HANDLE";
    case VK_ERROR_FRAGMENTATION:                  return "VK_ERROR_FRAGMENTATION";
    case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS: return "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS";
    case VK_PIPELINE_COMPILE_REQUIRED:            return "VK_PIPELINE_COMPILE_REQUIRED";
    case VK_ERROR_SURFACE_LOST_KHR:               return "VK_ERROR_SURFACE_LOST_KHR";
    case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:       return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
    case VK_SUBOPTIMAL_KHR:                       return "VK_SUBOPTIMAL_KHR";
    case VK_ERROR_OUT_OF_DATE_KHR:                return "VK_ERROR_OUT_OF_DATE_KHR";
    case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:       return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
    case VK_ERROR_VALIDATION_FAILED_EXT:          return "VK_ERROR_VALIDATION_FAILED_EXT";
    case VK_ERROR_INVALID_SHADER_NV:              return "VK_ERROR_INVALID_SHADER_NV";
    default: return "unrecognized vkresult";
    }
}

bool r_instance_layers_supported(const char **requested_layers, uint32_t requested_layer_count)
{
    uint32_t available_layer_count = 0;
    vkEnumerateInstanceLayerProperties(&available_layer_count, NULL);
    VkLayerProperties available_layers[available_layer_count];
    vkEnumerateInstanceLayerProperties(&available_layer_count, available_layers);

    for (size_t i = 0; i < requested_layer_count; i++) {
        bool found = false;
        for (size_t j = 0; j < available_layer_count; j++) {
            if (strcmp(requested_layers[i], available_layers[j].layerName) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            r_log(RVK_ERROR, "validation layer `%s` not available", requested_layers[i]);
            return false;
        }
    }
    return true;
}

bool r_instance_extensions_supported(const char **requested_extensions, uint32_t requested_extension_count)
{
    uint32_t available_layer_count = 0;
    vkEnumerateInstanceLayerProperties(&available_layer_count, NULL);
    VkLayerProperties available_layers[available_layer_count];
    vkEnumerateInstanceLayerProperties(&available_layer_count, available_layers);

    for (size_t i = 0; i < requested_extension_count; i++) {
        bool found = false;
        for (size_t j = 0; j < available_layer_count; j++) {
            if (strcmp(requested_extensions[i], available_layers[j].layerName) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            r_log(RVK_ERROR, "validation layer `%s` not available", requested_extensions[i]);
            return false;
        }
    }
    return true;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL r_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT msg_severity,
    VkDebugUtilsMessageTypeFlagsEXT msg_type,
    const VkDebugUtilsMessengerCallbackDataEXT* p_callback_data,
    void* p_user_data)
{
    (void)msg_type;
    (void)p_user_data;

    Rvk_Log_Level log_lvl = RVK_INFO;

    switch (msg_severity) {
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
        log_lvl = RVK_VERBOSE;
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
        log_lvl = RVK_VERBOSE; // not a mistake, but their idea of info is incredibly verbose
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
        log_lvl = RVK_WARNING;
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
        log_lvl = RVK_ERROR;
        break;
    default: return VK_FALSE;
    }

    if (log_lvl < RVK_LOG_LEVEL) return VK_FALSE;

    r_log(log_lvl, "%s", p_callback_data->pMessage);

    return VK_FALSE;
}

VkDebugUtilsMessengerCreateInfoEXT r_get_debug_messenger_info(void)
{
    return (VkDebugUtilsMessengerCreateInfoEXT) {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = 0x1110, // error, warning, info
        .messageType = 0x7, // general, validation, performance
        .pfnUserCallback = r_debug_callback,
    };
}

VkPhysicalDevice r_pick_physical_device(VkInstance instance)
{
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(instance, &device_count, NULL);
    VkPhysicalDevice devices[device_count];
    vkEnumeratePhysicalDevices(instance, &device_count, devices);
    VkPhysicalDeviceType rankings[] = {
        VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU,
        VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU,
        VK_PHYSICAL_DEVICE_TYPE_CPU,
    };
    for (size_t i = 0; i < RVK_ARRAY_LEN(rankings); i++) {
        for (size_t j = 0; j < device_count; j++) {
            VkPhysicalDeviceProperties props = {0};
            vkGetPhysicalDeviceProperties(devices[j], &props);
            if (props.deviceType == rankings[i]) {
                char *device_type = NULL;
                switch (props.deviceType) {
                case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: device_type = "Discrete"; break;
                case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: device_type = "Integrated"; break;
                case VK_PHYSICAL_DEVICE_TYPE_CPU: device_type = "CPU"; break;
                default: device_type = "N/A";
                }
                r_log(RVK_INFO, "Selected %s-GPU: %s", device_type, props.deviceName);
                return devices[i];
            }
        }
    }

    return VK_NULL_HANDLE;
}

uint32_t r_find_queue_family(VkPhysicalDevice physical_device, VkSurfaceKHR surface, VkQueueFlags flags)
{
    uint32_t queue_fam_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_fam_count, NULL);
    VkQueueFamilyProperties queue_fam_props[queue_fam_count];
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_fam_count, queue_fam_props);
    for (uint32_t i = 0; i < queue_fam_count; i++) {
        bool flag_check = (queue_fam_props[i].queueFlags & flags) == flags;
        if (surface) {
            VkBool32 present_support = true;
            vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, surface, &present_support);
            if (flag_check && present_support) return i;
        } else {
            if (flag_check) return i;
        }
    }

    return -1;
}

void r_log_queue_properties(VkPhysicalDevice physical_device, VkSurfaceKHR surface)
{
    uint32_t queue_fam_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_fam_count, NULL);
    VkQueueFamilyProperties queue_fam_props[queue_fam_count];
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_fam_count, queue_fam_props);
    for (uint32_t i = 0; i < queue_fam_count; i++) {
        if (surface) {
            VkBool32 present_support = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, surface, &present_support);
            r_log(RVK_INFO, "queue %u, present support: %s", i, (present_support) ? "true" : "false");
        } else {
            r_log(RVK_INFO, "queue %u", i);
        }

        VkQueueFlags flags = queue_fam_props[i].queueFlags;
        if (flags & VK_QUEUE_GRAPHICS_BIT        ) r_log(RVK_INFO, "    VK_QUEUE_GRAPHICS_BIT");
        if (flags & VK_QUEUE_COMPUTE_BIT         ) r_log(RVK_INFO, "    VK_QUEUE_COMPUTE_BIT");
        if (flags & VK_QUEUE_TRANSFER_BIT        ) r_log(RVK_INFO, "    VK_QUEUE_TRANSFER_BIT");
        if (flags & VK_QUEUE_SPARSE_BINDING_BIT  ) r_log(RVK_INFO, "    VK_QUEUE_SPARSE_BINDING_BIT");
        if (flags & VK_QUEUE_PROTECTED_BIT       ) r_log(RVK_INFO, "    VK_QUEUE_PROTECTED_BIT");
        if (flags & VK_QUEUE_VIDEO_DECODE_BIT_KHR) r_log(RVK_INFO, "    VK_QUEUE_VIDEO_DECODE_BIT_KHR");
        if (flags & VK_QUEUE_VIDEO_ENCODE_BIT_KHR) r_log(RVK_INFO, "    VK_QUEUE_VIDEO_ENCODE_BIT_KHR");
        if (flags & VK_QUEUE_OPTICAL_FLOW_BIT_NV ) r_log(RVK_INFO, "    VK_QUEUE_OPTICAL_FLOW_BIT_NV");
    }
}

Rvk_Device r_create_rvk_device(VkInstance instance, VkSurfaceKHR surface, Rvk_Device_Config config)
{
    /* pick physical device (tries to prefer discrete GPU) */
    Rvk_Device device = { .physical = VK_NULL_HANDLE, .logical = VK_NULL_HANDLE };
    if (!(device.physical = r_pick_physical_device(instance))) return (Rvk_Device){0};

    /* find a queue family with graphics & present support.
     * if we don't care about present support set surface = NULL.
     * if we want a queue family with compute and graphics set flags e.g.:
     *     VK_QUEUE_GRAPHICS_BIT|VK_QUEUE_COMPUTE_BIT */
    device.queue_family_index = r_find_queue_family(device.physical, surface, VK_QUEUE_GRAPHICS_BIT);
    if (device.queue_family_index == (uint32_t)-1) {
        r_log(RVK_ERROR, "failed to find sufficient queue family");
        return (Rvk_Device){0};
    }

    /* create a device with the queue family index and physical device we picked */
    float priority = 1.0f;
    VkDeviceQueueCreateInfo queue_ci = {
        .queueFamilyIndex = device.queue_family_index,
        .queueCount = 1,
        .pQueuePriorities = &priority,
    };

    VkPhysicalDeviceFeatures *default_features = NULL;
    VkPhysicalDeviceFeatures2 *extended_features = NULL;

    /* default features */
    VkPhysicalDeviceFeatures features = {
        .samplerAnisotropy = VK_TRUE,
        .fillModeNonSolid = VK_TRUE,
    };
    default_features = &features;
    VkPhysicalDeviceVulkan13Features vulkan_1_3_features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
    };
    VkPhysicalDeviceVulkan12Features vulkan_1_2_features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
    };
    VkPhysicalDeviceFeatures2 features2 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
    };

    if (config.atomic_features) {
        features.shaderInt64 = VK_TRUE;
        vulkan_1_3_features.synchronization2 = VK_TRUE;
        vulkan_1_2_features.shaderBufferInt64Atomics = VK_TRUE;
        vulkan_1_2_features.pNext = &vulkan_1_3_features,
        features2.features = features;
        features2.pNext = &vulkan_1_2_features;
        extended_features = &features2;
        default_features = NULL;
    }

    if (!vk_create_device(device.physical, NULL, &device.logical,
                          .pNext = extended_features,
                          .pQueueCreateInfos = &queue_ci,
                          .queueCreateInfoCount = 1,
                          .enabledExtensionCount = config.extension_count,
                          .ppEnabledExtensionNames = config.extensions,
                          .ppEnabledLayerNames = config.layers,
                          .enabledLayerCount = config.layer_count,
                          .pEnabledFeatures = default_features)) return (Rvk_Device){0};

    /* acquire the queue */
    vkGetDeviceQueue(device.logical, device.queue_family_index, 0, &device.queue);
    RVK_ASSERT(device.queue);
    RVK_ASSERT(vk_create_command_pool(device.logical, NULL, &device.command_pool,
                                      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                                      .queueFamilyIndex = device.queue_family_index));

    RVK_ASSERT(vk_allocate_command_buffers(device.logical, device.cmd_buffs,
                                           .commandPool = device.command_pool,
                                           .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                                           .commandBufferCount = RVK_MAX_FRAMES_IN_FLIGHT));

    for (size_t i = 0; i < RVK_MAX_FRAMES_IN_FLIGHT; i++) {
        RVK_ASSERT(vk_create_semaphore(device.logical, NULL, &device.image_available_sems[i]));
        RVK_ASSERT(vk_create_semaphore(device.logical, NULL, &device.render_finished_sems[i]));
        RVK_ASSERT(vk_create_fence(device.logical, NULL, &device.fences[i], .flags = VK_FENCE_CREATE_SIGNALED_BIT));
    }

    return device;
}

void r_destroy_rvk_device(Rvk_Device device)
{
    for (size_t i = 0; i < RVK_MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroyFence(device.logical, device.fences[i], NULL);
        vkDestroySemaphore(device.logical, device.image_available_sems[i], NULL);
        vkDestroySemaphore(device.logical, device.render_finished_sems[i], NULL);
    }
    vkFreeCommandBuffers(device.logical, device.command_pool, RVK_MAX_FRAMES_IN_FLIGHT, device.cmd_buffs);
    vkDestroyCommandPool(device.logical, device.command_pool, NULL);
    vkDestroyDevice(device.logical, NULL);
}

VkSurfaceFormatKHR r_choose_swapchain_surface_format(VkPhysicalDevice physical_device, VkSurfaceKHR surface)
{
    uint32_t surface_fmt_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &surface_fmt_count, NULL);
    VkSurfaceFormatKHR fmts[surface_fmt_count];
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &surface_fmt_count, fmts);
    for (size_t i = 0; i < surface_fmt_count; i++) {
        if (fmts[i].format == VK_FORMAT_B8G8R8A8_SRGB && fmts[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return fmts[i];
        }
    }

    return fmts[0];
}

VkExtent2D r_suggest_swapchain_extent(VkPhysicalDevice physical_device, VkSurfaceKHR surface, int width, int height)
{
    VkSurfaceCapabilitiesKHR capabilities = {0};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &capabilities);
    if (capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent;
    } else {
        return (VkExtent2D) {
            .width  = CLAMP((uint32_t)width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
            .height = CLAMP((uint32_t)height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height),
        };
    }
}

uint32_t r_get_suggested_image_count(VkPhysicalDevice physical_device, VkSurfaceKHR surface)
{
    VkSurfaceCapabilitiesKHR capabilities = {0};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &capabilities);
    uint32_t img_count = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && img_count > capabilities.minImageCount)
        img_count = capabilities.maxImageCount;

    return img_count;
}

uint32_t r_get_current_transform(VkPhysicalDevice physical_device, VkSurfaceKHR surface)
{
    VkSurfaceCapabilitiesKHR capabilities = {0};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &capabilities);
    return capabilities.currentTransform;
}

VkPresentModeKHR r_choose_present_mode(VkPhysicalDevice physical_device, VkSurfaceKHR surface)
{
    uint32_t present_mode_count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, NULL);
    VkPresentModeKHR present_modes[present_mode_count];
    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, present_modes);
    for (size_t i = 0; i < present_mode_count; i++) {
        if (present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
            return present_modes[i];
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

VkRenderPass r_create_render_pass(VkDevice device, VkFormat depth_format, VkFormat color_format)
{
    VkRenderPass render_pass = VK_NULL_HANDLE;

    VkAttachmentDescription color = {
        .format = color_format,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    };
    VkAttachmentReference color_ref = {.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkAttachmentDescription depth = {
        .format = depth_format,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    };
    VkAttachmentReference depth_ref = {
        .attachment = 1,
        .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    };
    VkSubpassDescription subpass = {
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_ref,
        .pDepthStencilAttachment = &depth_ref,
    };
    VkAttachmentDescription attachments[] = {color, depth};
    VkSubpassDependency dependency = {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        .srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
    };

    if (!vk_create_render_pass(device, NULL, &render_pass,
                               .attachmentCount = RVK_ARRAY_LEN(attachments),
                               .pAttachments = attachments,
                               .subpassCount = 1,
                               .pSubpasses = &subpass,
                               .dependencyCount = 1,
                               .pDependencies = &dependency)) return VK_NULL_HANDLE;
    return render_pass;
}

Rvk_Swapchain r_create_rvk_swapchain(Rvk_Device device, VkSurfaceKHR surface, int width, int height)
{
    Rvk_Swapchain swapchain = {
        .surface_format = r_choose_swapchain_surface_format(device.physical, surface),
        .extent         = r_suggest_swapchain_extent(device.physical, surface, width, height),
        .image_count    = r_get_suggested_image_count(device.physical, surface),
    };

    if (!vk_create_swapchain_khr(device.logical, NULL, &swapchain.handle,
                                 .surface = surface,
                                 .minImageCount = swapchain.image_count, // only a suggestion not guaranteed
                                 .imageFormat = swapchain.surface_format.format,
                                 .imageColorSpace = swapchain.surface_format.colorSpace,
                                 .imageExtent = swapchain.extent,
                                 .imageArrayLayers = 1,
                                 .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                                 .clipped = VK_TRUE,
                                 .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
                                 .presentMode = r_choose_present_mode(device.physical, surface),
                                 .preTransform = r_get_current_transform(device.physical, surface),
                                 .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE)) return (Rvk_Swapchain){0};

    /* query ACTUAL image count */
    RVK_ASSERT(RVK(vkGetSwapchainImagesKHR(device.logical, swapchain.handle, &swapchain.image_count, NULL)));
    RVK_ASSERT(swapchain.image_count <= RVK_MAX_SWAPCHAIN_IMAGES);
    RVK_ASSERT(RVK(vkGetSwapchainImagesKHR(device.logical, swapchain.handle, &swapchain.image_count,
                                           swapchain.images)));

    /* create the image views for the swapchain */
    for (size_t i = 0; i < swapchain.image_count; i++) {
        RVK_ASSERT(vk_create_image_view(device.logical, NULL, &swapchain.image_views[i],
                                        .image = swapchain.images[i],
                                        .viewType = VK_IMAGE_VIEW_TYPE_2D,
                                        .format = swapchain.surface_format.format,
                                        .subresourceRange = {
                                            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                            .levelCount = 1,
                                            .layerCount = 1,
                                        }));
    }

    /* create depth image */
    swapchain.depth_format = VK_FORMAT_D32_SFLOAT; // TODO: create r_choose_depth_format

    RVK_ASSERT((swapchain.depth_image = r_create_2D_image(device.logical, swapchain.depth_format,
                                                          VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                                                          swapchain.extent)));
    RVK_ASSERT((swapchain.depth_image_memory = r_allocate_and_bind_image_memory(device,
                                                                          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                                                          swapchain.depth_image)));
    RVK_ASSERT(vk_create_image_view(device.logical, NULL, &swapchain.depth_image_view,
                                    .image = swapchain.depth_image,
                                    .viewType = VK_IMAGE_VIEW_TYPE_2D,
                                    .format = swapchain.depth_format,
                                    .subresourceRange = {
                                        .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                                        .levelCount = 1, .layerCount = 1,
                                    }));

    RVK_ASSERT((swapchain.render_pass = r_create_render_pass(device.logical,
                                                             swapchain.depth_format,
                                                             swapchain.surface_format.format)));
    RVK_ASSERT(r_init_framebuffers(device.logical, &swapchain, swapchain.render_pass));

    return swapchain;
}

void r_destroy_rvk_swapchain(VkDevice device, Rvk_Swapchain swapchain)
{
    for (size_t i = 0; i < swapchain.image_count; i++)
        vkDestroyFramebuffer(device, swapchain.framebuffers[i], NULL);
    vkDestroyImageView(device, swapchain.depth_image_view, NULL);
    vkDestroyImage(device, swapchain.depth_image, NULL);
    vkFreeMemory(device, swapchain.depth_image_memory, NULL);
    vkDestroyRenderPass(device, swapchain.render_pass, NULL);
    for (size_t i = 0; i < swapchain.image_count; i++)
        vkDestroyImageView(device, swapchain.image_views[i], NULL);
    vkDestroySwapchainKHR(device, swapchain.handle, NULL);
}

VkPipelineShaderStageCreateInfo r_create_vertex_stage_ci(VkDevice device, size_t code_size, const uint32_t *code)
{
    return (VkPipelineShaderStageCreateInfo) {
        .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage  = VK_SHADER_STAGE_VERTEX_BIT,
        .pName  = "main",
        .module = r_create_shader_module(device, code_size, code),
    };
}

VkPipelineShaderStageCreateInfo r_create_fragment_stage_ci(VkDevice device, size_t code_size, const uint32_t *code)
{
    return (VkPipelineShaderStageCreateInfo) {
        .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage  = VK_SHADER_STAGE_FRAGMENT_BIT,
        .pName  = "main",
        .module = r_create_shader_module(device, code_size, code),
    };
}

VkPipelineShaderStageCreateInfo r_create_compute_shader_stage_ci(VkDevice device, size_t code_size, const uint32_t *code)
{
    return (VkPipelineShaderStageCreateInfo) {
        .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage  = VK_SHADER_STAGE_COMPUTE_BIT,
        .pName  = "main",
        .module = r_create_shader_module(device, code_size, code),
    };
}

VkShaderModule r_create_shader_module(VkDevice device, size_t code_size, const uint32_t *code)
{
    VkShaderModule module = VK_NULL_HANDLE;
    vk_create_shader_module(device, NULL, &module, .codeSize = code_size, .pCode = code);
    return module;
}

VkImage r_create_2D_image(VkDevice device, VkFormat format, VkImageUsageFlags usage, VkExtent2D extent)
{ // TODO: since this is the simple "r_*" function, I feel like it should also allocate and bind_image_memory
    VkImage image = VK_NULL_HANDLE;
    if (!vk_create_image(device, NULL, &image,
                         .imageType = VK_IMAGE_TYPE_2D,
                         .format = format,
                         .extent = {extent.width, extent.height, 1},
                         .mipLevels = 1,
                         .arrayLayers = 1,
                         .samples = VK_SAMPLE_COUNT_1_BIT,
                         .tiling = VK_IMAGE_TILING_OPTIMAL,
                         .usage = usage,
                         .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                         .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED)) return VK_NULL_HANDLE;
    return image;
}

uint32_t r_find_memory_type_index(VkPhysicalDevice physical_device, uint32_t type, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties mem_properites = {0};
    vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_properites);
    for (uint32_t i = 0; i < mem_properites.memoryTypeCount; i++) {
        if (type & (1 << i) && (mem_properites.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    return -1;
}

VkDeviceMemory r_allocate_and_bind_image_memory(Rvk_Device device, VkMemoryPropertyFlags mem_props, VkImage image)
{
    VkDeviceMemory memory = VK_NULL_HANDLE;

    VkMemoryRequirements mem_reqs = {0};
    vkGetImageMemoryRequirements(device.logical, image, &mem_reqs);
    VkMemoryAllocateInfo alloc_ci = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_reqs.size,
    };
    alloc_ci.memoryTypeIndex = r_find_memory_type_index(
        device.physical,
        mem_reqs.memoryTypeBits,
        mem_props
    );
    if (alloc_ci.memoryTypeIndex == (uint32_t)-1) {
        r_log(RVK_ERROR, "memory not suitable based on requirements");
        return VK_NULL_HANDLE;
    }
    if (!RVK(vkAllocateMemory(device.logical, &alloc_ci, NULL, &memory))) return VK_NULL_HANDLE;
    if (!RVK(vkBindImageMemory(device.logical, image, memory, 0))) return VK_NULL_HANDLE;

    return memory;
}

VkDeviceMemory r_allocate_and_bind_buffer_memory(Rvk_Device device, VkMemoryPropertyFlags mem_props, VkBuffer buffer)
{
    VkDeviceMemory memory = VK_NULL_HANDLE;

    VkMemoryRequirements mem_reqs = {0};
    vkGetBufferMemoryRequirements(device.logical, buffer, &mem_reqs);
    VkMemoryAllocateInfo alloc_ci = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_reqs.size,
    };
    alloc_ci.memoryTypeIndex = r_find_memory_type_index(
        device.physical,
        mem_reqs.memoryTypeBits,
        mem_props
    );
    if (alloc_ci.memoryTypeIndex == (uint32_t)-1) {
        r_log(RVK_ERROR, "memory not suitable based on requirements");
        return VK_NULL_HANDLE;
    }
    if (!RVK(vkAllocateMemory(device.logical, &alloc_ci, NULL, &memory))) return VK_NULL_HANDLE;
    if (!RVK(vkBindBufferMemory(device.logical, buffer, memory, 0))) return VK_NULL_HANDLE;

    return memory;
}

bool r_init_framebuffers(VkDevice device, Rvk_Swapchain *swapchain, VkRenderPass render_pass)
{
    bool result = true;

    for (size_t i = 0; i < swapchain->image_count; i++) {
        VkImageView attachments[] = {swapchain->image_views[i], swapchain->depth_image_view};
        VkFramebufferCreateInfo framebuffer_ci = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = render_pass,
            .attachmentCount = RVK_ARRAY_LEN(attachments),
            .pAttachments = attachments,
            .width  =  swapchain->extent.width,
            .height = swapchain->extent.height,
            .layers = 1,
        };
        result = RVK(vkCreateFramebuffer(device, &framebuffer_ci, NULL, &swapchain->framebuffers[i]));
        if (!result) return false;
    }

    return result;
}

VkPipelineDepthStencilStateCreateInfo *r_temp_default_depth_stencil_state_ci(void)
{
    VkPipelineDepthStencilStateCreateInfo *ci = r_temp_calloc(sizeof(*ci));
    *ci = (VkPipelineDepthStencilStateCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = VK_TRUE,
        .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
        .maxDepthBounds = 1.0f,
    };
    return ci;
}

VkPipelineRasterizationStateCreateInfo *r_temp_default_rasterization_state_ci(void)
{
    VkPipelineRasterizationStateCreateInfo *ci = r_temp_calloc(sizeof(*ci));
    *ci = (VkPipelineRasterizationStateCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .lineWidth = 1.0f,
        .cullMode = VK_CULL_MODE_NONE,
    };
    return ci;
}

VkPipelineRasterizationStateCreateInfo *r_temp_default_point_rasterization_state_ci(void)
{
    VkPipelineRasterizationStateCreateInfo *ci = r_temp_calloc(sizeof(*ci));
    *ci = (VkPipelineRasterizationStateCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_POINT,
        .lineWidth = 1.0f,
    };
    return ci;
}

VkPipelineRasterizationStateCreateInfo *r_temp_default_line_rasterization_state_ci(void)
{
    VkPipelineRasterizationStateCreateInfo *ci = r_temp_calloc(sizeof(*ci));
    *ci = (VkPipelineRasterizationStateCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_LINE,
        .lineWidth = 1.0f,
    };
    return ci;
}

VkPipelineMultisampleStateCreateInfo *r_temp_default_multisample_state_ci(void)
{
    VkPipelineMultisampleStateCreateInfo *ci = r_temp_calloc(sizeof(*ci));
    *ci = (VkPipelineMultisampleStateCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };
    return ci;
}


VkPipelineViewportStateCreateInfo *r_temp_default_viewport_state_ci(VkExtent2D extent)
{
    VkViewport *default_viewport_state = r_temp_calloc(sizeof(*default_viewport_state));
    VkRect2D *default_scissor_state    = r_temp_calloc(sizeof(*default_scissor_state));

    default_viewport_state->width    = extent.width;
    default_viewport_state->height   = extent.height;
    default_viewport_state->maxDepth = 1.0f;
    default_scissor_state->extent    = extent;

    VkPipelineViewportStateCreateInfo *ci = r_temp_calloc(sizeof(*ci));
    *ci = (VkPipelineViewportStateCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports = default_viewport_state,
        .scissorCount = 1,
        .pScissors = default_scissor_state,
    };
    return ci;
}

VkPipelineInputAssemblyStateCreateInfo *r_temp_default_input_assembly_state_ci(void)
{
    VkPipelineInputAssemblyStateCreateInfo *ci = r_temp_calloc(sizeof(*ci));
    *ci = (VkPipelineInputAssemblyStateCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
    };
    return ci;
}

VkPipelineInputAssemblyStateCreateInfo *r_temp_default_point_input_assembly_state_ci(void)
{
    VkPipelineInputAssemblyStateCreateInfo *ci = r_temp_calloc(sizeof(*ci));
    *ci = (VkPipelineInputAssemblyStateCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST
    };
    return ci;
}

VkPipelineInputAssemblyStateCreateInfo *r_temp_default_line_input_assembly_state_ci(void)
{
    VkPipelineInputAssemblyStateCreateInfo *ci = r_temp_calloc(sizeof(*ci));
    *ci = (VkPipelineInputAssemblyStateCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST
    };
    return ci;
}

VkPipelineVertexInputStateCreateInfo *r_temp_default_primitive_2D_vertex_input_state_ci(void)
{
    VkVertexInputBindingDescription *input_binding_desc = r_temp_calloc(sizeof(*input_binding_desc));
    *input_binding_desc = (VkVertexInputBindingDescription) {
        .binding   = 0,
        .stride    = sizeof(Rvk_Primitive_2D_Vertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };
    VkVertexInputAttributeDescription *vert_attrs = r_temp_calloc(2*sizeof(*vert_attrs));
    vert_attrs[0].location = 0;
    vert_attrs[0].format = VK_FORMAT_R32G32_SFLOAT;
    vert_attrs[0].offset = offsetof(Rvk_Primitive_2D_Vertex, position);
    vert_attrs[1].location = 1;
    vert_attrs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    vert_attrs[1].offset = offsetof(Rvk_Primitive_2D_Vertex, color);

    VkPipelineVertexInputStateCreateInfo *ci = r_temp_calloc(sizeof(*ci));
    *ci = (VkPipelineVertexInputStateCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = input_binding_desc,
        .vertexAttributeDescriptionCount = 2,
        .pVertexAttributeDescriptions = vert_attrs,
    };
    return ci;
}

VkPipelineVertexInputStateCreateInfo *r_temp_default_primitive_point_input_state_ci(void)
{
    VkVertexInputBindingDescription *input_binding_desc = r_temp_calloc(sizeof(*input_binding_desc));
    *input_binding_desc = (VkVertexInputBindingDescription) {
        .binding   = 0,
        .stride    = sizeof(Rvk_Primitive_Point),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };
    VkVertexInputAttributeDescription *vert_attrs = r_temp_calloc(2*sizeof(*vert_attrs));
    vert_attrs[0].location = 0;
    vert_attrs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    vert_attrs[0].offset = offsetof(Rvk_Primitive_Point, position);
    vert_attrs[1].location = 1;
    vert_attrs[1].format = VK_FORMAT_R8G8B8A8_UINT;
    vert_attrs[1].offset = offsetof(Rvk_Primitive_Point, color);

    VkPipelineVertexInputStateCreateInfo *ci = r_temp_calloc(sizeof(*ci));
    *ci = (VkPipelineVertexInputStateCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = input_binding_desc,
        .vertexAttributeDescriptionCount = 2,
        .pVertexAttributeDescriptions = vert_attrs,
    };
    return ci;
}

VkPipelineVertexInputStateCreateInfo *r_temp_default_primitive_line_input_state_ci(void)
{
    VkVertexInputBindingDescription *input_binding_desc = r_temp_calloc(sizeof(*input_binding_desc));
    *input_binding_desc = (VkVertexInputBindingDescription) {
        .binding   = 0,
        .stride    = sizeof(Rvk_Primitive_Point),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };
    VkVertexInputAttributeDescription *vert_attrs = r_temp_calloc(2*sizeof(*vert_attrs));
    vert_attrs[0].location = 0;
    vert_attrs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    vert_attrs[0].offset = offsetof(Rvk_Line_Vertex, position);
    vert_attrs[1].location = 1;
    vert_attrs[1].format = VK_FORMAT_R8G8B8A8_UINT;
    vert_attrs[1].offset = offsetof(Rvk_Line_Vertex, color);

    VkPipelineVertexInputStateCreateInfo *ci = r_temp_calloc(sizeof(*ci));
    *ci = (VkPipelineVertexInputStateCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = input_binding_desc,
        .vertexAttributeDescriptionCount = 2,
        .pVertexAttributeDescriptions = vert_attrs,
    };
    return ci;
}

VkPipelineVertexInputStateCreateInfo *r_temp_default_empty_input_state_ci(void)
{
    VkPipelineVertexInputStateCreateInfo *ci = r_temp_calloc(sizeof(*ci));
    *ci = (VkPipelineVertexInputStateCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    };
    return ci;
}

VkPipelineColorBlendStateCreateInfo *r_temp_default_color_blend_state_ci(void)
{
    VkPipelineColorBlendAttachmentState *attachment = r_temp_calloc(sizeof(*attachment));
    *attachment = (VkPipelineColorBlendAttachmentState) {
        .colorWriteMask = 0xf, // rgba
        .blendEnable = VK_FALSE,
    };

    VkPipelineColorBlendStateCreateInfo *color_blend = r_temp_calloc(sizeof(*color_blend));
    *color_blend = (VkPipelineColorBlendStateCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = attachment,
        .logicOp = VK_LOGIC_OP_COPY,
    };
    return color_blend;
}

VkPipelineDynamicStateCreateInfo *r_temp_default_dynamic_state_ci(void)
{
    VkDynamicState *dynamic_states = r_temp_calloc(2*sizeof(*dynamic_states));
    dynamic_states[0] = VK_DYNAMIC_STATE_VIEWPORT;
    dynamic_states[1] = VK_DYNAMIC_STATE_SCISSOR;
    VkPipelineDynamicStateCreateInfo *ci = r_temp_calloc(sizeof(*ci));
    *ci = (VkPipelineDynamicStateCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2,
        .pDynamicStates = dynamic_states,
    };
    return ci;
}

void r_cmd_set_viewport_scissor(VkCommandBuffer cmd_buff, VkExtent2D extent)
{
    VkViewport viewport = {
        .width    = extent.width,
        .height   = extent.height,
        .maxDepth = 1.0f,
    };
    vkCmdSetViewport(cmd_buff, 0, 1, &viewport);
    VkRect2D scissor = {.extent = extent};
    vkCmdSetScissor(cmd_buff, 0, 1, &scissor);
}

void r_cmd_draw_buffers(VkCommandBuffer cmd_buff, VkBuffer vtx_buff, VkBuffer idx_buff, size_t idx_count)
{
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd_buff, 0, 1, &vtx_buff, offsets);
    vkCmdBindIndexBuffer(cmd_buff, idx_buff, 0, VK_INDEX_TYPE_UINT16);
    vkCmdDrawIndexed(cmd_buff, idx_count, 1, 0, 0, 0);
}

void r_cmd_draw_point_buffer(VkCommandBuffer cmd_buff, VkBuffer vtx_buff, uint32_t point_count)
{
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd_buff, 0, 1, &vtx_buff, offsets);
    vkCmdDraw(cmd_buff, point_count, 1, 0, 0);
}

VkCommandBuffer r_begin_tmp_cmd_buff(VkCommandPool pool, VkDevice device)
{
    VkCommandBuffer cmd_buff = VK_NULL_HANDLE;
    VkCommandBufferAllocateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandPool = pool,
        .commandBufferCount = 1,
    };
    if (!RVK(vkAllocateCommandBuffers(device, &ci, &cmd_buff))) return VK_NULL_HANDLE;
    VkCommandBufferBeginInfo cmd_begin = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    if (!RVK(vkBeginCommandBuffer(cmd_buff, &cmd_begin))) return VK_NULL_HANDLE;
    return cmd_buff;
}

void r_end_tmp_cmd_buff(VkQueue queue, VkDevice device, VkCommandPool pool, VkCommandBuffer tmp_cmd_buff)
{
    if (!RVK(vkEndCommandBuffer(tmp_cmd_buff))) goto defer;
    VkSubmitInfo submit = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &tmp_cmd_buff,
    };
    if (!RVK(vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE))) goto defer;
    if (!RVK(vkQueueWaitIdle(queue))) goto defer;

defer:
    vkFreeCommandBuffers(device, pool, 1, &tmp_cmd_buff);
}

Rvk_Buffer r_create_vertex_buffer(Rvk_Device device, size_t size, void *data)
{
    if (!device.physical) return (Rvk_Buffer){0};
    if (!device.logical) return (Rvk_Buffer){0};
    if (!device.command_pool) return (Rvk_Buffer){0};

    /* book keeping */
    Rvk_Buffer buff = {.info.range = size};

    /* create a buffer */
    if (!vk_create_buffer(device.logical, NULL, &buff.info.buffer,
                          .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT|VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                          .size = size)) return (Rvk_Buffer){0};
    RVK_ASSERT((buff.memory = r_allocate_and_bind_buffer_memory(device,
                                                                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                                                buff.info.buffer)));

    /* create a staging buffer */
    Rvk_Buffer stg_buff = {0};
    RVK_ASSERT(vk_create_buffer(device.logical, NULL, &stg_buff.info.buffer,
                                .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                .size = size));
    RVK_ASSERT((stg_buff.memory = r_allocate_and_bind_buffer_memory(device,
                                                                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|
                                                                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                                                    stg_buff.info.buffer)));
    /* copy data to staging buffer */
    RVK_ASSERT(RVK(vkMapMemory(device.logical, stg_buff.memory, 0, size, 0, &stg_buff.mapped)));
    memcpy(stg_buff.mapped, data, size);
    vkUnmapMemory(device.logical, stg_buff.memory);

    /* transfer staging buffer to vertex buffer */
    VkCommandBuffer tmp_cmd_buff = r_begin_tmp_cmd_buff(device.command_pool, device.logical);
        VkBufferCopy copy_region = {.size = size};
        vkCmdCopyBuffer(tmp_cmd_buff, stg_buff.info.buffer, buff.info.buffer, 1, &copy_region);
    r_end_tmp_cmd_buff(device.queue, device.logical, device.command_pool, tmp_cmd_buff);

    /* destroy the staging buffer */
    vkDestroyBuffer(device.logical, stg_buff.info.buffer, NULL);
    vkFreeMemory(device.logical, stg_buff.memory, NULL);

    return buff;
}

Rvk_Buffer r_create_index_buffer(Rvk_Device device, size_t size, void *data)
{

    /* book keeping */
    Rvk_Buffer buff = {.info.range = size};

    /* create a buffer */
    if (!vk_create_buffer(device.logical, NULL, &buff.info.buffer,
                          .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT|VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                          .size = size)) return (Rvk_Buffer){0};
    RVK_ASSERT((buff.memory = r_allocate_and_bind_buffer_memory(device,
                                                          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                                          buff.info.buffer)));

    /* create a staging buffer */
    Rvk_Buffer stg_buff = {0};
    RVK_ASSERT(vk_create_buffer(device.logical, NULL, &stg_buff.info.buffer,
                          .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                          .size = size));
    RVK_ASSERT((stg_buff.memory = r_allocate_and_bind_buffer_memory(device,
                                                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|
                                                              VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                                              stg_buff.info.buffer)));
    /* copy data to staging buffer */
    RVK_ASSERT(RVK(vkMapMemory(device.logical, stg_buff.memory, 0, size, 0, &stg_buff.mapped)));
    memcpy(stg_buff.mapped, data, size);
    vkUnmapMemory(device.logical, stg_buff.memory);

    /* transfer staging buffer to vertex buffer */
    VkCommandBuffer tmp_cmd_buff = r_begin_tmp_cmd_buff(device.command_pool, device.logical);
        VkBufferCopy copy_region = {.size = size};
        vkCmdCopyBuffer(tmp_cmd_buff, stg_buff.info.buffer, buff.info.buffer, 1, &copy_region);
    r_end_tmp_cmd_buff(device.queue, device.logical, device.command_pool, tmp_cmd_buff);

    /* destroy the staging buffer */
    vkDestroyBuffer(device.logical, stg_buff.info.buffer, NULL);
    vkFreeMemory(device.logical, stg_buff.memory, NULL);

    return buff;
}

Rvk_Buffer r_create_mapped_uniform_buffer(Rvk_Device device, size_t size)
{
    if (!device.physical) return (Rvk_Buffer){0};
    if (!device.logical) return (Rvk_Buffer){0};
    if (!device.command_pool) return (Rvk_Buffer){0};

    /* book keeping */
    Rvk_Buffer buff = {.info.range = size};

    /* create a uniform buffer */
    if (!vk_create_buffer(device.logical, NULL, &buff.info.buffer,
                          .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                          .size = size)) return (Rvk_Buffer){0};
    VkMemoryPropertyFlags mem_props = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    RVK_ASSERT((buff.memory = r_allocate_and_bind_buffer_memory(device, mem_props, buff.info.buffer)));
    RVK_ASSERT(RVK(vkMapMemory(device.logical, buff.memory, 0, size, 0, &buff.mapped)));

    return buff;
}

Rvk_Buffer r_create_compute_buffer_from_host(Rvk_Device device, size_t size, void *data)
{
    if (!device.physical) return (Rvk_Buffer){0};
    if (!device.logical) return (Rvk_Buffer){0};
    if (!device.command_pool) return (Rvk_Buffer){0};

    /* book keeping */
    Rvk_Buffer buff = {.info.range = size};

    /* create a buffer */
    if (!vk_create_buffer(device.logical, NULL, &buff.info.buffer,
                          .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT|   // destination of staging buffer
                                   VK_BUFFER_USAGE_TRANSFER_SRC_BIT|   // in case we want to transfer back to host
                                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, // compute buffer (ssbo)
                          .size = size)) return (Rvk_Buffer){0};
    RVK_ASSERT((buff.memory = r_allocate_and_bind_buffer_memory(device,
                                                                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                                                buff.info.buffer)));

    /* create a staging buffer */
    Rvk_Buffer stg_buff = {0};
    RVK_ASSERT(vk_create_buffer(device.logical, NULL, &stg_buff.info.buffer,
                                .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                .size = size));
    RVK_ASSERT((stg_buff.memory = r_allocate_and_bind_buffer_memory(device,
                                                                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|
                                                                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                                                    stg_buff.info.buffer)));
    /* copy data to staging buffer */
    RVK_ASSERT(RVK(vkMapMemory(device.logical, stg_buff.memory, 0, size, 0, &stg_buff.mapped)));
    memcpy(stg_buff.mapped, data, size);
    vkUnmapMemory(device.logical, stg_buff.memory);

    /* transfer staging buffer to vertex buffer */
    VkCommandBuffer tmp_cmd_buff = r_begin_tmp_cmd_buff(device.command_pool, device.logical);
        VkBufferCopy copy_region = {.size = size};
        vkCmdCopyBuffer(tmp_cmd_buff, stg_buff.info.buffer, buff.info.buffer, 1, &copy_region);
    r_end_tmp_cmd_buff(device.queue, device.logical, device.command_pool, tmp_cmd_buff);

    /* destroy the staging buffer */
    vkDestroyBuffer(device.logical, stg_buff.info.buffer, NULL);
    vkFreeMemory(device.logical, stg_buff.memory, NULL);

    return buff;
}

Rvk_Buffer r_create_compute_buffer(Rvk_Device device, size_t size)
{
    if (!device.physical) return (Rvk_Buffer){0};
    if (!device.logical) return (Rvk_Buffer){0};
    if (!device.command_pool) return (Rvk_Buffer){0};

    /* book keeping */
    Rvk_Buffer buff = {.info.range = size};

    /* create a buffer */
    if (!vk_create_buffer(device.logical, NULL, &buff.info.buffer,
                          .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT|   // destination of staging buffer
                                   VK_BUFFER_USAGE_TRANSFER_SRC_BIT|   // in case we want to transfer back to host
                                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, // compute buffer (ssbo)
                          .size = size)) return (Rvk_Buffer){0};
    RVK_ASSERT((buff.memory = r_allocate_and_bind_buffer_memory(device,
                                                                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                                                buff.info.buffer)));

    return buff;
}

void r_destroy_rvk_buffer(VkDevice device, Rvk_Buffer buffer)
{
    vkDestroyBuffer(device, buffer.info.buffer, NULL);
    vkFreeMemory(device, buffer.memory, NULL);
}

bool r_wait_reset_fence(VkDevice device, VkFence *fence)
{
    if (!RVK(vkWaitForFences(device, 1, fence, VK_TRUE, UINT64_MAX))) return false;
    if (!RVK(vkResetFences(device, 1, fence))) return false;
    return true;
}

// TODO: note that this doesn't handle the case where the swapchain needs to be resized
bool r_acquire_next_image(VkDevice device, VkSwapchainKHR swapchain, VkSemaphore image_available, uint32_t *image_index)
{
    return RVK(vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, image_available, VK_NULL_HANDLE, image_index));
}

bool r_reset_begin_cmd_buff(VkCommandBuffer cmd_buff)
{
    if (!RVK(vkResetCommandBuffer(cmd_buff, 0))) return false;
    VkCommandBufferBeginInfo begin_info = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, };
    if (!RVK(vkBeginCommandBuffer(cmd_buff, &begin_info))) return false;
    return true;
}

bool r_submit(Rvk_Device device, uint32_t current_frame)
{
    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT|VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    // VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
    VkSubmitInfo submit = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &device.cmd_buffs[current_frame],
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &device.render_finished_sems[current_frame],
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &device.image_available_sems[current_frame],
        .pWaitDstStageMask = &wait_stage,
    };
    return RVK(vkQueueSubmit(device.queue, 1, &submit, device.fences[current_frame]));
}

bool r_present(VkQueue queue, VkSemaphore render_finished, uint32_t image_index, VkSwapchainKHR swapchain)
{
    VkPresentInfoKHR present = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &render_finished,
        .swapchainCount = 1,
        .pSwapchains = &swapchain,
        .pImageIndices = &image_index,
    };
    return RVK(vkQueuePresentKHR(queue, &present));
}

void r_cmd_begin_render_pass(VkCommandBuffer cb, VkRenderPass rp, VkFramebuffer fb, VkExtent2D extent, float r, float g, float b, float a)
{
    VkClearValue clear_color = { .color = {{r, g, b, a}} };
    VkClearValue clear_depth = {
        .depthStencil = {
            .depth = 1.0f,
            .stencil = 0,
        }
    };
    VkClearValue clear_values[] = {clear_color, clear_depth};
    VkRenderPassBeginInfo begin_rp = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = rp,
        .framebuffer = fb,
        .renderArea.extent = extent,
        .clearValueCount = RVK_ARRAY_LEN(clear_values),
        .pClearValues = clear_values,
    };
    vkCmdBeginRenderPass(cb, &begin_rp, VK_SUBPASS_CONTENTS_INLINE);
} 

bool r_transition_image_layout(Rvk_Device device, VkImage image, VkImageLayout old_layout, VkImageLayout new_layout)
{
    VkPipelineStageFlags src_stg_mask;
    VkPipelineStageFlags dst_stg_mask;
    VkAccessFlags src_access_mask;
    VkAccessFlags dst_access_mask;
    if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        src_access_mask = 0;
        dst_access_mask = VK_ACCESS_TRANSFER_WRITE_BIT;
        src_stg_mask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dst_stg_mask = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
               new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        src_access_mask = VK_ACCESS_TRANSFER_WRITE_BIT;
        dst_access_mask = VK_ACCESS_SHADER_READ_BIT;
        src_stg_mask = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dst_stg_mask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_GENERAL) {
        src_access_mask = 0;
        dst_access_mask = 0;
        src_stg_mask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        dst_stg_mask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    } else {
        r_log(RVK_ERROR, "old_layout %d with new_layout %d not allowed yet", old_layout, new_layout);
        return false;
    }

    VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = src_access_mask,
        .dstAccessMask = dst_access_mask,
        .oldLayout = old_layout,
        .newLayout = new_layout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .levelCount = 1,
            .layerCount = 1,
        },
    };

    VkCommandBuffer tmp = r_begin_tmp_cmd_buff(device.command_pool, device.logical);
    vkCmdPipelineBarrier(tmp, src_stg_mask, dst_stg_mask, 0, 0, NULL, 0, NULL, 1, &barrier);
    r_end_tmp_cmd_buff(device.queue, device.logical, device.command_pool, tmp);

    return true;
}

char *r_temp_strdup(const char *cstr)
{
    size_t n = strlen(cstr);
    char *result = (char*)r_temp_alloc(n + 1);
    RVK_ASSERT(result != NULL && "Increase RVK_TEMP_CAPACITY");
    memcpy(result, cstr, n);
    result[n] = '\0';
    return result;
}

char *r_temp_strndup(const char *s, size_t n)
{
    char *r = r_temp_alloc(n + 1);
    RVK_ASSERT(r != NULL && "Extend the size of the temporary allocator");
    memcpy(r, s, n);
    r[n] = '\0';
    return r;
}

void *r_temp_alloc(size_t requested_size)
{
    size_t word_size = sizeof(uintptr_t);
    size_t size = (requested_size + word_size - 1)/word_size*word_size;
    if (r_temp_size + size > RVK_TEMP_CAPACITY) return NULL;
    void *result = &r_temp[r_temp_size];
    r_temp_size += size;
    return result;
}

void *r_temp_calloc(size_t requested_size)
{
    void *result = r_temp_alloc(requested_size);
    if (result) {
        memset(result, 0, requested_size);
        return result;
    }

    return NULL;
}

char *r_temp_sprintf(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int n = vsnprintf(NULL, 0, format, args);
    va_end(args);

    RVK_ASSERT(n >= 0);
    char *result = (char*)r_temp_alloc(n + 1);
    RVK_ASSERT(result != NULL && "Extend the size of the temporary allocator");
    // TODO: use proper arenas for the temporary allocator;
    va_start(args, format);
    vsnprintf(result, n + 1, format, args);
    va_end(args);

    return result;
}

void r_temp_reset(void)
{
    r_temp_size = 0;
}

size_t r_temp_save(void)
{
    return r_temp_size;
}

void r_temp_rewind(size_t checkpoint)
{
    r_temp_size = checkpoint;
}

VkDescriptorPool r_create_default_descriptor_pool(VkDevice device)
{
    VkDescriptorPool pool = VK_NULL_HANDLE;
    VkDescriptorPoolSize pool_sizes[] = {
        {.type = VK_DESCRIPTOR_TYPE_SAMPLER,                .descriptorCount = RVK_MAX_DESCRIPTOR_SETS},
        {.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = RVK_MAX_DESCRIPTOR_SETS},
        {.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,          .descriptorCount = RVK_MAX_DESCRIPTOR_SETS},
        {.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          .descriptorCount = RVK_MAX_DESCRIPTOR_SETS},
        {.type = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,   .descriptorCount = RVK_MAX_DESCRIPTOR_SETS},
        {.type = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,   .descriptorCount = RVK_MAX_DESCRIPTOR_SETS},
        {.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         .descriptorCount = RVK_MAX_DESCRIPTOR_SETS},
        {.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         .descriptorCount = RVK_MAX_DESCRIPTOR_SETS},
        {.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, .descriptorCount = RVK_MAX_DESCRIPTOR_SETS},
        {.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, .descriptorCount = RVK_MAX_DESCRIPTOR_SETS},
        {.type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,       .descriptorCount = RVK_MAX_DESCRIPTOR_SETS},
    };

    /* don't catch any errors, just return null handle if that's the case */
    vk_create_descriptor_pool(device, NULL, &pool,
                              .poolSizeCount = RVK_ARRAY_LEN(pool_sizes),
                              .pPoolSizes    = pool_sizes,
                              .maxSets       = RVK_MAX_DESCRIPTOR_SETS);

    return pool;
}

size_t format_to_size(VkFormat fmt)
{
    if (VK_FORMAT_R8G8B8A8_UNORM <= fmt && fmt <= VK_FORMAT_B8G8R8A8_SRGB) {
        return 4;
    } else if (VK_FORMAT_R8_UNORM <= fmt && fmt <= VK_FORMAT_R8_SRGB) {
        return 1;
    } else {
        r_log(RVK_WARNING, "unrecognized format %d, returning 4 instead", fmt);
        return 4;
    }
}

// format e.g.: VK_FORMAT_R8G8B8A8_UNORM
Rvk_Texture r_create_texture(Rvk_Device device, VkExtent2D extent, void *data, VkFormat format)
{
    Rvk_Texture texture = {0};

    texture.image = r_create_2D_image(device.logical, format,
                                      VK_IMAGE_USAGE_SAMPLED_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT, extent);
    if (!texture.image) goto defer;
    texture.memory = r_allocate_and_bind_image_memory(device, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, texture.image);
    if (!texture.memory) goto defer;

    /* create a staging buffer to hold our image data */
    Rvk_Buffer stg_buff = {0};
    size_t size = extent.width*extent.height*format_to_size(format);
    RVK_ASSERT(vk_create_buffer(device.logical, NULL, &stg_buff.info.buffer,
                                .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                .size = size));
    RVK_ASSERT((stg_buff.memory = r_allocate_and_bind_buffer_memory(device,
                                                                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|
                                                                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                                                    stg_buff.info.buffer)));
    /* copy data to staging buffer */
    RVK_ASSERT(RVK(vkMapMemory(device.logical, stg_buff.memory, 0, size, 0, &stg_buff.mapped)));
    memcpy(stg_buff.mapped, data, size);
    vkUnmapMemory(device.logical, stg_buff.memory);

    /* transition the image layout */
    if (!r_transition_image_layout(device, texture.image,
                                   VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)) goto defer;

    /* transfer staging buffer to vertex buffer */
    VkCommandBuffer tmp_cmd_buff = r_begin_tmp_cmd_buff(device.command_pool, device.logical);
        VkBufferImageCopy copy_region = {
            .imageSubresource = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .layerCount = 1, },
            .imageExtent = {extent.width, extent.height, 1},
        };
        vkCmdCopyBufferToImage(tmp_cmd_buff, stg_buff.info.buffer, texture.image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region);
    r_end_tmp_cmd_buff(device.queue, device.logical, device.command_pool, tmp_cmd_buff);

    if (!r_transition_image_layout(device, texture.image,
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)) goto defer;

    if (!vk_create_image_view(device.logical, NULL, &texture.info.imageView,
                              .image = texture.image,
                              .viewType = VK_IMAGE_VIEW_TYPE_2D,
                              .format = format,
                              .subresourceRange = {
                                  .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                  .levelCount = 1, .layerCount = 1,})) goto defer;
    if (!vk_create_sampler(device.logical, NULL, &texture.info.sampler,
                           .compareOp = VK_COMPARE_OP_ALWAYS,
                           .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK)) goto defer;

    /* necessary book keeping */
    texture.info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    /* destroy the staging buffer */
    vkDestroyBuffer(device.logical, stg_buff.info.buffer, NULL);
    vkFreeMemory(device.logical, stg_buff.memory, NULL);

defer:
    return texture;
}

void r_destroy_texture(VkDevice device, Rvk_Texture texture)
{
    vkDestroySampler(device, texture.info.sampler, NULL);
    vkDestroyImageView(device, texture.info.imageView, NULL);
    vkDestroyImage(device, texture.image, NULL);
    vkFreeMemory(device, texture.memory, NULL);
}

/***********************************************************************************
*  vk_* API implementation
************************************************************************************/

bool vk_create_instance_(const VkAllocationCallbacks *pAllocator, VkInstance* pInstance, VkInstanceCreateInfo ci)
{
    ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    if (ci.pApplicationInfo) {
        VkBaseOutStructure *base = (VkBaseOutStructure *)ci.pApplicationInfo;
        base->sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    }

    return RVK(vkCreateInstance(&ci, pAllocator, pInstance));
}

bool vk_create_device_(VkPhysicalDevice physical_device, const VkAllocationCallbacks *pAllocator, VkDevice *pDevice, VkDeviceCreateInfo ci)
{
    ci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

    for (uint32_t i = 0; i < ci.queueCreateInfoCount; i++) {
        VkBaseOutStructure *base = (VkBaseOutStructure *)&ci.pQueueCreateInfos[i];
        base->sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    }

    return RVK(vkCreateDevice(physical_device, &ci, pAllocator, pDevice));
}

bool vk_create_swapchain_khr_(VkDevice device, const VkAllocationCallbacks* pAllocator, VkSwapchainKHR* pSwapchain, VkSwapchainCreateInfoKHR ci)
{
    ci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    return RVK(vkCreateSwapchainKHR(device, &ci, pAllocator, pSwapchain));
}

bool vk_create_image_view_(VkDevice device, const VkAllocationCallbacks* pAllocator, VkImageView* pView, VkImageViewCreateInfo ci)
{
    ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    return RVK(vkCreateImageView(device, &ci, pAllocator, pView));
}

bool vk_create_render_pass_(VkDevice device, const VkAllocationCallbacks *pAllocator, VkRenderPass *pRenderPass, VkRenderPassCreateInfo ci)
{
    ci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    return RVK(vkCreateRenderPass(device, &ci, pAllocator, pRenderPass));
}

bool vk_create_image_(VkDevice device, const VkAllocationCallbacks *pAllocator, VkImage *pImage, VkImageCreateInfo ci)
{
    ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    return RVK(vkCreateImage(device, &ci, pAllocator, pImage));
}

bool vk_create_command_pool_(VkDevice device, const VkAllocationCallbacks *pAllocator, VkCommandPool *pCommandPool, VkCommandPoolCreateInfo ci)
{
    ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    return RVK(vkCreateCommandPool(device, &ci, pAllocator, pCommandPool));
}

bool vk_allocate_command_buffers_(VkDevice device, VkCommandBuffer *pCommandBuffers, VkCommandBufferAllocateInfo ci)
{
    ci.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    return RVK(vkAllocateCommandBuffers(device, &ci, pCommandBuffers));
}

bool vk_create_semaphore_(VkDevice device, const VkAllocationCallbacks *pAllocator, VkSemaphore *pSemaphore, VkSemaphoreCreateInfo ci)
{
    ci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    return RVK(vkCreateSemaphore(device, &ci, pAllocator, pSemaphore));
}

bool vk_create_fence_(VkDevice device, const VkAllocationCallbacks *pAllocator, VkFence *pFence, VkFenceCreateInfo ci)
{
    ci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    return RVK(vkCreateFence(device, &ci, pAllocator, pFence));
}

bool vk_create_pipeline_layout_(VkDevice device, const VkAllocationCallbacks *pAllocator, VkPipelineLayout *pPipelineLayout, VkPipelineLayoutCreateInfo ci)
{
    ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    return RVK(vkCreatePipelineLayout(device, &ci, pAllocator, pPipelineLayout));
}

bool vk_create_graphics_pipeline_(VkDevice device, VkPipelineCache pipelineCache, const VkAllocationCallbacks *pAllocator, VkPipeline *pPipeline, VkGraphicsPipelineCreateInfo ci)
{
    ci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

    for (uint32_t i = 0; i < ci.stageCount; i++) {
        VkBaseOutStructure *base = (VkBaseOutStructure *)&ci.pStages[i];
        base->sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    }

    if (ci.pVertexInputState) {
        VkBaseOutStructure *base = (VkBaseOutStructure *)ci.pVertexInputState;
        base->sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    }

    if (ci.pInputAssemblyState) {
        VkBaseOutStructure *base = (VkBaseOutStructure *)ci.pInputAssemblyState;
        base->sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    }

    if (ci.pTessellationState) {
        VkBaseOutStructure *base = (VkBaseOutStructure *)ci.pTessellationState;
        base->sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
    }

    if (ci.pViewportState) {
        VkBaseOutStructure *base = (VkBaseOutStructure *)ci.pViewportState;
        base->sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    }

    if (ci.pRasterizationState) {
        VkBaseOutStructure *base = (VkBaseOutStructure *)ci.pRasterizationState;
        base->sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    }

    if (ci.pMultisampleState) {
        VkBaseOutStructure *base = (VkBaseOutStructure *)ci.pMultisampleState;
        base->sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    }

    if (ci.pDepthStencilState) {
        VkBaseOutStructure *base = (VkBaseOutStructure *)ci.pDepthStencilState;
        base->sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    }

    if (ci.pColorBlendState) {
        VkBaseOutStructure *base = (VkBaseOutStructure *)ci.pColorBlendState;
        base->sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    }

    if (ci.pDynamicState) {
        VkBaseOutStructure *base = (VkBaseOutStructure *)ci.pDynamicState;
        base->sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    }

    return RVK(vkCreateGraphicsPipelines(device, pipelineCache, 1, &ci, pAllocator, pPipeline));
}

bool vk_create_compute_pipeline_(VkDevice device, VkPipelineCache pipelineCache, const VkAllocationCallbacks *pAllocator, VkPipeline *pPipeline, VkComputePipelineCreateInfo ci)
{
    ci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    return RVK(vkCreateComputePipelines(device, pipelineCache, 1, &ci, pAllocator, pPipeline));
}

bool vk_create_shader_module_(VkDevice device, const VkAllocationCallbacks *pAllocator, VkShaderModule *pShaderModule, VkShaderModuleCreateInfo ci)
{
    ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    return RVK(vkCreateShaderModule(device, &ci, pAllocator, pShaderModule));
}

bool vk_create_buffer_(VkDevice device, const VkAllocationCallbacks *pAllocator, VkBuffer *pBuffer, VkBufferCreateInfo ci)
{
    ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    return RVK(vkCreateBuffer(device, &ci, pAllocator, pBuffer));
}

bool vk_create_descriptor_pool_(VkDevice device, const VkAllocationCallbacks *pAllocator, VkDescriptorPool *pDescriptorPool, VkDescriptorPoolCreateInfo ci)
{
    ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    return RVK(vkCreateDescriptorPool(device, &ci, pAllocator, pDescriptorPool));
}

bool vk_create_descriptor_set_layout_(VkDevice device, const VkAllocationCallbacks *pAllocator, VkDescriptorSetLayout *pSetLayout, VkDescriptorSetLayoutCreateInfo ci)
{
    ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    return RVK(vkCreateDescriptorSetLayout(device, &ci, pAllocator, pSetLayout));
}

bool vk_allocate_descriptor_sets_(VkDevice device, VkDescriptorSet *pDescriptorSets, VkDescriptorSetAllocateInfo ci)
{
    ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    return RVK(vkAllocateDescriptorSets(device, &ci, pDescriptorSets));
}

bool vk_create_sampler_(VkDevice device, const VkAllocationCallbacks *pAllocator, VkSampler *pSampler, VkSamplerCreateInfo ci)
{
    ci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    return RVK(vkCreateSampler(device, &ci, pAllocator, pSampler));
}

#endif // RVK_IMPLEMENTATION

/*
    Revision history:

    0.0.0 (2025-12-04) Highly experimental version.

*/

/*
   MIT License
   Copyright (c) 2025 Reese Gallagher
   Permission is hereby granted, free of charge, to any person obtaining a copy of
   this software and associated documentation files (the "Software"), to deal in
   the Software without restriction, including without limitation the rights to
   use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
   of the Software, and to permit persons to whom the Software is furnished to do
   so, subject to the following conditions:
   The above copyright notice and this permission notice shall be included in all
   copies or substantial portions of the Software.
   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
   SOFTWARE.
*/
