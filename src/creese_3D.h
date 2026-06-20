#ifndef CREESE_3D_H_
#define CREESE_3D_H_

// TODO: make the attribute switches do something in the triangle render shader
// TODO: decide if I actually want to pass indices, or just have it implied that it's in order 
// TODO: promote the simple shapes like cube to API
// TOOD: if there are no color attributes on the vertices, then the color should come from push constant
// TODO: pass the model view projection as a push constant instead of a ubo (or perhaps view and projection as ubo)
// TODO: frame buffer should be based on resolution not necessarily window size, make this configurable

// TODO: bring some of the ideas from creese 2D such as module structure
// TODO: separate into some behavior into platform_desktop
// TODO: precompile some of the external modules
// TODO: make the log level a little more dynamic for rvk
// TODO: configurable z_near/z_far and ttf bitmap defaults
// TODO: vulkan context should be at least vk_ctx instead of ctx
// TODO: currently primitives are not being used
// TODO: make it so that the pipelines are only created when needed (for now it's fine since there's not a lot of pipelines)

#include <stdbool.h>
#include "rvk.h"

#define GLFW_INCLUDE_NONE
#include "GLFW/glfw3.h"
#include "raymath.h"

#define NOB_STRIP_PREFIX
#include "../../nob.h"

#define LIGHTGRAY  (Color){ 200, 200, 200, 255 }
#define GRAY       (Color){ 130, 130, 130, 255 }
#define DARKGRAY   (Color){ 80, 80, 80, 255 }
#define YELLOW     (Color){ 253, 249, 0, 255 }
#define GOLD       (Color){ 255, 203, 0, 255 }
#define ORANGE     (Color){ 255, 161, 0, 255 }
#define PINK       (Color){ 255, 109, 194, 255 }
#define RED        (Color){ 230, 41, 55, 255 }
#define MAROON     (Color){ 190, 33, 55, 255 }
#define GREEN      (Color){ 0, 228, 48, 255 }
#define LIME       (Color){ 0, 158, 47, 255 }
#define DARKGREEN  (Color){ 0, 117, 44, 255 }
#define SKYBLUE    (Color){ 102, 191, 255, 255 }
#define BLUE       (Color){ 0, 121, 241, 255 }
#define DARKBLUE   (Color){ 0, 82, 172, 255 }
#define PURPLE     (Color){ 200, 122, 255, 255 }
#define VIOLET     (Color){ 135, 60, 190, 255 }
#define DARKPURPLE (Color){ 112, 31, 126, 255 }
#define BEIGE      (Color){ 211, 176, 131, 255 }
#define BROWN      (Color){ 127, 106, 79, 255 }
#define DARKBROWN  (Color){ 76, 63, 47, 255 }
#define WHITE      (Color){ 255, 255, 255, 255 }
#define BLACK      (Color){ 0, 0, 0, 255 }
#define BLANK      (Color){ 0, 0, 0, 0 }
#define MAGENTA    (Color){ 255, 0, 255, 255 }

typedef struct {
    Vector3 min, max;
} AABB;

typedef struct {
    Vector2 min, max;
} AABB_2D;

typedef struct Color {
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
} Color;

typedef struct {
    float x, y, z;
    uint32_t color;
} Point;

