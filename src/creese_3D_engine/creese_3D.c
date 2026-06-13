#include "../creese_3D.h"

#define RVK_LOG_LEVEL RVK_INFO
#define RVK_IMPLEMENTATION
#include "../external/rvk.h"

#define NOB_STRIP_PREFIX
#define NOB_IMPLEMENTATION
#include "../../nob.h"

#ifdef VULKAN_VALIDATION_ON
    #define VK_VALIDATION 1
#else
    #define VK_VALIDATION 0
#endif

#define RAYMATH_IMPLEMENTATION
#include "../external/raymath.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "../external/stb_truetype.h"

#ifndef Z_NEAR
    #define Z_NEAR 0.1
#endif
#ifndef Z_FAR
    #define Z_FAR 500.0
#endif

#ifndef DEFAULT_BITMAP_WIDTH
    #define DEFAULT_BITMAP_WIDTH 400
#endif
#ifndef DEFAULT_BITMAP_HEIGHT
    #define DEFAULT_BITMAP_HEIGHT 400
#endif

#define FPS_CAPTURE_FRAMES_COUNT 30

#define IMAGE_WORKGROUP_SIZE 16.0f
#define POINT_WORKGROUP_SIZE 1024.0f
#define POINTS_PER_BATCH (1<<22)

static struct {
    const char **items;
    size_t count;
    size_t capacity;
} layers = {0};

static struct {
    const char **items;
    size_t count;
    size_t capacity;
} instance_exts = {0};

static struct {
    const char **items;
    size_t count;
    size_t capacity;
} device_exts = {0};

static Vulkan_Context ctx = {0};
static Frame_Context frm_ctx = {0};

#define MAX_MAT_STACK 1024 * 1024
static Matrix mat_stack[MAX_MAT_STACK];
static size_t mat_stack_p = 0;

static struct {
    Matrix view;
    Matrix proj;
} matrices = {0};

static Camera current_camera = {0};

/* standard descriptor set layouts */
enum {
    DS_LAYOUT_CLEAR_BACKGROUND,
    DS_LAYOUT_RENDER,
    DS_LAYOUT_RESOLVE,
    DS_LAYOUT_TEXT,
    DS_LAYOUT_HIDDEN_SURFACE_REMOVAL,
    DS_LAYOUT_HOLE_FILLING,
    DS_LAYOUT_DISPLAY,
    DS_LAYOUT_COUNT,
};

#define FRAME_BUFFER_COUNT 2

/* standard pipelines for */
static struct {
    struct { Rvk_Pipeline pl; VkDescriptorSet ds; } display;
    struct { Rvk_Pipeline pl; VkDescriptorSet ds; } clear_background;
    struct { Rvk_Pipeline pl;                     } render;
    struct { Rvk_Pipeline pl; VkDescriptorSet ds; } hidden_surface;
    struct { Rvk_Pipeline pl; VkDescriptorSet ds[FRAME_BUFFER_COUNT]; } hole_filling;
    struct { Rvk_Pipeline pl; VkDescriptorSet ds; } resolve;
    struct { Rvk_Pipeline pl; VkDescriptorSet ds; Rvk_Texture texture; } text;

    /* For for certain pipelines (e.g. hidden surface and hole filling) we need two frame buffers.
     * In those cases we read from one frame buffer and write to another. */
    Rvk_Buffer frame_buffers[FRAME_BUFFER_COUNT];
    bool last_buff_write_was_fb1;
    Rvk_Texture display_texture;
    Rvk_Buffer uniform_buff;
    struct {
        float16 proj;
        float16 view;
        float16 model;
        int frame_width;
        int frame_height;
        uint32_t bg_color;
        int padding_1;
        Vector4 camera_pos;
    } uniform_data;

    VkDescriptorSetLayout ds_layouts[DS_LAYOUT_COUNT];
} standard = {0};

Rvk_Primitive_2D_Vertex primitive_2D_vertices[] = {
    {{-1.0f, -1.0f}, {1.0f, 0.0f, 0.0f}},
    {{ 1.0f, -1.0f}, {0.0f, 1.0f, 0.0f}},
    {{-1.0f,  1.0f}, {0.0f, 0.0f, 1.0f}},
    {{ 1.0f,  1.0f}, {1.0f, 1.0f, 1.0f}},
};

uint16_t primitive_2D_indices[] = {0, 1, 2, 2, 1, 3};

Rvk_Primitive_2D_Vertex text_vertices[] = {
    {{-1.0f, -1.0f}, {1.0f, 0.0f, 0.0f}},
    {{ 1.0f, -1.0f}, {0.0f, 1.0f, 0.0f}},
    {{-1.0f,  1.0f}, {0.0f, 0.0f, 1.0f}},
    {{ 1.0f,  1.0f}, {1.0f, 1.0f, 1.0f}},
};
uint16_t text_indices[] = {0, 1, 2, 2, 1, 3};

Rvk_Line_Vertex bounding_box_vertices[8] = {
    {.position = {-0.5f, -0.5f,  0.5f}, .color = 0xff000000},
    {.position = { 0.5f, -0.5f,  0.5f}, .color = 0xff000000},
    {.position = {-0.5f,  0.5f,  0.5f}, .color = 0xff000000},
    {.position = { 0.5f,  0.5f,  0.5f}, .color = 0xff000000},
    {.position = {-0.5f, -0.5f, -0.5f}, .color = 0xff000000},
    {.position = { 0.5f, -0.5f, -0.5f}, .color = 0xff000000},
    {.position = {-0.5f,  0.5f, -0.5f}, .color = 0xff000000},
    {.position = { 0.5f,  0.5f, -0.5f}, .color = 0xff000000},
};
uint16_t bounding_box_indices[24] = {0, 1, 2, 3, 0, 2, 1, 3,
                                     4, 5, 6, 7, 4, 6, 5, 7,
                                     0, 4, 1, 5, 2, 6, 3, 7};

Rvk_Line_Vertex frustum_vertices[8] = {
    {.position = {-1.0f, -1.0f, 0.0f}, .color = 0xff000000},
    {.position = { 1.0f, -1.0f, 0.0f}, .color = 0xff000000},
    {.position = {-1.0f,  1.0f, 0.0f}, .color = 0xff000000},
    {.position = { 1.0f,  1.0f, 0.0f}, .color = 0xff000000},
    {.position = {-1.0f, -1.0f, 1.0f}, .color = 0xff000000},
    {.position = { 1.0f, -1.0f, 1.0f}, .color = 0xff000000},
    {.position = {-1.0f,  1.0f, 1.0f}, .color = 0xff000000},
    {.position = { 1.0f,  1.0f, 1.0f}, .color = 0xff000000},
};
uint16_t frustum_indices[24] = {0, 1, 2, 3, 0, 2, 1, 3,
                                4, 5, 6, 7, 4, 6, 5, 7,
                                0, 4, 1, 5, 2, 6, 3, 7};

#define MAX_KEYBOARD_KEYS 512
#define MAX_KEY_PRESSED_QUEUE 16
#define MAX_CHAR_PRESSED_QUEUE 16
#define CAMERA_MOVE_SPEED 10.0f
#define CAMERA_MOUSE_MOVE_SENSITIVITY 0.1f
#define CAMERA_ROT_SENSITIVITY 0.1f
#define GAMEPAD_ROT_SENSITIVITY 1.0f
#define MAX_MOUSE_BUTTONS 8
#define MAX_GAMEPAD_BUTTONS 32
#define MAX_GAMEPAD_AXIS 8
#define FPS_CAPTURE_FRAMES_COUNT 30
#define FPS_AVERAGE_TIME_SECONDS 0.5f
#define FPS_STEP (FPS_AVERAGE_TIME_SECONDS/FPS_CAPTURE_FRAMES_COUNT)
#define DEAD_ZONE 0.25f

struct {
    int exit_key;
    char curr_key_state[MAX_KEYBOARD_KEYS];
    char prev_key_state[MAX_KEYBOARD_KEYS];
    char key_repeat_in_frame[MAX_KEYBOARD_KEYS];
    int key_pressed_queue[MAX_KEY_PRESSED_QUEUE];
    int key_pressed_queue_count;
    int char_pressed_queue[MAX_CHAR_PRESSED_QUEUE];
    int char_pressed_queue_count;
} keyboard;

struct {
    Vector2 prev_pos;
    Vector2 curr_pos;
    Vector2 curr_wheel_move;
    Vector2 prev_wheel_move;
    char curr_button_state[MAX_MOUSE_BUTTONS];
    char prev_button_state[MAX_MOUSE_BUTTONS];
} mouse;

struct {
    float axis_state[MAX_GAMEPAD_AXIS];
    char curr_button_state[MAX_GAMEPAD_BUTTONS];
    char prev_button_state[MAX_GAMEPAD_BUTTONS];
    int last_button_pressed;
} gamepad;

static Window window = {0};

static void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods);
static void mouse_cursor_pos_callback(GLFWwindow *window, double x, double y);
static void mouse_button_callback(GLFWwindow *window, int button, int action, int mods);
static void mouse_scroll_callback(GLFWwindow *window, double x_offset, double y_offset);
void init_standard_pipelines();
void resolve_image();

