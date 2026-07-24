#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdlib.h>
#include <string.h>

#include <lib/subghz/devices/devices.h>
#include <lib/subghz/devices/cc1101_int/cc1101_int_interconnect.h>

#define TAG "FlipScope"

/*
 * D0.3 async-RX pulse-recording probe.
 *
 * This replaces the D0.2 decoder-registry probe (its body remains in git
 * history at cb72d94) with the first probe that actually touches the
 * internal CC1101 radio. D0.2 proved decoder access; D0.3 proves that
 * asynchronous receive, level/duration pulse capture, and the FR-031
 * pre-arm ring buffer are all reachable through stable, publicly-exported
 * SDK APIs.
 *
 * On launch the app:
 *   1. Runs a RAM probe: logs free heap, allocates a >=16 KiB candidate
 *      pre-arm ring buffer (kept live as the real RX buffer for the rest
 *      of the run), then allocates and frees a >=32 KiB candidate buffer,
 *      logging free heap around each step. This answers "is the FR-031
 *      pre-arm buffer affordable in RAM?" directly from measurement.
 *   2. Acquires the internal CC1101 device via the stable `subghz_devices_*`
 *      API (lib/subghz/devices/devices.h), loads the stock
 *      FuriHalSubGhzPresetOok650Async preset, validates and tunes to
 *      433.92 MHz, and starts asynchronous receive with an ISR-safe
 *      level/duration callback that writes into the pre-allocated ring
 *      buffer.
 *   3. Shows a live view of recorded pulse/drop counts and the RAM probe
 *      summary while RX runs.
 *   4. On the first Back press, stops async RX, puts the radio to sleep,
 *      and releases it, then renders a PASS/FAIL result screen. A second
 *      Back press exits.
 *
 * Radio API path finding: the stable `subghz_devices_*` wrapper API
 * (lib/subghz/devices/devices.h, backed by the `cc1101_int` device named
 * via SUBGHZ_DEVICE_CC1101_INT_NAME) is exported by the ufbt stable SDK -
 * confirmed against targets/f7/api_symbols.csv in the cached SDK, not just
 * the header tree. That is the path used here; the older
 * `furi_hal_subghz_*` free-function API was not needed as a fallback.
 *
 * IQ boundary (PRD §3): the callback below receives only a demodulated
 * level (bool) and duration (us) per edge - never IQ samples, never a
 * waveform. Nothing here claims or implies IQ capture.
 *
 * Receive-only invariant (PRD §4.3): this file contains no transmit code
 * paths of any kind. It never includes lib/subghz/transmitter.h and never
 * calls any *_tx / *async_tx* / *transmit* API.
 *
 * Evidence preservation (PRD §4.5, FR-029): the ring buffer below is
 * write-once per slot - the capture callback never overwrites a slot that
 * already holds a recorded pulse; once full, further pulses are counted as
 * drops rather than overwriting older (still "original") entries.
 */

/* 433.92 MHz: PRD's reference OOK ISM frequency for this probe. */
#define FLIPSCOPE_FREQUENCY_HZ 433920000UL

/*
 * FR-031 pre-arm ring buffer candidates. Candidate A (>=16 KiB) is
 * allocated once and kept live as the actual RX ring buffer for the rest
 * of the run - this IS the pre-arm pattern. Candidate B (>=32 KiB) is
 * allocated, measured, and freed immediately: it exists purely to answer
 * "would a 32 KiB pre-arm buffer also fit?" without permanently reserving
 * that much RAM in this probe.
 */
#define FLIPSCOPE_RING_BYTES (16u * 1024u)
#define FLIPSCOPE_RING_CAPACITY (FLIPSCOPE_RING_BYTES / sizeof(int32_t))
#define FLIPSCOPE_CANDIDATE_B_BYTES (32u * 1024u)

/*
 * Pre-allocated pulse ring buffer. Entries store the raw (level, duration)
 * pair verbatim using the .sub RAW signed-duration convention (positive =
 * high, negative = low), exactly like the D0.2 test vector and the RAW
 * capture format - no normalization, filtering, or merging (PRD §4.5).
 *
 * write_index/total_pulses/overflow_drops are written only from the async
 * RX callback (interrupt context) and read only from the UI thread for
 * display; they are volatile so the compiler never caches a stale value
 * across that boundary. This is a single-writer/single-reader counter
 * pattern - exact synchronization is not required for an approximate live
 * display, and the values are frozen (no longer written) once RX stops.
 */
typedef struct {
    int32_t* entries;
    uint32_t capacity;
    volatile uint32_t write_index;
    volatile uint32_t total_pulses;
    volatile uint32_t overflow_drops;
} FlipScopeRingBuffer;

