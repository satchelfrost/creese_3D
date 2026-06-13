#define NOB_STRIP_PREFIX
#define NOB_IMPLEMENTATION
#include "nob.h"

#define BUILD "build/"
#define LINUX BUILD "linux/"
#define WINDOWS BUILD "windows/"
#define SRC "src/"
#define CREESE_3D SRC "creese_3D_engine/"
#define EXTERNAL SRC "external/"
#define SHADERS "shaders/"

const char *creese_3D_modules[] = {
    "creese_3D",
};

const char *shaders[] = {
    "standard_point_render.comp.glsl",
    "standard_point_resolve.comp.glsl",
    "standard_clear_background.comp.glsl",
    "standard_display.vert.glsl",
    "standard_display.frag.glsl",
    "standard_text.comp.glsl",
    "standard_point_hidden_surface_removal.comp.glsl",
    "standard_point_hole_filling.comp.glsl",
};

const char *examples[] = {
    "example_points",
    "example_text",
};

bool compile_shaders(Cmd *cmd)
{
    for (size_t i = 0; i < ARRAY_LEN(shaders); i++) {
        const char *src = temp_sprintf(SHADERS"%s", shaders[i]);
        const char *dst = temp_sprintf(SHADERS"%s.spv", shaders[i]);

        if (needs_rebuild(dst, &src, 1)) {
            char *stage = NULL;
            if (strstr(shaders[i], "frag")) stage = "-fshader-stage=frag";
            if (strstr(shaders[i], "vert")) stage = "-fshader-stage=vert";
            if (strstr(shaders[i], "comp")) stage = "-fshader-stage=comp";
            assert(stage && "shader stage unrecognize");
            cmd_append(cmd, "glslc", stage, "-o", dst, src);
            if (!cmd_run(cmd)) return false;
        }
    }

    return true;
}


bool build_glfw(Cmd *cmd, const char *target)
{
    assert(target);
    const char *build_dir = (target == "linux") ? LINUX : WINDOWS;
    const char *obj = temp_sprintf("%srglfw.o", build_dir);
    if (file_exists(obj)) return true;

    cmd_append(cmd, (target == "linux") ? "gcc" : "x86_64-w64-mingw32-gcc", "-Wall", "-Wextra", "-g");
    cmd_append(cmd, "-I./"EXTERNAL"glfw/");
    cmd_append(cmd, "-c", EXTERNAL"rglfw.c");
    cmd_append(cmd, "-o", obj);
    cmd_append(cmd, "-lm");
    return cmd_run(cmd);
}

bool build_(Cmd *cmd, bool force, const char *target)
{
    const char *build_dir = (target == "linux") ? LINUX : WINDOWS;

    for (size_t i = 0; i < ARRAY_LEN(creese_3D_modules); i++) {
        const char *o = temp_sprintf("%s%s.o", build_dir, creese_3D_modules[i]);
        const char *c = temp_sprintf(CREESE_3D"%s.c", creese_3D_modules[i]);
        int module_touched = needs_rebuild1(o, c);
            if (module_touched < 0) {
            nob_log(ERROR, "needs rebuild failed for module %s", creese_3D_modules[i]);
            return false;
        }
        if (!module_touched && !force) continue;

        cmd_append(cmd, (target == "linux") ? "gcc" : "x86_64-w64-mingw32-gcc", "-Wall", "-Wextra", "-g");
        if (target == "linux") cmd_append(cmd, "-DVULKAN_VALIDATION_ON"); // only use validation if not windows
        cmd_append(cmd, "-I./"EXTERNAL);
        cmd_append(cmd, "-I./"EXTERNAL"glfw/include");
        cmd_append(cmd, "-c", c);
        cmd_append(cmd, "-o", o);
        cmd_append(cmd, "-lm");
        if (!cmd_run(cmd)) return false;
    }

    return true;
}

bool build_example(Cmd *cmd, bool force, const char *target, const char *example_name)
{
    const char *build_dir = (target == "linux") ? LINUX : WINDOWS;

    const char *c = temp_sprintf(SRC"%s.c", example_name);
    const char *e = temp_sprintf("%s%s%s", build_dir, example_name, (target == "windows") ? ".exe" : "");
    const char *glfw = temp_sprintf("%srglfw.o", build_dir);
    File_Paths creese_3D_o_files = {0};

    for (size_t i = 0; i < ARRAY_LEN(creese_3D_modules); i++)
        da_append(&creese_3D_o_files, temp_sprintf("%s%s.o", build_dir, creese_3D_modules[i]));

    int src_touched       = needs_rebuild1(e, c);
    int creese_3D_touched = needs_rebuild(e, creese_3D_o_files.items, creese_3D_o_files.count);
    int glfw_touched      = needs_rebuild1(e, glfw);
    if (src_touched < 0)      nob_log(ERROR, "needs rebuild command failed for source %s", example_name);
    if (creese_3D_touched < 0) nob_log(ERROR, "needs rebuild command failed for creese_3D");
    if (glfw_touched < 0)     nob_log(ERROR, "needs rebuild command failed for example glfw");
    if (!src_touched && !creese_3D_touched && !glfw_touched && !force) return true;

    cmd_append(cmd, (target == "linux") ? "gcc" : "x86_64-w64-mingw32-gcc", "-Wall", "-Wextra", "-g");
    cmd_append(cmd, "-I./"EXTERNAL);
    cmd_append(cmd, "-I./"EXTERNAL"glfw/include");
    cmd_append(cmd, "-I./"SRC);
    cmd_append(cmd, "-o", e);
    cmd_append(cmd, c, glfw);
    for (size_t i = 0; i < creese_3D_o_files.count; i++)
        cmd_append(cmd, creese_3D_o_files.items[i]);
    if (target == "linux") cmd_append(cmd, "-lm", "-lvulkan");
    else                   cmd_append(cmd, "-L./"EXTERNAL, "-l:vulkan-1.lib", "-lgdi32");
    return cmd_run(cmd);
}