void init_window(int width, int height, char *title)
{
    /* initialize glfw and window */
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    window.handle = glfwCreateWindow(width, height, title, NULL, NULL);
    glfwSetKeyCallback(window.handle, key_callback);
    glfwSetMouseButtonCallback(window.handle, mouse_button_callback);
    glfwSetCursorPosCallback(window.handle, mouse_cursor_pos_callback);
    glfwSetScrollCallback(window.handle, mouse_scroll_callback);
    window.width  = width;
    window.height = height;

    VkDebugUtilsMessengerCreateInfoEXT debug_messenger_ci = r_get_debug_messenger_info();

    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = APP_NAME,
        .applicationVersion = VK_MAKE_VERSION(0, 0, 1),
        .pEngineName = "Point Cloud Renderer 2.0",
        .engineVersion = VK_MAKE_VERSION(0, 0, 1),
        .apiVersion = VK_API_VERSION_1_3, // TODO: 1_0_0
    };

    if (VK_VALIDATION) {
        da_append(&instance_exts, "VK_EXT_debug_utils");
        da_append(&layers, "VK_LAYER_KHRONOS_validation");
    }

    uint32_t req_ext_count = 0;
    const char **req_inst_exts = glfwGetRequiredInstanceExtensions(&req_ext_count);
    for (uint32_t i = 0; i < req_ext_count; i++) da_append(&instance_exts, req_inst_exts[i]);

    /* create vulkan instance (w/ or w/o validation layers i.e. VK_VALIDATION = 1/0) */
    vk_create_instance(NULL, &ctx.instance,
                       .pNext = (VK_VALIDATION) ? &debug_messenger_ci : NULL,
                       .ppEnabledLayerNames = layers.items,
                       .enabledLayerCount = layers.count,
                       .ppEnabledExtensionNames = instance_exts.items,
                       .enabledExtensionCount = instance_exts.count,
                       .pApplicationInfo = &app_info);
    assert(ctx.instance);

    /* create the vulkan surface */
    assert(RVK(glfwCreateWindowSurface(ctx.instance, window.handle, NULL, &ctx.surface)));

    da_append(&device_exts, "VK_KHR_swapchain");
    Rvk_Device_Config device_config = {
        .extension_count = device_exts.count,
        .extensions = device_exts.items,
        .layer_count = layers.count,
        .layers = layers.items,
        .atomic_features = true,
    };
    ctx.device = r_create_rvk_device(ctx.instance, ctx.surface, device_config);
    assert(ctx.device.logical);
    ctx.swapchain = r_create_rvk_swapchain(ctx.device, ctx.surface, width, height);
    assert(ctx.swapchain.handle);
    ctx.pool = r_create_default_descriptor_pool(ctx.device.logical);
    assert(ctx.pool);

    init_standard_pipelines();
}

void close_window()
{
    vkQueueWaitIdle(ctx.device.queue);

    for (size_t i = 0; i < FRAME_BUFFER_COUNT; i++)
        r_destroy_rvk_buffer(ctx.device.logical, standard.frame_buffers[i]);
    r_destroy_rvk_buffer(ctx.device.logical, standard.uniform_buff);
    r_destroy_texture(ctx.device.logical, standard.display_texture);
    r_destroy_texture(ctx.device.logical, standard.text.texture);
    destroy_pipeline(standard.display.pl);
    destroy_pipeline(standard.resolve.pl);
    destroy_pipeline(standard.hole_filling.pl);
    destroy_pipeline(standard.render.pl);
    destroy_pipeline(standard.hidden_surface.pl);
    destroy_pipeline(standard.clear_background.pl);
    destroy_pipeline(standard.text.pl);

    for (size_t i = 0; i < DS_LAYOUT_COUNT; i++)
        vkDestroyDescriptorSetLayout(ctx.device.logical, standard.ds_layouts[i], NULL);

    vkDestroyDescriptorPool(ctx.device.logical, ctx.pool, NULL);

    r_destroy_rvk_swapchain(ctx.device.logical, ctx.swapchain);
    r_destroy_rvk_device(ctx.device);
    vkDestroySurfaceKHR(ctx.instance, ctx.surface, NULL);
    vkDestroyInstance(ctx.instance, NULL);
}

bool window_should_close()
{
    bool result = (glfwGetKey(window.handle, GLFW_KEY_ESCAPE) == GLFW_PRESS) || glfwWindowShouldClose(window.handle);

    glfwPollEvents();

    /* wait for vulkan device to idle so that we can destroy stuff without getting validation warnings */
    if (result) wait_idle();

    return result;
}

Window get_window()
{
    return window;
}

void begin_drawing()
{
    begin_timer();
    r_wait_reset_fence(ctx.device.logical, &ctx.device.fences[0]);
    r_acquire_next_image(ctx.device.logical, ctx.swapchain.handle,
                         ctx.device.image_available_sems[0], &frm_ctx.img_idx);
    r_reset_begin_cmd_buff(ctx.device.cmd_buffs[0]);
} 

static struct {
    uint32_t frame_width;
    uint32_t frame_height;
    uint32_t clear_color;
} clear_background_push_const = {0};

void comp_to_comp_buff_barrier(VkBuffer buff);

void clear_background(Color bg_color)
{
    assert(standard.frame_buffers[0].info.buffer);
    assert(standard.clear_background.pl.handle);
    assert(standard.clear_background.pl.layout);
    assert(standard.clear_background.ds);

    VkCommandBuffer cb = ctx.device.cmd_buffs[0];

    int group_x = 1, group_y = 1, group_z = 1;
    group_x = ceil(window.width/IMAGE_WORKGROUP_SIZE);
    group_y = ceil(window.height/IMAGE_WORKGROUP_SIZE);
    clear_background_push_const.frame_width  = window.width;
    clear_background_push_const.frame_height = window.height;
    clear_background_push_const.clear_color  = color_to_uint32_t(bg_color);
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, standard.clear_background.pl.handle);
    vkCmdPushConstants(cb, standard.clear_background.pl.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                       sizeof(clear_background_push_const), &clear_background_push_const);
    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, standard.clear_background.pl.layout, 0, 1, &standard.clear_background.ds, 0, NULL);
    vkCmdDispatch(cb, group_x, group_y, group_z);

    /* barrier to ensure we have fully cleared the screen before anything else reads */
    comp_to_comp_buff_barrier(standard.frame_buffers[0].info.buffer);
}

void end_drawing()
{
    /* update our standard uniform buffer and copy to device visible buffer */
    standard.uniform_data.frame_width  = window.width;
    standard.uniform_data.frame_height = window.height;
    standard.uniform_data.bg_color     = clear_background_push_const.clear_color;
    memcpy(standard.uniform_buff.mapped, &standard.uniform_data, sizeof(standard.uniform_data));

    VkCommandBuffer cb = ctx.device.cmd_buffs[0];
    resolve_image();
    compute_to_frag_sample_image_barrier(cb, standard.display_texture.image);

    /* view the rendered image (displayed as a screen space triangle)*/
    VkFramebuffer fb = get_current_frame_buffer();
    r_cmd_begin_render_pass(cb, ctx.swapchain.render_pass, fb,
                            ctx.swapchain.extent, 0.0f, 0.0f, 0.0f, 1.0f);
        vkCmdBindPipeline(cb, 0, standard.display.pl.handle);
        vkCmdBindDescriptorSets(cb, 0, standard.display.pl.layout, 0, 1, &standard.display.ds, 0, NULL);
        r_cmd_set_viewport_scissor(cb, ctx.swapchain.extent);
        vkCmdDraw(cb, 3, 1, 0, 0);
    vkCmdEndRenderPass(cb);

    /* submit the command buffer and present when ready */
    RVK(vkEndCommandBuffer(ctx.device.cmd_buffs[0]));
    r_submit(ctx.device, 0);
    r_present(ctx.device.queue, ctx.device.render_finished_sems[0], frm_ctx.img_idx, ctx.swapchain.handle);

    end_timer();
    poll_input_events();
}

static void wait_time(double seconds)
{
    if (seconds <= 0) return;

    /* prepare for partial busy wait loop */
    double destination_time = get_time() + seconds;
    double sleep_secs = seconds - seconds * 0.05;

    /* for now wait time only supports linux */
#if defined(__linux__)
    struct timespec req = {0};
    time_t sec = sleep_secs;
    long nsec = (sleep_secs - sec) * 1000000000L;
    req.tv_sec = sec;
    req.tv_nsec = nsec;

    while (nanosleep(&req, &req) == -1) continue;
#endif

#if defined(_WIN32)
    Sleep((unsigned long)(sleep_secs * 1000.0));
#endif

    /* partial busy wait loop */
    while (get_time() < destination_time) {}
}

void begin_timer()
{
    frm_ctx.time.curr   = get_time();
    frm_ctx.time.prev   = (frm_ctx.time.prev == 0) ? frm_ctx.time.curr : frm_ctx.time.prev; // avoid huge time diff on first frame
    frm_ctx.time.update = frm_ctx.time.curr - frm_ctx.time.prev;
    frm_ctx.time.prev   = frm_ctx.time.curr;
}

