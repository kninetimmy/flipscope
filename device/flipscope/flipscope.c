#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <string.h>

#include <lib/subghz/environment.h>
#include <lib/subghz/receiver.h>
#include <lib/subghz/registry.h>
#include <lib/subghz/subghz_protocol_registry.h>

#define TAG "FlipScope"

/*
 * D0.2 decoder-registry probe.
 *
 * This extends the D0.1 toolchain scaffold into a feasibility spike that
 * proves SubGhz protocol decoder access is reachable through stable,
 * publicly-exported SDK APIs, without touching radio hardware at all.
 *
 * On launch the app:
 *   1. Allocates a SubGhzEnvironment and binds the built-in protocol
 *      registry (`subghz_protocol_registry`), logging its protocol count
 *      and names.
 *   2. Allocates a SubGhzReceiver over that environment, registers a
 *      receiver-level rx callback, and filters to decodable protocols.
 *   3. Feeds an embedded level/duration test vector into
 *      `subghz_receiver_decode` as an in-memory stimulus (no RX path, no
 *      `furi_hal_subghz` calls of any kind).
 *   4. Renders a PASS/FAIL result screen from whatever the receiver
 *      decoded.
 *
 * Receive-only invariant (PRD §4.3): this file contains no transmit code
 * paths of any kind. It never includes lib/subghz/transmitter.h and never
 * calls into furi_hal_subghz.
 */

/*
 * Embedded test vector: the first three complete Princeton bursts
 * transcribed from captures/princeton_raw.sub (see captures/README.md for
 * upstream provenance and integrity hashes). Each RAW_Data entry is a
 * signed duration in microseconds: positive means level high, negative
 * means level low. A burst is terminated by its ~16ms inter-burst guard gap
 * (the `-16xxx` entry). The leading `1711 -32700 621 -1600` preamble
 * entries are included exactly as they appear in the fixture's first
 * RAW_Data line.
 *
 * Three bursts are required, not two: the official Princeton decoder (see
 * lib/subghz/protocols/princeton.c) only invokes its rx callback once it
 * has committed a frame, and commit requires two consecutive identical
 * decodes (decode_data == last_data, with last_data non-zero) - a single
 * decoded frame only updates last_data and returns. Worse, burst 1 never
 * decodes at all here: the decoder only leaves its reset state on a low
 * gap within te_short*36 +/- te_delta*36 (~14 ms +/- 10.8 ms), and burst
 * 1's leading -32700 us entry (32.7 ms) falls outside that preamble
 * window, so all of burst 1 is consumed merely arming the decoder at its
 * own terminating -16342 us gap. Burst 2 is therefore the first frame
 * actually decoded and stored as last_data, and burst 3 is the matching
 * repeat whose decode_data == last_data comparison fires the callback.
 *
 * Burst 1 (52 entries, ends at the first guard gap, -16342 us; consumed
 * only to arm the decoder - produces no decoded frame):
 *   1711 -32700 621 -1600 1623 -564 1639 -552 1637 -520 1701 -514 1665 -516
 *   579 -1556 607 -1562 571 -1570 1697 -484 605 -1544 1701 -500 611 -1544
 *   1697 -528 1675 -518 1705 -492 589 -1580 593 -1536 609 -1562 583 -1574
 *   595 -1542 603 -1568 1699 -490 1693 -496 613 -16342
 *
 * Burst 2 (50 entries, ends at the second guard gap, -16368 us; the first
 * decoded frame, stored as last_data but not yet committed):
 *   623 -1538 1693 -520 1675 -486 1721 -492 1709 -520 1701 -486 579 -1588
 *   573 -1578 589 -1568 1695 -482 605 -1582 1703 -492 589 -1578 1675 -518
 *   1707 -484 1703 -500 609 -1576 571 -1574 611 -1532 627 -1538 603 -1546
 *   607 -1548 1729 -474 1719 -490 613 -16368
 *
 * Burst 3 (50 entries, ends at the third guard gap, -16394 us; the
 * matching repeat frame whose decode_data == last_data commit fires the
 * rx callback):
 *   591 -1570 1689 -504 1699 -484 1711 -506 1715 -486 1707 -488 619 -1576
 *   583 -1540 611 -1562 1711 -486 615 -1546 1705 -490 621 -1542 1705 -506
 *   1713 -486 1713 -486 629 -1540 609 -1564 589 -1574 599 -1540 607 -1568
 *   611 -1546 1707 -492 1711 -480 609 -16394
 */