typedef enum {
    KEY_NULL            = 0,
    KEY_APOSTROPHE      = 39,
    KEY_COMMA           = 44,
    KEY_MINUS           = 45,
    KEY_PERIOD          = 46,
    KEY_SLASH           = 47,
    KEY_ZERO            = 48,
    KEY_ONE             = 49,
    KEY_TWO             = 50,
    KEY_THREE           = 51,
    KEY_FOUR            = 52,
    KEY_FIVE            = 53,
    KEY_SIX             = 54,
    KEY_SEVEN           = 55,
    KEY_EIGHT           = 56,
    KEY_NINE            = 57,
    KEY_SEMICOLON       = 59,
    KEY_EQUAL           = 61,
    KEY_A               = 65,
    KEY_B               = 66,
    KEY_C               = 67,
    KEY_D               = 68,
    KEY_E               = 69,
    KEY_F               = 70,
    KEY_G               = 71,
    KEY_H               = 72,
    KEY_I               = 73,
    KEY_J               = 74,
    KEY_K               = 75,
    KEY_L               = 76,
    KEY_M               = 77,
    KEY_N               = 78,
    KEY_O               = 79,
    KEY_P               = 80,
    KEY_Q               = 81,
    KEY_R               = 82,
    KEY_S               = 83,
    KEY_T               = 84,
    KEY_U               = 85,
    KEY_V               = 86,
    KEY_W               = 87,
    KEY_X               = 88,
    KEY_Y               = 89,
    KEY_Z               = 90,
    KEY_LEFT_BRACKET    = 91,
    KEY_BACKSLASH       = 92,
    KEY_RIGHT_BRACKET   = 93,
    KEY_GRAVE           = 96,
    KEY_SPACE           = 32,
    KEY_ESCAPE          = 256,
    KEY_ENTER           = 257,
    KEY_TAB             = 258,
    KEY_BACKSPACE       = 259,
    KEY_INSERT          = 260,
    KEY_DELETE          = 261,
    KEY_RIGHT           = 262,
    KEY_LEFT            = 263,
    KEY_DOWN            = 264,
    KEY_UP              = 265,
    KEY_PAGE_UP         = 266,
    KEY_PAGE_DOWN       = 267,
    KEY_HOME            = 268,
    KEY_END             = 269,
    KEY_CAPS_LOCK       = 280,
    KEY_SCROLL_LOCK     = 281,
    KEY_NUM_LOCK        = 282,
    KEY_PRINT_SCREEN    = 283,
    KEY_PAUSE           = 284,
    KEY_F1              = 290,
    KEY_F2              = 291,
    KEY_F3              = 292,
    KEY_F4              = 293,
    KEY_F5              = 294,
    KEY_F6              = 295,
    KEY_F7              = 296,
    KEY_F8              = 297,
    KEY_F9              = 298,
    KEY_F10             = 299,
    KEY_F11             = 300,
    KEY_F12             = 301,
    KEY_LEFT_SHIFT      = 340,
    KEY_LEFT_CONTROL    = 341,
    KEY_LEFT_ALT        = 342,
    KEY_LEFT_SUPER      = 343,
    KEY_RIGHT_SHIFT     = 344,
    KEY_RIGHT_CONTROL   = 345,
    KEY_RIGHT_ALT       = 346,
    KEY_RIGHT_SUPER     = 347,
    KEY_KB_MENU         = 348,
    KEY_KP_0            = 320,
    KEY_KP_1            = 321,
    KEY_KP_2            = 322,
    KEY_KP_3            = 323,
    KEY_KP_4            = 324,
    KEY_KP_5            = 325,
    KEY_KP_6            = 326,
    KEY_KP_7            = 327,
    KEY_KP_8            = 328,
    KEY_KP_9            = 329,
    KEY_KP_DECIMAL      = 330,
    KEY_KP_DIVIDE       = 331,
    KEY_KP_MULTIPLY     = 332,
    KEY_KP_SUBTRACT     = 333,
    KEY_KP_ADD          = 334,
    KEY_KP_ENTER        = 335,
    KEY_KP_EQUAL        = 336,
    KEY_BACK            = 4,
    KEY_MENU            = 82,
    KEY_VOLUME_UP       = 24,
    KEY_VOLUME_DOWN     = 25 
} Keyboard_Key;

typedef enum {
    MOUSE_BUTTON_LEFT    = 0,
    MOUSE_BUTTON_RIGHT   = 1,
    MOUSE_BUTTON_MIDDLE  = 2,
    MOUSE_BUTTON_SIDE    = 3,
    MOUSE_BUTTON_EXTRA   = 4,
    MOUSE_BUTTON_FORWARD = 5,
    MOUSE_BUTTON_BACK    = 6,
} Mouse_Button;

typedef enum {
    GAMEPAD_BUTTON_UNKNOWN = 0,
    GAMEPAD_BUTTON_LEFT_FACE_UP,
    GAMEPAD_BUTTON_LEFT_FACE_RIGHT,
    GAMEPAD_BUTTON_LEFT_FACE_DOWN,
    GAMEPAD_BUTTON_LEFT_FACE_LEFT,
    GAMEPAD_BUTTON_RIGHT_FACE_UP,
    GAMEPAD_BUTTON_RIGHT_FACE_RIGHT,
    GAMEPAD_BUTTON_RIGHT_FACE_DOWN,
    GAMEPAD_BUTTON_RIGHT_FACE_LEFT,
    GAMEPAD_BUTTON_LEFT_TRIGGER_1,
    GAMEPAD_BUTTON_LEFT_TRIGGER_2,
    GAMEPAD_BUTTON_RIGHT_TRIGGER_1,
    GAMEPAD_BUTTON_RIGHT_TRIGGER_2,
    GAMEPAD_BUTTON_MIDDLE_LEFT,
    GAMEPAD_BUTTON_MIDDLE,
    GAMEPAD_BUTTON_MIDDLE_RIGHT,
    GAMEPAD_BUTTON_LEFT_THUMB,
    GAMEPAD_BUTTON_RIGHT_THUMB
} Gamepad_Button;

typedef enum {
    GAMEPAD_AXIS_LEFT_X        = 0,
    GAMEPAD_AXIS_LEFT_Y        = 1,
    GAMEPAD_AXIS_RIGHT_X       = 2,
    GAMEPAD_AXIS_RIGHT_Y       = 3,
    GAMEPAD_AXIS_LEFT_TRIGGER  = 4,
    GAMEPAD_AXIS_RIGHT_TRIGGER = 5
} Gamepad_Axis;

typedef struct {
    uint32_t width, height;
    GLFWwindow *handle;
} Window;

void init_window(int width, int height, char *title);
void close_window();
bool window_should_close();
Window get_window();
void begin_drawing();
void clear_background(Color bg_color);
void end_drawing();
void poll_input_events(); // called by end_drawing
void begin_timer();
void end_timer();
double get_time();
double get_frame_time();