void end_timer()
{
    frm_ctx.time.curr = get_time();
    frm_ctx.time.draw = frm_ctx.time.curr - frm_ctx.time.prev;
    frm_ctx.time.prev = frm_ctx.time.curr;
    frm_ctx.time.frame = frm_ctx.time.update + frm_ctx.time.draw;

    if (frm_ctx.time.frame < frm_ctx.time.target) {
        wait_time(frm_ctx.time.target - frm_ctx.time.frame);

        frm_ctx.time.curr = get_time();
        double wait = frm_ctx.time.curr - frm_ctx.time.prev;
        frm_ctx.time.prev = frm_ctx.time.curr;
        frm_ctx.time.frame += wait;
    }

    frm_ctx.time.frame_count++;
}

double get_frame_time()
{
    return frm_ctx.time.frame;
}

double get_time()
{
    return glfwGetTime();
}

Vulkan_Context get_vulkan_context()
{
    return ctx;
}

Matrix calc_projection(Camera camera)
{
    double aspect = ctx.swapchain.extent.width / (double)ctx.swapchain.extent.height;
    return MatrixPerspectiveVk(camera.fovy * DEG2RAD, aspect, Z_NEAR, Z_FAR);
}

void begin_mode_3D(Camera camera)
{
    matrices.proj = calc_projection(camera);
    matrices.view = MatrixLookAt(camera.position, camera.target, camera.up);
    current_camera = camera;

    push_matrix();
}

void hidden_surface_removal();
void copy_fb1_to_fb0();

void end_mode_3D()
{
    standard.uniform_data.proj         = MatrixToFloatV(get_projection());
    standard.uniform_data.view         = MatrixToFloatV(get_view());
    standard.uniform_data.model        = MatrixToFloatV(get_model());
    Vector4 cam_pos = (Vector4){current_camera.position.x, current_camera.position.y, current_camera.position.z, 1.0};
    standard.uniform_data.camera_pos   = cam_pos;
    pop_matrix();

    while (mat_stack_p > 0) {
        pop_matrix();
        r_log(RVK_WARNING, "more matrix pushes than pops");
    }
}

void push_matrix()
{
    if (mat_stack_p < MAX_MAT_STACK) {
        if (mat_stack_p) {
            mat_stack[mat_stack_p] = mat_stack[mat_stack_p - 1];
            mat_stack_p++;
        } else {
            mat_stack[mat_stack_p++] = MatrixIdentity();
        }
    } else {
        r_log(RVK_ERROR, "matrix stack overflow");
    }
}

void pop_matrix()
{
    if (mat_stack_p > 0)
        mat_stack_p--;
    else
        r_log(RVK_ERROR, "matrix stack underflow");
}

Matrix get_projection()
{
    return matrices.proj;
}

Matrix get_view()
{
    return matrices.view;
}

Matrix get_model()
{
    Matrix model = MatrixIdentity();
    if (mat_stack_p) model = mat_stack[mat_stack_p - 1];
    return model;
}

VkCommandBuffer get_current_cmd_buff()
{
    return ctx.device.cmd_buffs[0];
}

void set_viewport_scissor()
{
    r_cmd_set_viewport_scissor(ctx.device.cmd_buffs[0], ctx.swapchain.extent);
}

void wait_idle()
{
    vkQueueWaitIdle(ctx.device.queue);
}

Vector3 get_camera_forward(Camera *camera)
{
    return Vector3Normalize(Vector3Subtract(camera->target, camera->position));
}

Vector3 get_camera_up(Camera *camera)
{
    return Vector3Normalize(camera->up);
}

Vector3 get_camera_right(Camera *camera)
{
    Vector3 forward = get_camera_forward(camera);
    Vector3 up = get_camera_up(camera);
    return Vector3CrossProduct(forward, up);
}

void camera_move_forward(Camera *camera, float distance)
{
    Vector3 forward = get_camera_forward(camera);
    forward = Vector3Scale(forward, distance);
    camera->position = Vector3Add(camera->position, forward);
    camera->target = Vector3Add(camera->target, forward);
}

void camera_move_right(Camera *camera, float distance)
{
    Vector3 right = get_camera_right(camera);
    right = Vector3Scale(right, distance);
    camera->position = Vector3Add(camera->position, right);
    camera->target = Vector3Add(camera->target, right);
}

void camera_move_up(Camera *camera, float distance)
{
    Vector3 up = get_camera_up(camera);
    up = Vector3Scale(up, distance);
    camera->position = Vector3Add(camera->position, up);
    camera->target = Vector3Add(camera->target, up);
}

Vector2 get_mouse_delta()
{
    Vector2 delta = {
        .x = mouse.curr_pos.x - mouse.prev_pos.x,
        .y = mouse.curr_pos.y - mouse.prev_pos.y
    };
    return delta;
}

void camera_yaw(Camera *camera, float angle)
{
    Vector3 up = get_camera_up(camera);
    Vector3 target_pos = Vector3Subtract(camera->target, camera->position);
    target_pos = Vector3RotateByAxisAngle(target_pos, up, angle);
    camera->target = Vector3Add(camera->position, target_pos);
}

void camera_roll(Camera *camera, float angle)
{
    Vector3 forward = get_camera_forward(camera);
    camera->up = Vector3RotateByAxisAngle(camera->up, forward, angle);
}

void camera_pitch(Camera *camera, float angle)
{
    Vector3 right = get_camera_right(camera);
    Vector3 target_pos = Vector3Subtract(camera->target, camera->position);
    target_pos = Vector3RotateByAxisAngle(target_pos, right, angle);
    camera->target = Vector3Add(camera->position, target_pos);
}

void camera_move_to_target(Camera *camera, float delta)
{
    float distance = Vector3Distance(camera->position, camera->target);
    distance += delta;
    if (distance <= 0) distance = 0.001f;
    Vector3 forward = get_camera_forward(camera);
    camera->position = Vector3Add(camera->target, Vector3Scale(forward, -distance));
}

bool is_key_down(int key)
{
    bool down = false;

    if ((key > 0) && (key < MAX_KEYBOARD_KEYS)) {
        if (keyboard.curr_key_state[key] == 1) down = true;
    }

    return down;
}

bool is_gamepad_button_pressed(int button)
{
    bool pressed = false;

    if (button < MAX_GAMEPAD_BUTTONS) {
        if (gamepad.prev_button_state[button] == 0 && gamepad.curr_button_state[button] == 1)
            pressed = true;
    }

    return pressed;
}

bool is_gamepad_button_down(int button)
{
    bool pressed = false;

    if (button < MAX_GAMEPAD_BUTTONS) {
        if (gamepad.curr_button_state[button] == 1)
            pressed = true;
    }

    return pressed;
}

float get_gamepad_axis_movement(int axis)
{
    float value = 0;

    if (axis < MAX_GAMEPAD_AXIS && fabsf(gamepad.axis_state[axis]) > 0.1f)
        value = gamepad.axis_state[axis];

    return value;
}

int get_mouse_x()
{
    return (int)mouse.curr_pos.x;
}

int get_mouse_y()
{
    return (int)mouse.curr_pos.y;
}

bool is_mouse_button_down(int button)
{
    return mouse.curr_button_state[button] == 1;
}

float get_mouse_wheel_move()
{
    float result = 0.0f;

    if (fabsf(mouse.curr_wheel_move.x) > fabsf(mouse.curr_wheel_move.y)) result = (float)mouse.curr_wheel_move.x;
    else result = (float)mouse.curr_wheel_move.y;

    return result;
}