static const int32_t flipscope_princeton_test_vector[] = {
    /* Burst 1 */
    1711,
    -32700,
    621,
    -1600,
    1623,
    -564,
    1639,
    -552,
    1637,
    -520,
    1701,
    -514,
    1665,
    -516,
    579,
    -1556,
    607,
    -1562,
    571,
    -1570,
    1697,
    -484,
    605,
    -1544,
    1701,
    -500,
    611,
    -1544,
    1697,
    -528,
    1675,
    -518,
    1705,
    -492,
    589,
    -1580,
    593,
    -1536,
    609,
    -1562,
    583,
    -1574,
    595,
    -1542,
    603,
    -1568,
    1699,
    -490,
    1693,
    -496,
    613,
    -16342,
    /* Burst 2 */
    623,
    -1538,
    1693,
    -520,
    1675,
    -486,
    1721,
    -492,
    1709,
    -520,
    1701,
    -486,
    579,
    -1588,
    573,
    -1578,
    589,
    -1568,
    1695,
    -482,
    605,
    -1582,
    1703,
    -492,
    589,
    -1578,
    1675,
    -518,
    1707,
    -484,
    1703,
    -500,
    609,
    -1576,
    571,
    -1574,
    611,
    -1532,
    627,
    -1538,
    603,
    -1546,
    607,
    -1548,
    1729,
    -474,
    1719,
    -490,
    613,
    -16368,
    /* Burst 3 */
    591,
    -1570,
    1689,
    -504,
    1699,
    -484,
    1711,
    -506,
    1715,
    -486,
    1707,
    -488,
    619,
    -1576,
    583,
    -1540,
    611,
    -1562,
    1711,
    -486,
    615,
    -1546,
    1705,
    -490,
    621,
    -1542,
    1705,
    -506,
    1713,
    -486,
    1713,
    -486,
    629,
    -1540,
    609,
    -1564,
    589,
    -1574,
    599,
    -1540,
    607,
    -1568,
    611,
    -1546,
    1707,
    -492,
    1711,
    -480,
    609,
    -16394,
};

#define FLIPSCOPE_PRINCETON_VECTOR_LEN \
    (sizeof(flipscope_princeton_test_vector) / sizeof(flipscope_princeton_test_vector[0]))

/* Ground truth, from captures/princeton.sub: Protocol=Princeton, Bit=24, Key=00 00 00 00 00 95 D5 D4. */
#define FLIPSCOPE_EXPECTED_PROTOCOL_NAME "Princeton"
#define FLIPSCOPE_EXPECTED_BIT_COUNT_TOKEN "24bit"
#define FLIPSCOPE_EXPECTED_KEY_TOKEN_UPPER "95D5D4"
#define FLIPSCOPE_EXPECTED_KEY_TOKEN_LOWER "95d5d4"

typedef struct {
    bool decoded;
    bool pass;
    char protocol_name[24];
    char diag_line1[24];
    char diag_line2[24];
} FlipScopeProbeResult;

typedef struct {
    FuriMessageQueue* input_queue;
    FlipScopeProbeResult probe;
} FlipScopeApp;