void log_usage(const char *program)
{
    printf("usage: %s [options]\n", program);
    printf("    --help\n");
    printf("    --clean, force clean build\n");
    printf("    --target, build target (e.g. windows and linux)\n");
    printf("    --run <executable> <args>, run after building (only for linux)\n");
    printf("    --renderdoc <executable> <args>, (only for linux) expects renderdoc/renderdoccmd in path (https://renderdoc.org/builds)\n");
}

typedef struct {
    const char **items;
    size_t count;
    size_t capacity;
} Args;

struct {
    const char *program;
    const char *target;
    const char *executable;
    bool clean;
    bool renderdoc;
    bool run;
    Args args;
} config;

bool parse_cmd_args(int argc, char **argv)
{
    config.program = shift(argv, argc);

    while (argc) {
        const char *flag = shift(argv, argc);
        if (!strcmp("--help", flag)) {
            log_usage(config.program);
            return false;
        } else if (!strcmp("--clean", flag)) {
            config.clean = true;
            nob_log(INFO, "executing clean build");
        } else if (!strcmp("--renderdoc", flag)) {
            config.renderdoc = true;
            if (argc <= 0) {
                nob_log(ERROR, "renderdoc usage: `./nob --renderdoc <executable> <args>`");
                return false;
            }
            config.executable = shift(argv, argc);
            while (argc) {
                const char *arg = shift(argv, argc);
                da_append(&config.args, arg);
            }
        } else if (!strcmp("--run", flag)) {
            config.run = true;
            if (argc <= 0) {
                nob_log(ERROR, "--run usage: `./nob --run <executable> <args>`");
                return false;
            }
            config.executable = shift(argv, argc);
            while (argc) {
                const char *arg = shift(argv, argc);
                da_append(&config.args, arg);
            }
        } else if (!strcmp("--target", flag)) {
            const char *target = shift(argv, argc);
            if (!strcmp(target, "windows")) config.target = "windows";
            if (!strcmp(target, "linux"))   config.target = "linux";
        } else {
            nob_log(ERROR, "unrecognized flag %s", flag);
            log_usage(config.program);
            return false;
        }
    }

    /* default target linux */
    if (!config.target) config.target = "linux";

    if (config.run && config.renderdoc) {
        nob_log(ERROR, "--run and --renderdoc not compatible together (just choose one or the other)");
        return false;
    }

    return true;
}

bool launch_renderdoc(Cmd *cmd)
{
    /* first run the renderdoc cmd to capture the snapshot */
    cmd_append(cmd, "renderdoccmd", "capture", "-d", ".", "-c", "snapshot", "-w");
    assert(config.executable);
    cmd_append(cmd, config.executable);
    for (size_t i = 0; i < config.args.count; i++)
        cmd_append(cmd, config.args.items[i]);
    if (!cmd_run(cmd)) return false;

    /* next read the snapshot and launch renderdoc */
    File_Paths paths = {0};
    read_entire_dir(".", &paths);
    for (size_t i = 0; i < paths.count; i++) {
        const char *file_name = paths.items[i];
        /* open the first recognized capture file */
        if (strstr(file_name, ".rdc")) {
            nob_log(NOB_INFO, "reading renderdoc file %s", file_name);
            cmd_append(cmd, "renderdoc", file_name);
            if (!cmd_run_sync_and_reset(cmd)) return false;

            /* cleanup capture file */
            nob_log(NOB_INFO, "removing renderdoc file %s", file_name);
            cmd_append(cmd, "rm", file_name);
            if (!cmd_run_sync_and_reset(cmd)) return false;
            break;
        }
    }

    return true;
}

bool launch_exec(Cmd *cmd)
{
    assert(config.executable);
    cmd_append(cmd, config.executable);
    for (size_t i = 0; i < config.args.count; i++)
        cmd_append(cmd, config.args.items[i]);
    if (!cmd_run(cmd)) return false;

    return true;
}

int main(int argc, char **argv)
{
    NOB_GO_REBUILD_URSELF(argc, argv);

    if (!parse_cmd_args(argc, argv))   return 1;
    if (!mkdir_if_not_exists(BUILD))   return 1;
    if (!mkdir_if_not_exists(LINUX))   return 1;
    if (!mkdir_if_not_exists(WINDOWS)) return 1;

    Cmd cmd = {0};

    if (!build_glfw(&cmd, config.target)) return 1;
    if (!build_(&cmd, config.clean, config.target)) return 1;
    for (size_t i = 0; i < ARRAY_LEN(examples); i++)
        if (!build_example(&cmd, config.clean, config.target, examples[i])) return 1;
    if (!compile_shaders(&cmd)) return 1;

    if (config.run)       if (!launch_exec(&cmd))      return 1;
    if (config.renderdoc) if (!launch_renderdoc(&cmd)) return 1;

    return 0;
}