void update_camera_free(Camera *camera)
{
    Vector2 delta = get_mouse_delta();
    float dt = get_frame_time();

    if (is_mouse_button_down(MOUSE_BUTTON_RIGHT)) {
        camera_yaw(camera,   -delta.x*dt*CAMERA_MOUSE_MOVE_SENSITIVITY);
        camera_pitch(camera, -delta.y*dt*CAMERA_MOUSE_MOVE_SENSITIVITY);
    }

    float move_speed = CAMERA_MOVE_SPEED*dt;

    /* gamepad movement */
    float joy_x = get_gamepad_axis_movement(GAMEPAD_AXIS_RIGHT_X);
    float joy_y = get_gamepad_axis_movement(GAMEPAD_AXIS_RIGHT_Y);
    float joy_x_norm = (fabsf(joy_x) - DEAD_ZONE) / (1.0f - DEAD_ZONE);
    float joy_y_norm = (fabsf(joy_y) - DEAD_ZONE) / (1.0f - DEAD_ZONE);
    if (joy_x >  DEAD_ZONE) camera_yaw(camera,  -joy_x_norm * dt * GAMEPAD_ROT_SENSITIVITY);
    if (joy_y >  DEAD_ZONE) camera_pitch(camera,-joy_y_norm * dt * GAMEPAD_ROT_SENSITIVITY);
    if (joy_x < -DEAD_ZONE) camera_yaw(camera,   joy_x_norm * dt * GAMEPAD_ROT_SENSITIVITY);
    if (joy_y < -DEAD_ZONE) camera_pitch(camera, joy_y_norm * dt * GAMEPAD_ROT_SENSITIVITY);
    float fb = get_gamepad_axis_movement(GAMEPAD_AXIS_LEFT_Y);
    float lr = get_gamepad_axis_movement(GAMEPAD_AXIS_LEFT_X);
    float fb_norm = (fabsf(fb) - DEAD_ZONE) / (1.0f - DEAD_ZONE);
    float lr_norm = (fabsf(lr) - DEAD_ZONE) / (1.0f - DEAD_ZONE);
    if (fb <= -DEAD_ZONE) camera_move_forward(camera,  move_speed * fb_norm);
    if (lr <= -DEAD_ZONE) camera_move_right(camera,   -move_speed * lr_norm);
    if (fb >=  DEAD_ZONE) camera_move_forward(camera, -move_speed * fb_norm);
    if (lr >=  DEAD_ZONE) camera_move_right(camera,    move_speed * lr_norm);
    if (is_gamepad_button_down(GAMEPAD_BUTTON_RIGHT_TRIGGER_2)) camera_move_up(camera, move_speed / 2.0f);
    if (is_gamepad_button_down(GAMEPAD_BUTTON_LEFT_TRIGGER_2))  camera_move_up(camera, -move_speed / 2.0f);

    /* keyboard movement */
    if (is_key_down(KEY_LEFT_SHIFT)) move_speed *= 10.0f;
    if (is_key_down(KEY_W)) camera_move_forward(camera,  move_speed);
    if (is_key_down(KEY_A)) camera_move_right(camera,   -move_speed);
    if (is_key_down(KEY_S)) camera_move_forward(camera, -move_speed);
    if (is_key_down(KEY_D)) camera_move_right(camera,    move_speed);
    if (is_key_down(KEY_E)) camera_move_up(camera,  move_speed);
    if (is_key_down(KEY_Q)) camera_move_up(camera, -move_speed);
    if (is_key_down(KEY_LEFT))  camera_roll(camera, -CAMERA_ROT_SENSITIVITY * dt);
    if (is_key_down(KEY_RIGHT)) camera_roll(camera,  CAMERA_ROT_SENSITIVITY * dt);
}

void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
    (void)scancode;
    if (key < 0) return;

    switch (action) {
    case GLFW_RELEASE: keyboard.curr_key_state[key] = 0; break;
    case GLFW_PRESS: keyboard.curr_key_state[key] = 1; break;
    case GLFW_REPEAT: keyboard.key_repeat_in_frame[key] = 1; break;
    }

    if (((key == KEY_CAPS_LOCK) && ((mods & GLFW_MOD_CAPS_LOCK) > 0)) ||
        ((key == KEY_NUM_LOCK) && ((mods & GLFW_MOD_NUM_LOCK) > 0))) keyboard.curr_key_state[key] = 1;

    if ((keyboard.key_pressed_queue_count < MAX_KEY_PRESSED_QUEUE) && (action == GLFW_PRESS)) {
        keyboard.key_pressed_queue[keyboard.key_pressed_queue_count] = key;
        keyboard.key_pressed_queue_count++;
    }

    if ((key == keyboard.exit_key) && (action == GLFW_PRESS)) glfwSetWindowShouldClose(window, GLFW_TRUE);
}

void poll_input_events()
{
    /* keyboard */
    keyboard.key_pressed_queue_count = 0;
    keyboard.char_pressed_queue_count = 0;
    for (int i = 0; i < MAX_KEYBOARD_KEYS; i++) {
        keyboard.prev_key_state[i] = keyboard.curr_key_state[i];
        keyboard.key_repeat_in_frame[i] = 0;
    }

    /* mouse */
    mouse.prev_pos = mouse.curr_pos;
    for (int i = 0; i < MAX_MOUSE_BUTTONS; i++)
        mouse.prev_button_state[i] = mouse.curr_button_state[i];
    mouse.prev_wheel_move = mouse.curr_wheel_move;
    mouse.curr_wheel_move = (Vector2){ 0.0f, 0.0f };

    /* gamepad */
    GLFWgamepadstate state = {0};
    glfwGetGamepadState(0, &state); // 0 means only one controller is supported

    /* gamepad buttons */
    for (int i = 0; i < MAX_GAMEPAD_BUTTONS; i++)
        gamepad.prev_button_state[i] = gamepad.curr_button_state[i];
    const unsigned char *buttons = state.buttons;
    for (int i = 0; buttons != NULL && i < GLFW_GAMEPAD_BUTTON_DPAD_LEFT + 1 && i < MAX_GAMEPAD_BUTTONS; i++) {
        int btn = -1;

        switch (i) {
        case GLFW_GAMEPAD_BUTTON_Y:            btn = GAMEPAD_BUTTON_RIGHT_FACE_UP;    break;
        case GLFW_GAMEPAD_BUTTON_B:            btn = GAMEPAD_BUTTON_RIGHT_FACE_RIGHT; break;
        case GLFW_GAMEPAD_BUTTON_A:            btn = GAMEPAD_BUTTON_RIGHT_FACE_DOWN;  break;
        case GLFW_GAMEPAD_BUTTON_X:            btn = GAMEPAD_BUTTON_RIGHT_FACE_LEFT;  break;
        case GLFW_GAMEPAD_BUTTON_LEFT_BUMPER:  btn = GAMEPAD_BUTTON_LEFT_TRIGGER_1;   break;
        case GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER: btn = GAMEPAD_BUTTON_RIGHT_TRIGGER_1;  break;
        case GLFW_GAMEPAD_BUTTON_BACK:         btn = GAMEPAD_BUTTON_MIDDLE_LEFT;      break;
        case GLFW_GAMEPAD_BUTTON_GUIDE:        btn = GAMEPAD_BUTTON_MIDDLE;           break;
        case GLFW_GAMEPAD_BUTTON_START:        btn = GAMEPAD_BUTTON_MIDDLE_RIGHT;     break;
        case GLFW_GAMEPAD_BUTTON_DPAD_UP:      btn = GAMEPAD_BUTTON_LEFT_FACE_UP;     break;
        case GLFW_GAMEPAD_BUTTON_DPAD_RIGHT:   btn = GAMEPAD_BUTTON_LEFT_FACE_RIGHT;  break;
        case GLFW_GAMEPAD_BUTTON_DPAD_DOWN:    btn = GAMEPAD_BUTTON_LEFT_FACE_DOWN;   break;
        case GLFW_GAMEPAD_BUTTON_DPAD_LEFT:    btn = GAMEPAD_BUTTON_LEFT_FACE_LEFT;   break;
        case GLFW_GAMEPAD_BUTTON_LEFT_THUMB:   btn = GAMEPAD_BUTTON_LEFT_THUMB;       break;
        case GLFW_GAMEPAD_BUTTON_RIGHT_THUMB:  btn = GAMEPAD_BUTTON_RIGHT_THUMB;      break;
        default: break;
        }

        if (btn != -1) {
            if (buttons[i] == GLFW_PRESS) {
                gamepad.curr_button_state[btn] = 1;
                gamepad.last_button_pressed = btn;
            } else {
                gamepad.curr_button_state[btn] = 0;
            }
        }
    }

    /* gamepad axes */
    const float *axes = state.axes;
    for (int i = 0; axes != NULL && i < GLFW_GAMEPAD_AXIS_LAST + 1 && i < MAX_GAMEPAD_AXIS; i++)
        gamepad.axis_state[i] = axes[i];

    /* if we want to treat trigger buttons as booleans */
    gamepad.curr_button_state[GAMEPAD_BUTTON_LEFT_TRIGGER_2]  = gamepad.axis_state[GAMEPAD_AXIS_LEFT_TRIGGER] > 0.1f;
    gamepad.curr_button_state[GAMEPAD_BUTTON_RIGHT_TRIGGER_2] = gamepad.axis_state[GAMEPAD_AXIS_RIGHT_TRIGGER] > 0.1f;

    // glfwPollEvents();
}

void mouse_scroll_callback(GLFWwindow *window, double x_offset, double y_offset)
{
    (void)window;
    Vector2 offsets = {.x = x_offset, .y = y_offset};
    mouse.curr_wheel_move = offsets;
}

void mouse_button_callback(GLFWwindow *window, int button, int action, int mods)
{
    (void) window;
    (void) mods;
    mouse.curr_button_state[button] = action;
}

void mouse_cursor_pos_callback(GLFWwindow *window, double x, double y)
{
    (void) window;

    mouse.curr_pos.x = x;
    mouse.curr_pos.y = y;
}

bool is_key_pressed(int key)
{
    bool pressed = false;

    if ((key > 0) && (key < MAX_KEYBOARD_KEYS)) {
        if ((keyboard.prev_key_state[key] == 0) && (keyboard.curr_key_state[key] == 1))
            pressed = true;
    }

    return pressed;
}

void scale(float x, float y, float z)
{
    if (mat_stack_p > 0)
        mat_stack[mat_stack_p - 1] = MatrixMultiply(MatrixScale(x, y, z), mat_stack[mat_stack_p - 1]);
    else
        r_log(RVK_ERROR, "no matrix available to scale");
}

void translate(float x, float y, float z)
{
    if (mat_stack_p > 0)
        mat_stack[mat_stack_p - 1] = MatrixMultiply(MatrixTranslate(x, y, z), mat_stack[mat_stack_p - 1]);
    else
        r_log(RVK_ERROR, "no matrix available to translate");
}