static void flipscope_subghz_rx_callback(
    SubGhzReceiver* receiver,
    SubGhzProtocolDecoderBase* decoder_base,
    void* context) {
    UNUSED(receiver);
    FlipScopeProbeResult* result = context;

    /* Keep the first successful decode; the embedded vector only needs one. */
    if(result->decoded) {
        return;
    }

    const char* name =
        (decoder_base != NULL && decoder_base->protocol != NULL) ? decoder_base->protocol->name :
                                                                     "(unknown)";
    strncpy(result->protocol_name, name, sizeof(result->protocol_name) - 1);
    result->protocol_name[sizeof(result->protocol_name) - 1] = '\0';

    FuriString* text = furi_string_alloc();
    bool has_string =
        decoder_base != NULL && subghz_protocol_decoder_base_get_string(decoder_base, text);
    const char* raw = has_string ? furi_string_get_cstr(text) : "";

    FURI_LOG_I(
        TAG,
        "D0.2 probe: rx callback fired, protocol=%s decode_string=%s",
        name,
        has_string ? raw : "(none)");

    /* Flatten CRLF-delimited decode text into two short lines for the result screen. */
    char flat[64];
    size_t j = 0;
    for(size_t i = 0; raw[i] != '\0' && j + 1 < sizeof(flat); i++) {
        char c = raw[i];
        if(c == '\r') continue;
        if(c == '\n') c = ' ';
        flat[j++] = c;
    }
    flat[j] = '\0';

    strncpy(result->diag_line1, flat, sizeof(result->diag_line1) - 1);
    result->diag_line1[sizeof(result->diag_line1) - 1] = '\0';
    size_t line1_len = strlen(result->diag_line1);
    strncpy(result->diag_line2, flat + line1_len, sizeof(result->diag_line2) - 1);
    result->diag_line2[sizeof(result->diag_line2) - 1] = '\0';

    bool name_ok = strcmp(name, FLIPSCOPE_EXPECTED_PROTOCOL_NAME) == 0;
    bool bits_ok = has_string &&
                   furi_string_search_str(text, FLIPSCOPE_EXPECTED_BIT_COUNT_TOKEN, 0) !=
                       FURI_STRING_FAILURE;
    bool key_ok = has_string &&
                  (furi_string_search_str(text, FLIPSCOPE_EXPECTED_KEY_TOKEN_UPPER, 0) !=
                       FURI_STRING_FAILURE ||
                   furi_string_search_str(text, FLIPSCOPE_EXPECTED_KEY_TOKEN_LOWER, 0) !=
                       FURI_STRING_FAILURE);

    result->pass = name_ok && bits_ok && key_ok;
    result->decoded = true;

    furi_string_free(text);
}

/*
 * Feed the embedded test vector through a SubGhz decoder chain built entirely
 * from stable SDK APIs. No furi_hal_subghz calls, no RX path: this is a pure
 * in-memory decode feed.
 */
static void flipscope_run_decoder_probe(FlipScopeProbeResult* result) {
    memset(result, 0, sizeof(*result));

    FURI_LOG_I(TAG, "D0.2 probe: allocating SubGhz environment");
    SubGhzEnvironment* environment = subghz_environment_alloc();
    subghz_environment_set_protocol_registry(environment, &subghz_protocol_registry);

    size_t protocol_count = subghz_protocol_registry_count(&subghz_protocol_registry);
    FURI_LOG_I(TAG, "D0.2 probe: registry has %zu protocols", protocol_count);
    for(size_t i = 0; i < protocol_count; i++) {
        const SubGhzProtocol* protocol =
            subghz_protocol_registry_get_by_index(&subghz_protocol_registry, i);
        FURI_LOG_I(
            TAG,
            "D0.2 probe: [%zu] %s",
            i,
            (protocol != NULL && protocol->name != NULL) ? protocol->name : "(unnamed)");
    }

    FURI_LOG_I(TAG, "D0.2 probe: allocating SubGhz receiver");
    SubGhzReceiver* receiver = subghz_receiver_alloc_init(environment);
    subghz_receiver_set_rx_callback(receiver, flipscope_subghz_rx_callback, result);
    subghz_receiver_set_filter(receiver, SubGhzProtocolFlag_Decodable);

    FURI_LOG_I(
        TAG,
        "D0.2 probe: feeding %zu duration entries transcribed from captures/princeton_raw.sub",
        FLIPSCOPE_PRINCETON_VECTOR_LEN);
    for(size_t i = 0; i < FLIPSCOPE_PRINCETON_VECTOR_LEN; i++) {
        int32_t entry = flipscope_princeton_test_vector[i];
        bool level = entry > 0;
        uint32_t duration = (uint32_t)(level ? entry : -entry);
        subghz_receiver_decode(receiver, level, duration);
    }

    if(!result->decoded) {
        FURI_LOG_E(TAG, "D0.2 probe: no decode callback fired for embedded test vector");
    } else {
        FURI_LOG_I(
            TAG,
            "D0.2 probe: decode result protocol=%s pass=%s",
            result->protocol_name,
            result->pass ? "true" : "false");
    }

    subghz_receiver_free(receiver);
    subghz_environment_free(environment);
}