/* Free-heap measurements taken around the two FR-031 candidate allocations. */
typedef struct {
    size_t free_heap_baseline;
    size_t max_free_block_baseline;
    size_t free_heap_after_16k;
    bool candidate_16k_ok;
    size_t free_heap_after_32k_alloc;
    bool candidate_32k_ok;
    size_t free_heap_after_32k_free;
} FlipScopeRamProbe;

typedef struct {
    const SubGhzDevice* device;
    bool devices_inited;
    bool acquired; /* get_by_name succeeded; gates sleep()/end() in stop() */
    bool frequency_valid;
    uint32_t real_frequency;
    bool rx_started; /* currently active; guards stop() against double-stop */
    bool rx_entered; /* ever successfully started; sticky for the result screen */
} FlipScopeRadio;

typedef enum {
    FlipScopeStateRunning,
    FlipScopeStateStopped,
} FlipScopeState;

typedef struct {
    FuriMessageQueue* input_queue;
    FlipScopeRamProbe ram;
    FlipScopeRingBuffer ring;
    FlipScopeRadio radio;
    FlipScopeState state;
    bool pass;
} FlipScopeApp;

/*
 * Async RX capture callback - runs in interrupt context (installed via
 * subghz_devices_start_async_rx). It may ONLY read its arguments, write
 * into the pre-allocated ring buffer through a volatile index, and
 * increment plain counters. No furi_* calls, no allocation, no locking, no
 * logging here - all of those are unsafe or unbounded-latency from an ISR.
 */
static void flipscope_capture_callback(bool level, uint32_t duration, void* context) {
    FlipScopeRingBuffer* ring = context;

    uint32_t index = ring->write_index;
    if(index < ring->capacity) {
        /* Verbatim raw value, RAW-file signed-duration convention. */
        ring->entries[index] = level ? (int32_t)duration : -(int32_t)duration;
        ring->write_index = index + 1;
    } else {
        ring->overflow_drops++;
    }
    ring->total_pulses++;
}

/*
 * RAM probe: measure free heap before, during, and after allocating the
 * two FR-031 candidate buffer sizes. Candidate A becomes the live RX ring
 * buffer (freed only at app exit); candidate B is allocated, measured, and
 * freed immediately since this probe only needs to know it fits.
 */
static void flipscope_run_ram_probe(FlipScopeRamProbe* ram, FlipScopeRingBuffer* ring) {
    memset(ram, 0, sizeof(*ram));
    memset(ring, 0, sizeof(*ring));

    ram->free_heap_baseline = memmgr_get_free_heap();
    ram->max_free_block_baseline = memmgr_heap_get_max_free_block();
    FURI_LOG_I(
        TAG,
        "D0.3 probe: free heap baseline = %zu bytes (max free block %zu bytes)",
        ram->free_heap_baseline,
        ram->max_free_block_baseline);

    /* Candidate A: >=16 KiB, kept live as the real pre-arm RX ring buffer. */
    ring->capacity = FLIPSCOPE_RING_CAPACITY;
    ring->entries = malloc(FLIPSCOPE_RING_BYTES);
    ram->candidate_16k_ok = (ring->entries != NULL);
    ram->free_heap_after_16k = memmgr_get_free_heap();
    FURI_LOG_I(
        TAG,
        "D0.3 probe: 16KiB pre-arm candidate alloc %s, free heap now %zu bytes",
        ram->candidate_16k_ok ? "OK" : "FAILED",
        ram->free_heap_after_16k);
    if(!ram->candidate_16k_ok) {
        ring->capacity = 0;
    }

    /* Candidate B: >=32 KiB, probed only (allocate, measure, free). */
    void* candidate_b = malloc(FLIPSCOPE_CANDIDATE_B_BYTES);
    ram->candidate_32k_ok = (candidate_b != NULL);
    ram->free_heap_after_32k_alloc = memmgr_get_free_heap();
    FURI_LOG_I(
        TAG,
        "D0.3 probe: 32KiB pre-arm candidate alloc %s, free heap now %zu bytes",
        ram->candidate_32k_ok ? "OK" : "FAILED",
        ram->free_heap_after_32k_alloc);
    free(candidate_b);
    ram->free_heap_after_32k_free = memmgr_get_free_heap();
    FURI_LOG_I(
        TAG,
        "D0.3 probe: 32KiB pre-arm candidate freed, free heap restored to %zu bytes",
        ram->free_heap_after_32k_free);

    FURI_LOG_I(
        TAG,
        "D0.3 probe: FR-031 affordability - baseline=%zuB 16K:%s(kept, free=%zuB) "
        "32K:%s(probed, free_after_alloc=%zuB, restored=%zuB)",
        ram->free_heap_baseline,
        ram->candidate_16k_ok ? "OK" : "NO",
        ram->free_heap_after_16k,
        ram->candidate_32k_ok ? "OK" : "NO",
        ram->free_heap_after_32k_alloc,
        ram->free_heap_after_32k_free);
}