void matrix_cat(Matrix m)
{
    if (mat_stack_p > 0)
        mat_stack[mat_stack_p - 1] = MatrixMultiply(m, mat_stack[mat_stack_p - 1]);
    else
        r_log(RVK_ERROR, "no matrix available to concantenate");
}

int get_fps()
{
    double frame_time = get_frame_time();
    if (frame_time == 0) return 0;
    else return (int)roundf(1.0f / frame_time);
}

int get_avg_fps()
{
    int fps = 0;

    static int index = 0;
    static double history[FPS_CAPTURE_FRAMES_COUNT] = {0};
    static double average = 0, last = 0;
    double fps_frame = get_frame_time();

    if (frm_ctx.time.frame_count == 0) {
        average = 0;
        last = 0;
        index = 0;

        for (int i = 0; i < FPS_CAPTURE_FRAMES_COUNT; i++) history[i] = 0;
    }

    if (fps_frame == 0) return 0;

    if ((get_time() - last) > FPS_STEP)
    {
        last = get_time();
        index = (index + 1) % FPS_CAPTURE_FRAMES_COUNT;
        average -= history[index];
        history[index] = fps_frame / FPS_CAPTURE_FRAMES_COUNT;
        average += history[index];
    }

    fps = (int)roundf((float)1.0f/average);

    return fps;
}

float get_avg_frame_time()
{
    static int index = 0;
    static double history[FPS_CAPTURE_FRAMES_COUNT] = {0};
    static double average = 0, last = 0;
    double fps_frame = get_frame_time();

    if (frm_ctx.time.frame_count == 0) {
        average = 0;
        last = 0;
        index = 0;

        for (int i = 0; i < FPS_CAPTURE_FRAMES_COUNT; i++) history[i] = 0;
    }

    if (fps_frame == 0) return 0;

    if ((get_time() - last) > FPS_STEP)
    {
        last = get_time();
        index = (index + 1) % FPS_CAPTURE_FRAMES_COUNT;
        average -= history[index];
        history[index] = fps_frame / FPS_CAPTURE_FRAMES_COUNT;
        average += history[index];
    }

    return average;
}

void log_fps()
{
    static int fps = -1;
    int curr_fps = get_fps();
    if (curr_fps != fps) {
        printf("FPS: %d (%fms)\n", curr_fps, get_frame_time() * 1000.0f);
        fps = curr_fps;
    }
}

Frame_Context get_frame_context()
{
    return frm_ctx;
}

VkFramebuffer get_current_frame_buffer()
{
    return ctx.swapchain.framebuffers[frm_ctx.img_idx];
}

Font load_font(const char *file_path, int font_height)
{
    bool result = true;

    Font font = {
        .bitmap_width = DEFAULT_BITMAP_WIDTH,
        .bitmap_height = DEFAULT_BITMAP_HEIGHT,
        .bitmap = malloc(DEFAULT_BITMAP_WIDTH*DEFAULT_BITMAP_HEIGHT),
        .height = font_height,
    };

    String_Builder sb = {0};
    if (!read_entire_file(file_path, &sb)) return_defer(false);
    int res = stbtt_BakeFontBitmap((unsigned char *)sb.items, 0, font.height, font.bitmap,
                                   font.bitmap_width, font.bitmap_height, FIRST_CHAR, CHAR_COUNT,
                                   (stbtt_bakedchar*)font.glyphs);
    if (res <= 0) {
        printf("ERROR: unable to fully bake font bitmap (return code: %d)\n", res);
        printf("       if return is negative, returns the negative of the number of characters that fit\n");
        printf("       if return is 0, no characters fit and no rows were used\n");
        printf("       try increasing DEFAULT_BITMAP_WIDTH/HEIGHT in creese_2D.c\n");
        return_defer(false);
    }

defer:
    sb_free(sb);
    if (!result) free(font.bitmap);
    return font;
}

void unload_font(Font font)
{
    free(font.bitmap);
}

uint32_t color_to_uint32_t(Color color)
{
    uint32_t r = color.r;
    uint32_t g = color.g;
    uint32_t b = color.b;
    uint32_t a = color.a;
    return a << 24 | b << 16 | g << 8 | r;
}

Color color_from_hsv(float hue, float saturation, float value)
{
    Color color = { 0, 0, 0, 255 };

    // Red channel
    float k = fmodf((5.0f + hue/60.0f), 6);
    float t = 4.0f - k;
    k = (t < k)? t : k;
    k = (k < 1)? k : 1;
    k = (k > 0)? k : 0;
    color.r = (unsigned char)((value - value*saturation*k)*255.0f);

    // Green channel
    k = fmodf((3.0f + hue/60.0f), 6);
    t = 4.0f - k;
    k = (t < k)? t : k;
    k = (k < 1)? k : 1;
    k = (k > 0)? k : 0;
    color.g = (unsigned char)((value - value*saturation*k)*255.0f);

    // Blue channel
    k = fmodf((1.0f + hue/60.0f), 6);
    t = 4.0f - k;
    k = (t < k)? t : k;
    k = (k < 1)? k : 1;
    k = (k > 0)? k : 0;
    color.b = (unsigned char)((value - value*saturation*k)*255.0f);

    return color;
}

VkPipelineShaderStageCreateInfo load_compute_shader(const char *file_path, String_Builder *sb)
{
    VkPipelineShaderStageCreateInfo ci = {0};

    sb->count = 0;
    if (read_entire_file(file_path, sb))
        ci = r_create_compute_shader_stage_ci(ctx.device.logical, sb->count, (uint32_t*)sb->items);

    return ci;
}

void unload_shader(VkPipelineShaderStageCreateInfo ci)
{
    vkDestroyShaderModule(ctx.device.logical, ci.module, NULL);
}

void destroy_pipeline(Rvk_Pipeline pipeline)
{
    vkDestroyPipeline(ctx.device.logical, pipeline.handle, NULL);
    vkDestroyPipelineLayout(ctx.device.logical, pipeline.layout, NULL);
}

void setup_display_ds_layout()
{
    VkDescriptorSetLayoutBinding bindings[] = {
        {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        },
    };
    vk_create_descriptor_set_layout(ctx.device.logical, NULL, &standard.ds_layouts[DS_LAYOUT_DISPLAY],
                                    .pBindings = bindings,
                                    .bindingCount = ARRAY_LEN(bindings));
}

void create_display_pipeline()
{
    VkPipelineShaderStageCreateInfo stages[2];
    String_Builder sb = {0};

    assert(standard.ds_layouts[DS_LAYOUT_DISPLAY]);
    vk_create_pipeline_layout(ctx.device.logical,
                              NULL,
                              &standard.display.pl.layout,
                              .pSetLayouts = &standard.ds_layouts[DS_LAYOUT_DISPLAY],
                              .setLayoutCount = 1);

    /* load shaders */
    read_entire_file("shaders/standard_display.vert.glsl.spv", &sb);
    stages[0] = r_create_vertex_stage_ci(ctx.device.logical, sb.count, (uint32_t*)sb.items);
    assert(stages[0].module);
    sb.count = 0; // reuse memory
    read_entire_file("shaders/standard_display.frag.glsl.spv", &sb);
    stages[1] = r_create_fragment_stage_ci(ctx.device.logical, sb.count, (uint32_t*)sb.items);
    assert(stages[1].module);
    sb.count = 0;

    /* temporary allocator */
    size_t temp_alloc_save_point = r_temp_save();
    vk_create_graphics_pipeline(ctx.device.logical, NULL, NULL, &standard.display.pl.handle,
                                .stageCount = ARRAY_LEN(stages),
                                .pStages = stages,
                                .pVertexInputState = r_temp_default_empty_input_state_ci(),
                                .pInputAssemblyState = r_temp_default_input_assembly_state_ci(),
                                .pViewportState = r_temp_default_viewport_state_ci(ctx.swapchain.extent),
                                .pRasterizationState = r_temp_default_rasterization_state_ci(),
                                .pMultisampleState = r_temp_default_multisample_state_ci(),
                                .pDepthStencilState = r_temp_default_depth_stencil_state_ci(),
                                .pColorBlendState = r_temp_default_color_blend_state_ci(),
                                .pDynamicState = r_temp_default_dynamic_state_ci(),
                                .layout = standard.display.pl.layout,
                                .renderPass = ctx.swapchain.render_pass);
    r_temp_rewind(temp_alloc_save_point);

    vkDestroyShaderModule(ctx.device.logical, stages[0].module, NULL);
    vkDestroyShaderModule(ctx.device.logical, stages[1].module, NULL);
    sb_free(sb);
}