/*
 * Row y-coordinates for the 128x64 canvas. Six rows of text can appear at
 * once (title, subtitle, result, diag1, diag2, back), so rows are spaced
 * tightly enough that the last row (FLIPSCOPE_ROW_BACK_Y, AlignTop) still
 * starts at y<=54 - leaving its ~10px-tall glyphs fully inside the 64px
 * canvas instead of clipped off the bottom.
 */
#define FLIPSCOPE_ROW_TITLE_Y 6
#define FLIPSCOPE_ROW_SUBTITLE_Y 17
#define FLIPSCOPE_ROW_1_Y 27
#define FLIPSCOPE_ROW_2_Y 36
#define FLIPSCOPE_ROW_3_Y 45
#define FLIPSCOPE_ROW_BACK_Y 54

static void flipscope_draw_callback(Canvas* canvas, void* context) {
    FlipScopeApp* app = context;
    canvas_clear(canvas);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(
        canvas, 2, FLIPSCOPE_ROW_TITLE_Y, AlignLeft, AlignTop, "FlipScope D0.2");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(
        canvas, 2, FLIPSCOPE_ROW_SUBTITLE_Y, AlignLeft, AlignTop, "Princeton decoder probe");

    if(!app->probe.decoded) {
        canvas_draw_str_aligned(canvas, 2, FLIPSCOPE_ROW_1_Y, AlignLeft, AlignTop, "Result: FAIL");
        canvas_draw_str_aligned(
            canvas, 2, FLIPSCOPE_ROW_2_Y, AlignLeft, AlignTop, "No decode callback fired");
    } else {
        char result_line[24];
        snprintf(result_line, sizeof(result_line), "Result: %s", app->probe.pass ? "PASS" : "FAIL");
        canvas_draw_str_aligned(canvas, 2, FLIPSCOPE_ROW_1_Y, AlignLeft, AlignTop, result_line);
        canvas_draw_str_aligned(
            canvas, 2, FLIPSCOPE_ROW_2_Y, AlignLeft, AlignTop, app->probe.diag_line1);
        canvas_draw_str_aligned(
            canvas, 2, FLIPSCOPE_ROW_3_Y, AlignLeft, AlignTop, app->probe.diag_line2);
    }

    canvas_draw_str_aligned(canvas, 2, FLIPSCOPE_ROW_BACK_Y, AlignLeft, AlignTop, "Back = exit");
}

static void flipscope_input_callback(InputEvent* event, void* context) {
    FuriMessageQueue* input_queue = context;
    furi_message_queue_put(input_queue, event, FuriWaitForever);
}

int32_t flipscope_app(void* p) {
    UNUSED(p);

    FlipScopeApp app = {0};
    app.input_queue = furi_message_queue_alloc(8, sizeof(InputEvent));

    flipscope_run_decoder_probe(&app.probe);

    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, flipscope_draw_callback, &app);
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