/*
 * Acquire the internal CC1101 radio via the stable subghz_devices API,
 * load a stock OOK async preset, tune to 433.92 MHz, and start async RX
 * into the pre-allocated ring buffer. Returns false (radio left in a safe,
 * released state) if any step fails.
 */
static bool flipscope_radio_start(FlipScopeRadio* radio, FlipScopeRingBuffer* ring) {
    memset(radio, 0, sizeof(*radio));

    subghz_devices_init();
    radio->devices_inited = true;

    radio->device = subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_INT_NAME);
    if(radio->device == NULL) {
        FURI_LOG_E(
            TAG,
            "D0.3 probe: subghz_devices_get_by_name(%s) returned NULL",
            SUBGHZ_DEVICE_CC1101_INT_NAME);
        return false;
    }
    radio->acquired = true;

    /*
     * subghz_devices_begin() is still called here for lifecycle symmetry
     * with subghz_devices_end() below, but its return value must NOT gate
     * setup for the internal CC1101. In official firmware 1.4.3 (this
     * repo's exact SDK, API 87.1), cc1101_int's interconnect declares
     * `.begin = NULL` (lib/subghz/devices/cc1101_int/cc1101_int_interconnect.c),
     * and subghz_devices_begin() (lib/subghz/devices/devices.c) returns
     * false whenever a device's begin hook is absent - false means "this
     * device has no begin step", not "begin failed". The stock Sub-GHz app
     * only inspects this return to detect an external radio module (which
     * DOES have a begin hook); it is never a failure signal for the
     * always-present internal radio. Confirmed on hardware (D0.3b run):
     * three launches all logged this as false and RX never armed while the
     * old code treated it as fatal, even though the RAM probe passed.
     */
    bool began_ok = subghz_devices_begin(radio->device);
    FURI_LOG_I(
        TAG,
        "D0.3 probe: subghz_devices_begin returned %s (expected false for "
        "cc1101_int - not fatal, see comment above)",
        began_ok ? "true" : "false");

    subghz_devices_reset(radio->device);
    subghz_devices_idle(radio->device);

    /* Stock async OOK preset, default register table (preset_data = NULL
     * is only meaningful for FuriHalSubGhzPresetCustom). */
    subghz_devices_load_preset(radio->device, FuriHalSubGhzPresetOok650Async, NULL);
    FURI_LOG_I(TAG, "D0.3 probe: loaded FuriHalSubGhzPresetOok650Async");

    radio->frequency_valid = subghz_devices_is_frequency_valid(radio->device, FLIPSCOPE_FREQUENCY_HZ);
    FURI_LOG_I(
        TAG,
        "D0.3 probe: %lu Hz frequency_valid=%s",
        (unsigned long)FLIPSCOPE_FREQUENCY_HZ,
        radio->frequency_valid ? "true" : "false");
    if(!radio->frequency_valid) {
        FURI_LOG_E(TAG, "D0.3 probe: refusing to tune to an invalid frequency");
        return false;
    }

    radio->real_frequency = subghz_devices_set_frequency(radio->device, FLIPSCOPE_FREQUENCY_HZ);
    FURI_LOG_I(
        TAG,
        "D0.3 probe: tuned, device reports real frequency = %lu Hz",
        (unsigned long)radio->real_frequency);

    if(ring->entries == NULL) {
        FURI_LOG_E(
            TAG, "D0.3 probe: no pre-arm ring buffer available (16KiB alloc failed); skipping RX");
        return false;
    }

    subghz_devices_flush_rx(radio->device);
    subghz_devices_set_rx(radio->device);
    subghz_devices_start_async_rx(radio->device, flipscope_capture_callback, ring);
    radio->rx_started = true;
    radio->rx_entered = true;
    FURI_LOG_I(
        TAG,
        "D0.3 probe: async RX started at %lu Hz under FuriHalSubGhzPresetOok650Async",
        (unsigned long)radio->real_frequency);
    return true;
}

/* Stop RX (if active), sleep and release the radio, and deinit the device
 * registry. Guarded so repeated calls (e.g. Back-then-Back) never fault. */
static void flipscope_radio_stop(FlipScopeRadio* radio) {
    if(radio->rx_started) {
        subghz_devices_stop_async_rx(radio->device);
        radio->rx_started = false;
        FURI_LOG_I(TAG, "D0.3 probe: async RX stopped");
    }
    if(radio->acquired) {
        subghz_devices_sleep(radio->device);
        subghz_devices_end(radio->device);
        radio->acquired = false;
        FURI_LOG_I(TAG, "D0.3 probe: radio released and put to sleep");
    }
    if(radio->devices_inited) {
        subghz_devices_deinit();
        radio->devices_inited = false;
    }
}