void create_storage_image(int width, int height)
{
    VkExtent2D extent = {width, height};
    standard.display_texture.image = r_create_2D_image(ctx.device.logical, VK_FORMAT_R8G8B8A8_UNORM,
                                                      VK_IMAGE_USAGE_STORAGE_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,
                                                      extent);
    assert(standard.display_texture.image);
    standard.display_texture.memory = r_allocate_and_bind_image_memory(ctx.device,
                                                                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                                                      standard.display_texture.image);
    assert(standard.display_texture.memory);
    r_transition_image_layout(ctx.device, standard.display_texture.image,
                              VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

    vk_create_image_view(ctx.device.logical, NULL, &standard.display_texture.info.imageView,
                         .image = standard.display_texture.image,
                         .viewType = VK_IMAGE_VIEW_TYPE_2D,
                         .format = VK_FORMAT_R8G8B8A8_UNORM,
                         .subresourceRange = {
                                  .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                  .levelCount = 1, .layerCount = 1,});
    vk_create_sampler(ctx.device.logical, NULL, &standard.display_texture.info.sampler,
                      .compareOp = VK_COMPARE_OP_ALWAYS,
                      .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK);

    /* book keeping */
    standard.display_texture.info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
}

void init_display_ds()
{
    assert(standard.ds_layouts[DS_LAYOUT_DISPLAY]);
    assert(standard.display_texture.image);

    vk_allocate_descriptor_sets(ctx.device.logical, &standard.display.ds,
                                .descriptorPool = ctx.pool,
                                .descriptorSetCount = 1,
                                .pSetLayouts = &standard.ds_layouts[DS_LAYOUT_DISPLAY]);
    VkWriteDescriptorSet write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstBinding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .dstSet = standard.display.ds,
        .pImageInfo = &standard.display_texture.info,
    };
    vkUpdateDescriptorSets(ctx.device.logical, 1, &write, 0, NULL);
}

void setup_standard_ds_layouts()
{
    /* standard_clear_background.comp.glsl */
    VkDescriptorSetLayoutBinding clear_background_bindings[] = {
        {
            .binding         = 0,
            .descriptorCount = 1,
            .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT,
        },
    };
    vk_create_descriptor_set_layout(ctx.device.logical, NULL, &standard.ds_layouts[DS_LAYOUT_CLEAR_BACKGROUND],
                                    .pBindings = clear_background_bindings,
                                    .bindingCount = ARRAY_LEN(clear_background_bindings));

    /* standard_point_render.comp.glsl */
    VkDescriptorSetLayoutBinding render_bindings[] = {
        {
            .binding         = 0,
            .descriptorCount = 1,
            .descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT,
        },
        {
            .binding         = 1,
            .descriptorCount = 1,
            .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT,
        },
        {
            .binding         = 2,
            .descriptorCount = 1,
            .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT,
        },
    };
    vk_create_descriptor_set_layout(ctx.device.logical, NULL, &standard.ds_layouts[DS_LAYOUT_RENDER],
                                    .pBindings = render_bindings,
                                    .bindingCount = ARRAY_LEN(render_bindings));

    /* standard_point_resolve.comp.glsl */
    VkDescriptorSetLayoutBinding resolve_bindings[] = {
        {
            .binding         = 0,
            .descriptorCount = 1,
            .descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT,
        },
        {
            .binding         = 1,
            .descriptorCount = 1,
            .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT,
        },
        {
            .binding         = 2,
            .descriptorCount = 1,
            .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT,
        },
    };
    vk_create_descriptor_set_layout(ctx.device.logical, NULL, &standard.ds_layouts[DS_LAYOUT_RESOLVE],
                                    .pBindings = resolve_bindings,
                                    .bindingCount = ARRAY_LEN(resolve_bindings));


    /* standard_text.comp.glsl */
    VkDescriptorSetLayoutBinding text_bindings[] = {
        {
            .binding         = 0,
            .descriptorCount = 1,
            .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT,
        },
        {
            .binding         = 1,
            .descriptorCount = 1,
            .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT,
        },
    };
    vk_create_descriptor_set_layout(ctx.device.logical, NULL, &standard.ds_layouts[DS_LAYOUT_TEXT],
                                    .pBindings = text_bindings,
                                    .bindingCount = ARRAY_LEN(text_bindings));

    /* hidden_surface_removal.comp.glsl */
    VkDescriptorSetLayoutBinding hidden_surface_bindings[] = {
        {
            .binding         = 0,
            .descriptorCount = 1,
            .descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT,
        },
        {
            .binding         = 1,
            .descriptorCount = 1,
            .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT,
        },
        {
            .binding         = 2,
            .descriptorCount = 1,
            .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT,
        },
    };
    vk_create_descriptor_set_layout(ctx.device.logical, NULL, &standard.ds_layouts[DS_LAYOUT_HIDDEN_SURFACE_REMOVAL],
                                    .pBindings = hidden_surface_bindings,
                                    .bindingCount = ARRAY_LEN(hidden_surface_bindings));

    /* standard_point_hole_filling.comp.glsl */
    VkDescriptorSetLayoutBinding hole_filling_bindings[] = {
        {
            .binding         = 0,
            .descriptorCount = 1,
            .descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT,
        },
        {
            .binding         = 1,
            .descriptorCount = 1,
            .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT,
        },
        {
            .binding         = 2,
            .descriptorCount = 1,
            .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT,
        },
    };
    vk_create_descriptor_set_layout(ctx.device.logical, NULL, &standard.ds_layouts[DS_LAYOUT_HOLE_FILLING],
                                    .pBindings = hole_filling_bindings,
                                    .bindingCount = ARRAY_LEN(hole_filling_bindings));

    setup_display_ds_layout();
}

struct {
    uint32_t batch_offset;
    uint32_t base_offset;
    uint32_t point_count;
} point_render_push_const = {0};

struct {
    int x;
    int y;
    int x0;
    int y0;
    int x1;
    int y1;
    uint32_t color;
    int frame_width;
    int frame_height;
    int bitmap_width;
    int bitmap_height;
} text_push_const;

void create_standard_compute_pipelines()
{
    VkPipelineCache cache = VK_NULL_HANDLE;
    String_Builder sb = {0};
    VkPushConstantRange pc_range = {
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        .size = sizeof(point_render_push_const)
    };

    /* standard_point_render.comp.glsl pipeline */
    vk_create_pipeline_layout(ctx.device.logical, NULL, &standard.render.pl.layout,
                              .setLayoutCount = 1,
                              .pSetLayouts = &standard.ds_layouts[DS_LAYOUT_RENDER],
                              .pushConstantRangeCount = 1,
                              .pPushConstantRanges = &pc_range);

    VkPipelineShaderStageCreateInfo render = load_compute_shader("shaders/standard_point_render.comp.glsl.spv", &sb);
    assert(render.module);
    vk_create_compute_pipeline(ctx.device.logical, cache, NULL, &standard.render.pl.handle,
                               .layout = standard.render.pl.layout,
                               .stage = render);

    /* standard_clear_background.comp.glsl pipeline */
    pc_range.size = sizeof(clear_background_push_const);
    vk_create_pipeline_layout(ctx.device.logical, NULL, &standard.clear_background.pl.layout,
                              .setLayoutCount = 1,
                              .pSetLayouts = &standard.ds_layouts[DS_LAYOUT_CLEAR_BACKGROUND],
                              .pushConstantRangeCount = 1,
                              .pPushConstantRanges = &pc_range);
    VkPipelineShaderStageCreateInfo clear_bg = load_compute_shader("shaders/standard_clear_background.comp.glsl.spv", &sb);
    assert(clear_bg.module);
    vk_create_compute_pipeline(ctx.device.logical, cache, NULL, &standard.clear_background.pl.handle,
                               .layout = standard.clear_background.pl.layout,
                               .stage = clear_bg);

    /* standard_point_resolve.comp.glsl pipeline */
    vk_create_pipeline_layout(ctx.device.logical, NULL, &standard.resolve.pl.layout,
                              .setLayoutCount = 1,
                              .pSetLayouts = &standard.ds_layouts[DS_LAYOUT_RESOLVE]);
    VkPipelineShaderStageCreateInfo resolve = load_compute_shader("shaders/standard_point_resolve.comp.glsl.spv", &sb);
    assert(resolve.module);
    vk_create_compute_pipeline(ctx.device.logical, cache, NULL, &standard.resolve.pl.handle,
                               .layout = standard.resolve.pl.layout,
                               .stage = resolve);

    /* standard_text.comp.glsl pipeline */
    assert(standard.ds_layouts[DS_LAYOUT_TEXT]);
    pc_range.size = sizeof(text_push_const);
    vk_create_pipeline_layout(ctx.device.logical, NULL, &standard.text.pl.layout,
                              .setLayoutCount = 1,
                              .pSetLayouts = &standard.ds_layouts[DS_LAYOUT_TEXT],
                              .pushConstantRangeCount = 1,
                              .pPushConstantRanges = &pc_range);
    VkPipelineShaderStageCreateInfo text = load_compute_shader("shaders/standard_text.comp.glsl.spv", &sb);
    assert(text.module);
    vk_create_compute_pipeline(ctx.device.logical, cache, NULL, &standard.text.pl.handle,
                               .layout = standard.text.pl.layout,
                               .stage = text);

    /* hidden_surface.comp.glsl pipeline */ // TODO: maybe only initialize if needed
    assert(standard.ds_layouts[DS_LAYOUT_HIDDEN_SURFACE_REMOVAL]);
    vk_create_pipeline_layout(ctx.device.logical, NULL, &standard.hidden_surface.pl.layout,
                              .setLayoutCount = 1,
                              .pSetLayouts = &standard.ds_layouts[DS_LAYOUT_HIDDEN_SURFACE_REMOVAL]);
    VkPipelineShaderStageCreateInfo hidden_surface = load_compute_shader("shaders/standard_point_hidden_surface_removal.comp.glsl.spv", &sb);
    assert(hidden_surface.module);
    vk_create_compute_pipeline(ctx.device.logical, cache, NULL, &standard.hidden_surface.pl.handle,
                               .layout = standard.hidden_surface.pl.layout,
                               .stage = hidden_surface);

    /* hole_filling.comp.glsl pipeline */ // TODO: maybe only initialize if needed
    assert(standard.ds_layouts[DS_LAYOUT_HIDDEN_SURFACE_REMOVAL]);
    vk_create_pipeline_layout(ctx.device.logical, NULL, &standard.hole_filling.pl.layout,
                              .setLayoutCount = 1,
                              .pSetLayouts = &standard.ds_layouts[DS_LAYOUT_HOLE_FILLING]);
    VkPipelineShaderStageCreateInfo hole_filling = load_compute_shader("shaders/standard_point_hole_filling.comp.glsl.spv", &sb);
    assert(hole_filling.module);
    vk_create_compute_pipeline(ctx.device.logical, cache, NULL, &standard.hole_filling.pl.handle,
                               .layout = standard.hole_filling.pl.layout,
                               .stage = hole_filling);

    create_display_pipeline();

    unload_shader(render);
    unload_shader(hole_filling);
    unload_shader(hidden_surface);
    unload_shader(resolve);
    unload_shader(clear_bg);
    unload_shader(text);
    sb_free(sb);
}

