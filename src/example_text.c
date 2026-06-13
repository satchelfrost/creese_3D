#include "creese_3D.h"

#define FONT_SIZE 42

const char *msgs[] = {
    "Hello how are yee? I hope thou art gewd",
    "This is some more text, let's see how this goes",
    "Yet again I have written down more things to try and slow things down",
    "Wow, I'm still going, isn't this so fun right now... yis.",
    "Hello how are yee? I hope thou art gewd",
    "This is some more text, let's see how this goes",
    "Yet again I have written down more things to try and slow things down",
    "Wow, I'm still going, isn't this so fun right now... yis.",
    "Hello how are yee? I hope thou art gewd",
    "This is some more text, let's see how this goes",
    "Yet again I have written down more things to try and slow things down",
    "Wow, I'm still going, isn't this so fun right now... yis.",
    "Hello how are yee? I hope thou art gewd",
    "This is some more text, let's see how this goes",
    "Yet again I have written down more things to try and slow things down",
    "Wow, I'm still going, isn't this so fun right now... yis.",
};

int main()
{
    init_window(1600, 900, "example text");

    Font font = load_font("assets/RobotoMono-Medium.ttf", FONT_SIZE);
    String_Builder sb = {0};

    while (!window_should_close()) {
        begin_drawing();
            clear_background(BLUE);
            sb.count = 0;
            sb_appendf(&sb, "FPS:%d", get_avg_fps());
            draw_text_at_base(font, sb.items, sb.count, 20, FONT_SIZE, BLACK);
            for (size_t i = 0; i < ARRAY_LEN(msgs); i++)
                draw_text_at_base(font, msgs[i], strlen(msgs[i]), 20, FONT_SIZE*(2 + i), BLACK);
        end_drawing();
    }

    close_window();

    return 0;
}