typedef struct {
    Vector3 position;
    Vector3 target;
    Vector3 up;
    float fovy;
} Camera;

void begin_mode_3D(Camera camera);
void end_mode_3D();
void update_camera_free(Camera *camera);

bool draw_rectangle(int x, int y, int width, int height, Color color);
bool draw_bounding_box_from_matrix_stack(Color color);
bool draw_frustum(Color color, Camera camera);

typedef struct {
    Rvk_Device device;
    Rvk_Swapchain swapchain;
    VkInstance instance;
    VkSurfaceKHR surface;
    VkDescriptorPool pool;
} Vulkan_Context;

Vulkan_Context get_vulkan_context();

typedef struct {
    uint32_t img_idx;

    struct {
        double curr;
        double prev;
        double update;
        double draw;
        double frame;
        double target;
        size_t frame_count;
    } time;
} Frame_Context;

Frame_Context get_frame_context();
VkFramebuffer get_current_frame_buffer();

void push_matrix();
void pop_matrix();
void scale(float x, float y, float z);
void translate(float x, float y, float z);
Matrix calc_projection(Camera camera);
Matrix get_projection();
Matrix get_view();
Matrix get_model();
void matrix_cat(Matrix m);
void rotate_y(float angle);

VkCommandBuffer get_current_cmd_buff();
void set_viewport_scissor();
void wait_idle();
bool is_key_pressed(int key);
bool is_key_down(int key);
void log_fps();
int get_fps();
int get_avg_fps();
float get_avg_frame_time();
bool is_gamepad_button_pressed(int button);

/* Text */
#define CHAR_COUNT 96
#define FIRST_CHAR 32

typedef struct {
   unsigned short x0, y0, x1, y1;
   float x_offset, y_offset, x_advance;
} Glyph;

typedef struct {
    float height;
    int bitmap_width;
    int bitmap_height;
    uint8_t *bitmap;
    Glyph glyphs[CHAR_COUNT]; // ASCII 32..126 is 95 glyphs
} Font;

typedef enum {
    ATTRIBUTE_POSITION,
    ATTRIBUTE_NORMAL,
    ATTRIBUTE_TEX_COORD,
    ATTRIBUTE_TANGET,
    ATTRIBUTE_COLOR,
    ATTRIBUTE_COUNT,
} Attribute;

typedef struct {
    uint32_t attr_mask;
    struct {  Vector3 *items; size_t count; size_t capacity; } positions;
    struct {  Vector3 *items; size_t count; size_t capacity; } normals;
    struct {  Vector2 *items; size_t count; size_t capacity; } tex_coords;
    struct {  Vector2 *items; size_t count; size_t capacity; } tangets;
    struct { uint32_t *items; size_t count; size_t capacity; } colors;
    struct { uint32_t *items; size_t count; size_t capacity; } indices;

    // vulkan cannot have a zero size buffer
    // so this serves as something that's nonzero, but
    // used when a model does not have an attribute. For example,
    // suppose a model doesn't have tangets, but the shader still
    // has a slot for them, well in that case the nil buffer gets bound to that slot
    size_t nil_buffer;

    size_t tri_count;

    // vulkan buffers
    Rvk_Buffer position_buff;
    Rvk_Buffer normal_buff;
    Rvk_Buffer tex_coord_buff;
    Rvk_Buffer tanget_buff;
    Rvk_Buffer color_buff;
    Rvk_Buffer indices_buff;

    VkDescriptorSet ds;
} Model;

Model load_obj_model(const char *file_name);
void draw_model(Model model);
void destroy_model(Model model);
void init_triangle_model_ds(Model *model);
void update_triangle_model(Model model);

Font load_font(const char *file_path, int font_height);
void unload_font(Font font);
void draw_text_at_base(Font font, const char *text, size_t text_len, int x, int y, Color color);
uint32_t color_to_uint32_t(Color color);
Color color_from_hsv(float hue, float saturation, float value);

VkPipelineShaderStageCreateInfo load_compute_shader(const char *file_path, String_Builder *sb);
void unload_shader(VkPipelineShaderStageCreateInfo ci);
void destroy_pipeline(Rvk_Pipeline pipeline);
void update_render_ds(Rvk_Buffer point_buffer, VkDescriptorSet ds);
void read_after_write_ssbo_barrier(VkCommandBuffer cb);
void draw_points(VkDescriptorSet ds, size_t point_count, size_t point_offset);
void point_hidden_surface_removal();
void point_hole_filling();
Rvk_Buffer create_compute_buffer(size_t size, void *data);
void destroy_buffer(Rvk_Buffer buff);
void compute_to_frag_sample_image_barrier(VkCommandBuffer cb, VkImage image);
void alloc_point_render_ds(VkDescriptorSet *ds);
void update_point_render_ds(Rvk_Buffer buff, VkDescriptorSet ds);

#endif // CREESE_3D_H_