Rvk_Buffer create_software_frame_buffer(int width, int height)
{
    assert(width > 0);
    assert(height > 0);
    size_t size = width*height*sizeof(uint64_t);
    return r_create_compute_buffer(ctx.device, size);
}

void update_point_render_ds(Rvk_Buffer point_buffer, VkDescriptorSet ds)
{
    if (!standard.uniform_buff.info.buffer) {
        standard.uniform_buff = r_create_mapped_uniform_buffer(ctx.device, sizeof(standard.uniform_data));
    }
    if (!standard.frame_buffers[0].info.buffer) {
        standard.frame_buffers[0] = create_software_frame_buffer(window.width, window.height);
    }
    assert(point_buffer.info.buffer);

    VkWriteDescriptorSet writes[] = {
        /* render.comp.glsl */
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &standard.uniform_buff.info,
            .dstSet = ds,
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstBinding = 1,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &point_buffer.info,
            .dstSet = ds,
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstBinding = 2,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &standard.frame_buffers[0].info,
            .dstSet = ds,
        },
    };
    vkUpdateDescriptorSets(ctx.device.logical, ARRAY_LEN(writes), writes, 0, NULL);
}

void init_clear_background_ds()
{
    assert(standard.frame_buffers[0].info.buffer);

    vk_allocate_descriptor_sets(ctx.device.logical, &standard.clear_background.ds,
                                .descriptorPool = ctx.pool,
                                .descriptorSetCount = 1,
                                .pSetLayouts = &standard.ds_layouts[DS_LAYOUT_CLEAR_BACKGROUND]);
    VkWriteDescriptorSet writes[] = {
        /* standard_clear_background.comp.glsl */
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &standard.frame_buffers[0].info,
            .dstSet = standard.clear_background.ds,
        },
    };
    vkUpdateDescriptorSets(ctx.device.logical, ARRAY_LEN(writes), writes, 0, NULL);
}

void init_resolve_ds()
{
    if (!standard.uniform_buff.info.buffer)
        standard.uniform_buff = r_create_mapped_uniform_buffer(ctx.device, sizeof(standard.uniform_data));
    if (!standard.frame_buffers[0].info.buffer)
        standard.frame_buffers[0] = create_software_frame_buffer(window.width, window.height);

    vk_allocate_descriptor_sets(ctx.device.logical, &standard.resolve.ds,
            .descriptorPool = ctx.pool,
            .descriptorSetCount = 1,
            .pSetLayouts = &standard.ds_layouts[DS_LAYOUT_RESOLVE]);
    VkWriteDescriptorSet writes[] = {
        /* standard_point_resolve.comp.glsl */
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &standard.uniform_buff.info,
            .dstSet = standard.resolve.ds,
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstBinding = 1,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &standard.frame_buffers[0].info,
            .dstSet = standard.resolve.ds,
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstBinding = 2,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo = &standard.display_texture.info,
            .dstSet = standard.resolve.ds,
        },
    };
    vkUpdateDescriptorSets(ctx.device.logical, ARRAY_LEN(writes), writes, 0, NULL);
}

void init_hidden_surface_ds()
{
    if (!standard.uniform_buff.info.buffer)
        standard.uniform_buff = r_create_mapped_uniform_buffer(ctx.device, sizeof(standard.uniform_data));
    for (size_t i = 0; i < FRAME_BUFFER_COUNT; i++) {
        if (!standard.frame_buffers[i].info.buffer)
            standard.frame_buffers[i] = create_software_frame_buffer(window.width, window.height);
    }

    vk_allocate_descriptor_sets(ctx.device.logical, &standard.hidden_surface.ds,
            .descriptorPool = ctx.pool,
            .descriptorSetCount = 1,
            .pSetLayouts = &standard.ds_layouts[DS_LAYOUT_HIDDEN_SURFACE_REMOVAL]);
    VkWriteDescriptorSet writes[] = {
        /* standard_point_hidden_surface.comp.glsl */
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &standard.uniform_buff.info,
            .dstSet = standard.hidden_surface.ds,
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstBinding = 1,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &standard.frame_buffers[0].info,
            .dstSet = standard.hidden_surface.ds,
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstBinding = 2,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &standard.frame_buffers[1].info,
            .dstSet = standard.hidden_surface.ds,
        },
    };
    vkUpdateDescriptorSets(ctx.device.logical, ARRAY_LEN(writes), writes, 0, NULL);
}

void init_hole_filling_ds()
{
    if (!standard.uniform_buff.info.buffer)
        standard.uniform_buff = r_create_mapped_uniform_buffer(ctx.device, sizeof(standard.uniform_data));
    for (size_t i = 0; i < FRAME_BUFFER_COUNT; i++) {
        if (!standard.frame_buffers[i].info.buffer)
            standard.frame_buffers[i] = create_software_frame_buffer(window.width, window.height);
    }

    vk_allocate_descriptor_sets(ctx.device.logical, &standard.hole_filling.ds[0],
                                .descriptorPool = ctx.pool,
                                .descriptorSetCount = 1,
                                .pSetLayouts = &standard.ds_layouts[DS_LAYOUT_HOLE_FILLING]);
    vk_allocate_descriptor_sets(ctx.device.logical, &standard.hole_filling.ds[1],
                                .descriptorPool = ctx.pool,
                                .descriptorSetCount = 1,
                                .pSetLayouts = &standard.ds_layouts[DS_LAYOUT_HOLE_FILLING]);
    VkWriteDescriptorSet writes[] = {
        /* hole_filling.comp.glsl */
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &standard.uniform_buff.info,
            .dstSet = standard.hole_filling.ds[0],
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstBinding = 1,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &standard.frame_buffers[0].info,
            .dstSet = standard.hole_filling.ds[0],
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstBinding = 2,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &standard.frame_buffers[1].info,
            .dstSet = standard.hole_filling.ds[0],
        },
    };
    vkUpdateDescriptorSets(ctx.device.logical, ARRAY_LEN(writes), writes, 0, NULL);

    writes[0].dstSet = standard.hole_filling.ds[1];
    writes[1].dstSet = standard.hole_filling.ds[1];
    writes[2].dstSet = standard.hole_filling.ds[1];
    writes[1].pBufferInfo = &standard.frame_buffers[1].info;
    writes[2].pBufferInfo = &standard.frame_buffers[0].info;
    vkUpdateDescriptorSets(ctx.device.logical, ARRAY_LEN(writes), writes, 0, NULL);
}

void draw_points(VkDescriptorSet ds, size_t point_count, size_t point_offset)
{
    int group_x = 1, group_y = 1, group_z = 1;
    VkCommandBuffer cb = ctx.device.cmd_buffs[0];


    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, standard.render.pl.handle);
    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, standard.render.pl.layout, 0, 1, &ds, 0, NULL);

    size_t points_left = point_count;
    size_t batch_size = 0;
    int batch_offset = 0;
    while (points_left) {
        batch_size = (points_left > POINTS_PER_BATCH) ? POINTS_PER_BATCH : points_left;
        point_render_push_const.batch_offset = batch_offset;
        point_render_push_const.base_offset  = point_offset;
        point_render_push_const.point_count  = point_count;
        group_x = ceilf(batch_size/POINT_WORKGROUP_SIZE);
        batch_offset += batch_size;
        points_left  -= batch_size;
        vkCmdPushConstants(cb, standard.render.pl.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                           sizeof(point_render_push_const), &point_render_push_const);
        vkCmdDispatch(cb, group_x, group_y, group_z);
    }
}