static bool flipscope_compute_pass(const FlipScopeApp* app) {
    return app->radio.rx_entered && app->ring.total_pulses > 0 && app->ram.candidate_16k_ok &&
           app->ram.candidate_32k_ok;
}

/*
 * Row y-coordinates for the 128x64 canvas. Six rows of text can appear at
 * once (title, subtitle, three data rows, back), so rows are spaced
 * tightly enough that the last row (FLIPSCOPE_ROW_BACK_Y, AlignTop) still
 * starts at y<=54 - leaving its ~10px-tall glyphs fully inside the 64px
 * canvas instead of clipped off the bottom (learned in D0.2).
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
        canvas, 2, FLIPSCOPE_ROW_TITLE_Y, AlignLeft, AlignTop, "FlipScope D0.3");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(
        canvas, 2, FLIPSCOPE_ROW_SUBTITLE_Y, AlignLeft, AlignTop, "Async RX pulse probe");

    char row1[24];
    if(app->state == FlipScopeStateStopped) {
        snprintf(row1, sizeof(row1), "Result: %s", app->pass ? "PASS" : "FAIL");
    } else if(app->radio.rx_started) {
        snprintf(row1, sizeof(row1), "Status: RX armed");
    } else {
        snprintf(row1, sizeof(row1), "Status: RX failed");
    }
    canvas_draw_str_aligned(canvas, 2, FLIPSCOPE_ROW_1_Y, AlignLeft, AlignTop, row1);

    char row2[24];
    snprintf(
        row2,
        sizeof(row2),
        "Pulses:%lu Drop:%lu",
        (unsigned long)app->ring.total_pulses,
        (unsigned long)app->ring.overflow_drops);
    canvas_draw_str_aligned(canvas, 2, FLIPSCOPE_ROW_2_Y, AlignLeft, AlignTop, row2);

    char row3[32];
    snprintf(
        row3,
        sizeof(row3),
        "16K:%s 32K:%s Free:%zuK",
        app->ram.candidate_16k_ok ? "OK" : "NO",
        app->ram.candidate_32k_ok ? "OK" : "NO",
        app->ram.free_heap_after_32k_free / 1024);
    canvas_draw_str_aligned(canvas, 2, FLIPSCOPE_ROW_3_Y, AlignLeft, AlignTop, row3);

    canvas_draw_str_aligned(
        canvas,
        2,
        FLIPSCOPE_ROW_BACK_Y,
        AlignLeft,
        AlignTop,
        app->state == FlipScopeStateRunning ? "Back = stop" : "Back = exit");
}

static void flipscope_input_callback(InputEvent* event, void* context) {
    FuriMessageQueue* input_queue = context;
    furi_message_queue_put(input_queue, event, FuriWaitForever);
}

int32_t flipscope_app(void* p) {
    UNUSED(p);

    FlipScopeApp app = {0};
    app.input_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    app.state = FlipScopeStateRunning;

    /* Pre-allocate the FR-031 candidate buffers before RX starts - this IS
     * the pre-arm pattern under test. */
    flipscope_run_ram_probe(&app.ram, &app.ring);

    if(!flipscope_radio_start(&app.radio, &app.ring)) {
        FURI_LOG_E(TAG, "D0.3 probe: radio setup did not reach active async RX");
    }

    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, flipscope_draw_callback, &app);
    view_port_input_callback_set(view_port, flipscope_input_callback, app.input_queue);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    InputEvent event;
    bool running = true;
    while(running) {
        /* Timed wait (not FuriWaitForever) so the live pulse/drop counts
         * on screen keep refreshing even without user input. */
        FuriStatus status = furi_message_queue_get(app.input_queue, &event, 100);
        if(status == FuriStatusOk && event.type == InputTypeShort && event.key == InputKeyBack) {
            if(app.state == FlipScopeStateRunning) {
                flipscope_radio_stop(&app.radio);
                app.pass = flipscope_compute_pass(&app);
                FURI_LOG_I(
                    TAG,
                    "D0.3 probe: stopped - pulses=%lu drops=%lu pass=%s",
                    (unsigned long)app.ring.total_pulses,
                    (unsigned long)app.ring.overflow_drops,
                    app.pass ? "true" : "false");
                app.state = FlipScopeStateStopped;
            } else {
                running = false;
            }
        }
        view_port_update(view_port);
    }

    /* Safety net: if the loop somehow exits from FlipScopeStateRunning
     * (it shouldn't via the input handling above), still leave the radio
     * stopped and released rather than fault on the next launch. */
    flipscope_radio_stop(&app.radio);

    gui_remove_view_port(gui, view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(view_port);
    furi_message_queue_free(app.input_queue);
    free(app.ring.entries);

    return 0;
}
