#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>

/*
 * D0.1 toolchain scaffold: minimal viewport/canvas hello-world FAP.
 *
 * This exists to prove the ufbt build path works end to end. It draws a
 * static screen and exits on Back. Receive-only invariant: this app has no
 * radio access and no transmit code paths of any kind.
 */

typedef struct {
    FuriMessageQueue* input_queue;
} FlipScopeApp;

static void flipscope_draw_callback(Canvas* canvas, void* context) {
    UNUSED(context);
    canvas_clear(canvas);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignCenter, "FlipScope");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 36, AlignCenter, AlignCenter, "Toolchain proof (D0.1)");
    canvas_draw_str_aligned(canvas, 64, 52, AlignCenter, AlignCenter, "Press Back to exit");
}

static void flipscope_input_callback(InputEvent* event, void* context) {
    FuriMessageQueue* input_queue = context;
    furi_message_queue_put(input_queue, event, FuriWaitForever);
}

int32_t flipscope_app(void* p) {
    UNUSED(p);

    FlipScopeApp app = {0};
    app.input_queue = furi_message_queue_alloc(8, sizeof(InputEvent));

    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, flipscope_draw_callback, NULL);
    view_port_input_callback_set(view_port, flipscope_input_callback, app.input_queue);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    InputEvent event;
    bool running = true;
    while(running) {
        FuriStatus status = furi_message_queue_get(app.input_queue, &event, FuriWaitForever);
        if(status == FuriStatusOk) {
            if(event.key == InputKeyBack && event.type == InputTypeShort) {
                running = false;
            }
        }
    }

    gui_remove_view_port(gui, view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(view_port);
    furi_message_queue_free(app.input_queue);

    return 0;
}