void resolve_image()
{
    /* resolve only reads from fb0 */
    if (standard.last_buff_write_was_fb1) copy_fb1_to_fb0();

    /* ensure all writes to fb0 are finished before we start reading */
    comp_to_comp_buff_barrier(standard.frame_buffers[0].info.buffer);

    int group_x = 1, group_y = 1, group_z = 1;
    VkCommandBuffer cb = ctx.device.cmd_buffs[0];

    group_x = ceil(window.width/IMAGE_WORKGROUP_SIZE);
    group_y = ceil(window.height/IMAGE_WORKGROUP_SIZE);
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, standard.resolve.pl.handle);

    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, standard.resolve.pl.layout, 0, 1,
                            &standard.resolve.ds, 0, NULL);

    vkCmdDispatch(cb, group_x, group_y, group_z);
}

void point_hidden_surface_removal()
{
    int group_x = 1, group_y = 1, group_z = 1;
    VkCommandBuffer cb = ctx.device.cmd_buffs[0];

    /* ensure all writes to fb0 are finished before we start reading */
    comp_to_comp_buff_barrier(standard.frame_buffers[0].info.buffer);

    group_x = ceil(window.width/IMAGE_WORKGROUP_SIZE);
    group_y = ceil(window.height/IMAGE_WORKGROUP_SIZE);
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, standard.hidden_surface.pl.handle);
    assert(standard.hidden_surface.ds);
    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, standard.hidden_surface.pl.layout, 0, 1,
                            &standard.hidden_surface.ds, 0, NULL);
    vkCmdDispatch(cb, group_x, group_y, group_z);

    /* ensure all writes to fb1 are finished anything else starts reading from it */
    comp_to_comp_buff_barrier(standard.frame_buffers[1].info.buffer);

    /* since this shader copys frame buffer 0 into frame buffer 1, we need to eventually copy it back to 0 */
    standard.last_buff_write_was_fb1 = true;
}

#define HOLE_FILLING_ITERATIONS 3

void point_hole_filling()
{
    int group_x = 1, group_y = 1, group_z = 1;
    VkCommandBuffer cb = ctx.device.cmd_buffs[0];

    /* ensure all writes to fb0 are finished before we start reading */
    comp_to_comp_buff_barrier(standard.frame_buffers[0].info.buffer);

    group_x = ceil(window.width/IMAGE_WORKGROUP_SIZE);
    group_y = ceil(window.height/IMAGE_WORKGROUP_SIZE);
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, standard.hole_filling.pl.handle);
    assert(standard.hole_filling.ds[0]);
    assert(standard.hole_filling.ds[1]);

    for (int i = 0; i < HOLE_FILLING_ITERATIONS; i++) {
        int descriptor_set_idx = (standard.last_buff_write_was_fb1) ? 1 : 0;
        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, standard.hole_filling.pl.layout, 0, 1,
                                &standard.hole_filling.ds[descriptor_set_idx], 0, NULL);
        vkCmdDispatch(cb, group_x, group_y, group_z);

        if (standard.last_buff_write_was_fb1) {
            comp_to_comp_buff_barrier(standard.frame_buffers[0].info.buffer);
            standard.last_buff_write_was_fb1 = false;
        } else {
            comp_to_comp_buff_barrier(standard.frame_buffers[1].info.buffer);
            standard.last_buff_write_was_fb1 = true;
        }
    }
}

void compute_to_frag_sample_image_barrier(VkCommandBuffer cb, VkImage image)
{
    VkImageMemoryBarrier barrier = {
       .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
       .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
       .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
       .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
       .newLayout = VK_IMAGE_LAYOUT_GENERAL,
       .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
       .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
       .image = image,
       .subresourceRange = {
           .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
           .baseMipLevel = 0,
           .levelCount = 1,
           .baseArrayLayer = 0,
           .layerCount = 1,
       },
    };
    vkCmdPipelineBarrier(
        cb,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0,
        0, NULL,
        0, NULL,
        1, &barrier
    );
}

void comp_to_comp_buff_barrier(VkBuffer buffer)
{
    VkBufferMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .buffer = buffer,
        .size = VK_WHOLE_SIZE,
    };

    vkCmdPipelineBarrier(
        ctx.device.cmd_buffs[0],
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0,
        0, NULL,
        1, &barrier,
        0, NULL
    );
}

void alloc_point_render_ds(VkDescriptorSet *ds)
{
    vk_allocate_descriptor_sets(ctx.device.logical, ds,
                                .descriptorPool = ctx.pool,
                                .descriptorSetCount = 1,
                                .pSetLayouts = &standard.ds_layouts[DS_LAYOUT_RENDER]);
}

void init_standard_pipelines()
{
    create_storage_image(window.width, window.height);
    setup_standard_ds_layouts();
    create_standard_compute_pipelines();

    /* go ahead and allocate needed standard descriptor sets */
    init_display_ds();
    init_resolve_ds();
    init_hidden_surface_ds();
    init_hole_filling_ds();
    init_clear_background_ds();
}

Rvk_Buffer create_compute_buffer(size_t size, void *data)
{
    return r_create_compute_buffer_from_host(ctx.device, size, data);
}

void destroy_buffer(Rvk_Buffer buff)
{
    r_destroy_rvk_buffer(ctx.device.logical, buff);
}

void draw_text_at_base(Font font, const char *text, size_t text_len, int x, int y, Color color)
{
    /* draw text only reads/writes to/from fb0 */
    if (standard.last_buff_write_was_fb1) copy_fb1_to_fb0();

    /* ensure all writes to fb0 are finished before we start reading */
    comp_to_comp_buff_barrier(standard.frame_buffers[0].info.buffer);

    if (!standard.text.ds) {
        assert(standard.ds_layouts[DS_LAYOUT_TEXT]);
        vk_allocate_descriptor_sets(ctx.device.logical, &standard.text.ds,
                .descriptorPool = ctx.pool,
                .descriptorSetCount = 1,
                .pSetLayouts = &standard.ds_layouts[DS_LAYOUT_TEXT]);
        assert(standard.text.ds);
        if (standard.text.texture.image == NULL) {
            VkExtent2D extent = {.width = font.bitmap_width, .height = font.bitmap_height};
            standard.text.texture = r_create_texture(ctx.device, extent, font.bitmap, VK_FORMAT_R8_UNORM);
        }
        assert(standard.frame_buffers[0].info.buffer);
        VkWriteDescriptorSet writes[] = {
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstBinding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .dstSet = standard.text.ds,
                .pImageInfo = &standard.text.texture.info,
            },
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstBinding = 1,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .dstSet = standard.text.ds,
                .pBufferInfo = &standard.frame_buffers[0].info,
            },
        };
        vkUpdateDescriptorSets(ctx.device.logical, ARRAY_LEN(writes), writes, 0, NULL);
    }


    /* dispatch the text shader */
    int group_x = 1, group_y = 1, group_z = 1;
    VkCommandBuffer cb = ctx.device.cmd_buffs[0];
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, standard.text.pl.handle);
    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, standard.text.pl.layout, 0, 1,
                            &standard.text.ds, 0, NULL);

    int x_advance = 0;
    for (size_t c = 0; c < text_len; c++) {
        uint8_t ch = text[c];
        if (!(FIRST_CHAR <= ch && ch < (CHAR_COUNT + FIRST_CHAR))) continue;
        Glyph glyph = font.glyphs[ch-FIRST_CHAR];
        text_push_const.color = color_to_uint32_t(color);
        text_push_const.x     = x + x_advance + glyph.x_offset;
        text_push_const.y     = y + glyph.y_offset;
        text_push_const.x0    = glyph.x0;
        text_push_const.y0    = glyph.y0;
        text_push_const.x1    = glyph.x1;
        text_push_const.y1    = glyph.y1;
        text_push_const.frame_width   = window.width;
        text_push_const.frame_height  = window.height;
        text_push_const.bitmap_width  = font.bitmap_width;
        text_push_const.bitmap_height = font.bitmap_height;
        vkCmdPushConstants(cb, standard.text.pl.layout, VK_SHADER_STAGE_COMPUTE_BIT,
                           0, sizeof(text_push_const), &text_push_const);

        int glyph_height = glyph.y1 - glyph.y0 + 1;
        int glyph_width  = glyph.x1 - glyph.x0 + 1;
        group_x = ceil(glyph_width/IMAGE_WORKGROUP_SIZE);
        group_y = ceil(glyph_height/IMAGE_WORKGROUP_SIZE);
        vkCmdDispatch(cb, group_x, group_y, group_z);

        x_advance += glyph.x_advance;
    }
}

void copy_fb1_to_fb0()
{
    comp_to_comp_buff_barrier(standard.frame_buffers[0].info.buffer);
    comp_to_comp_buff_barrier(standard.frame_buffers[1].info.buffer);

    VkBuffer src = standard.frame_buffers[1].info.buffer;
    VkBuffer dst = standard.frame_buffers[0].info.buffer;
    VkBufferCopy copy_region = {.size = standard.frame_buffers[1].info.range};
    vkCmdCopyBuffer(ctx.device.cmd_buffs[0], src, dst, 1, &copy_region);

    comp_to_comp_buff_barrier(standard.frame_buffers[0].info.buffer);

    standard.last_buff_write_was_fb1 = false;
}
