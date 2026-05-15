#include "state_share_host.h"
#include "../fs/moment_store.h"
#include "../net/share_protocol.h"
#include "../net/uds_transport.h"
#include "../state/state_manager.h"
#include "../state/transitions.h"
#include "../ui/ui_panel.h"
#include "../ui/ui_text.h"
#include <3ds.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Share Host — advertises a UDS network and serves one client.
//
// MVP scope: handles a single client at a time. The state machine reacts to
// incoming requests (REQ_LIST / REQ_FETCH / REQ_WAV / REQ_UPLOAD_*) by
// switching its `mode` and using ShareSend / ShareRecv until the transfer
// completes, then returns to MODE_IDLE waiting for the next request.
// ---------------------------------------------------------------------------

typedef enum {
    MODE_IDLE,
    MODE_SEND_FETCH,
    MODE_SEND_WAV,
    MODE_RECV_UPLOAD,
} HostMode;

static bool       s_started = false;
static bool       s_failed  = false;
static char       s_status[96];
static HostMode   s_mode    = MODE_IDLE;

static ShareSend  s_send;
static ShareRecv  s_recv;
static uint8_t   *s_send_buf = NULL;     // owned by malloc
static size_t     s_send_len = 0;
static uint8_t   *s_recv_buf = NULL;
static size_t     s_recv_cap = 0;
static char       s_upload_filename[64];

static void set_status(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void set_status(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(s_status, sizeof(s_status), fmt, ap);
    va_end(ap);
}

static void clear_send_buf(void)
{
    if (s_send_buf) { free(s_send_buf); s_send_buf = NULL; }
    s_send_len = 0;
}
static void clear_recv_buf(void)
{
    if (s_recv_buf) { free(s_recv_buf); s_recv_buf = NULL; }
    s_recv_cap = 0;
}

static void return_to_idle(void)
{
    s_mode = MODE_IDLE;
    clear_send_buf();
    clear_recv_buf();
    s_upload_filename[0] = '\0';
    set_status("Ready, waiting for request...");
}

// ---------------------------------------------------------------------------
// Request handlers
// ---------------------------------------------------------------------------
static void handle_req_list(uint16_t src, uint32_t req_id)
{
    static MomentInfo items[MAX_MOMENTS];
    int n = moment_store_list(items, MAX_MOMENTS);

    set_status("Sending list (%d items)...", n);

    for (int i = 0; i < n; i++) {
        uint8_t payload[SHARE_LIST_ITEM_SIZE];
        share_list_item_pack(payload, items[i].filename,
                             items[i].filter_id, items[i].frame_id,
                             items[i].music_name);
        share_send(src, MSG_RESP_LIST_ITEM, req_id,
                   (uint16_t)i, (uint16_t)n,
                   payload, SHARE_LIST_ITEM_SIZE);
        // Tiny gap so we don't overrun the peer's recv buffer.
        svcSleepThread(2000000LL); // 2 ms
    }
    share_send(src, MSG_RESP_LIST_END, req_id, 0, 0, NULL, 0);
    set_status("List sent (%d). Waiting...", n);
}

static void handle_req_fetch(uint16_t src, uint32_t req_id,
                             const uint8_t *payload, uint16_t payload_len)
{
    char filename[64];
    size_t n = (payload_len < sizeof(filename) - 1)
               ? payload_len : sizeof(filename) - 1;
    memcpy(filename, payload, n);
    filename[n] = '\0';

    if (moment_store_read_bytes(filename, &s_send_buf, &s_send_len) != 0) {
        share_send(src, MSG_ERR, req_id, 0, 0, NULL, 0);
        set_status("Fetch failed: %s", filename);
        return_to_idle();
        return;
    }

    s_mode = MODE_SEND_FETCH;
    share_send_begin(&s_send, src, req_id,
                     MSG_RESP_FETCH_CHUNK, MSG_RESP_FETCH_DONE,
                     s_send_buf, s_send_len);
    set_status("Sending %s (%u KB)...", filename,
               (unsigned)(s_send_len / 1024));
}

static void handle_req_wav(uint16_t src, uint32_t req_id,
                           const uint8_t *payload, uint16_t payload_len)
{
    char basename[64];
    size_t n = (payload_len < sizeof(basename) - 1)
               ? payload_len : sizeof(basename) - 1;
    memcpy(basename, payload, n);
    basename[n] = '\0';

    if (moment_store_read_wav(basename, &s_send_buf, &s_send_len) != 0) {
        share_send(src, MSG_RESP_WAV_MISSING, req_id, 0, 0, NULL, 0);
        set_status("WAV missing: %s", basename);
        return_to_idle();
        return;
    }

    s_mode = MODE_SEND_WAV;
    share_send_begin(&s_send, src, req_id,
                     MSG_RESP_WAV_CHUNK, MSG_RESP_WAV_DONE,
                     s_send_buf, s_send_len);
    set_status("Sending WAV %s (%u KB)...", basename,
               (unsigned)(s_send_len / 1024));
}

static void handle_req_upload_begin(uint16_t src, uint32_t req_id,
                                    const uint8_t *payload, uint16_t payload_len)
{
    char filename[64];
    size_t n = (payload_len < sizeof(filename) - 1)
               ? payload_len : sizeof(filename) - 1;
    memcpy(filename, payload, n);
    filename[n] = '\0';

    if (!moment_store_basename_safe(filename, ".moment")) {
        share_send(src, MSG_NAK, req_id, 0, 0, NULL, 0);
        set_status("Upload rejected: bad name");
        return_to_idle();
        return;
    }

    // Allocate generously enough for a typical .moment (~384 KB) plus slack.
    s_recv_cap = 512 * 1024;
    s_recv_buf = (uint8_t *)malloc(s_recv_cap);
    if (!s_recv_buf) {
        share_send(src, MSG_ERR, req_id, 0, 0, NULL, 0);
        set_status("Upload OOM");
        return_to_idle();
        return;
    }

    strncpy(s_upload_filename, filename, sizeof(s_upload_filename) - 1);
    s_upload_filename[sizeof(s_upload_filename) - 1] = '\0';

    s_mode = MODE_RECV_UPLOAD;
    share_recv_begin(&s_recv, req_id,
                     MSG_REQ_UPLOAD_CHUNK, MSG_REQ_UPLOAD_END,
                     s_recv_buf, s_recv_cap);
    s_recv.src_node = src;
    set_status("Receiving %s...", filename);
}

// ---------------------------------------------------------------------------
// Per-frame service loop
// ---------------------------------------------------------------------------
static void poll_and_dispatch(void)
{
    uint8_t frame[SHARE_FRAME_MAX];
    uint16_t src = 0;

    // Drain up to a few frames per tick so transfers progress quickly.
    for (int i = 0; i < 8; i++) {
        int n = uds_recv_frame(frame, sizeof(frame), &src, 0);
        if (n <= 0) break;

        ShareHeader h;
        const uint8_t *payload = NULL;
        if (!share_unpack(frame, (size_t)n, &h, &payload)) continue;

        // Route by current mode first (ACK / chunk traffic).
        if (s_mode == MODE_SEND_FETCH || s_mode == MODE_SEND_WAV) {
            if (h.type == MSG_ACK || h.type == MSG_NAK || h.type == MSG_ERR) {
                share_send_feed(&s_send, &h);
                continue;
            }
        }
        if (s_mode == MODE_RECV_UPLOAD) {
            if (h.type == MSG_REQ_UPLOAD_CHUNK || h.type == MSG_REQ_UPLOAD_END) {
                share_recv_feed(&s_recv, &h, payload, src);
                continue;
            }
        }

        // Otherwise treat as a new request.
        switch (h.type) {
            case MSG_REQ_LIST:
                handle_req_list(src, h.req_id);
                break;
            case MSG_REQ_FETCH:
                handle_req_fetch(src, h.req_id, payload, h.payload_len);
                break;
            case MSG_REQ_WAV:
                handle_req_wav(src, h.req_id, payload, h.payload_len);
                break;
            case MSG_REQ_UPLOAD_BEGIN:
                handle_req_upload_begin(src, h.req_id, payload, h.payload_len);
                break;
            default:
                break;
        }
    }
}

static void advance_session(void)
{
    if (s_mode == MODE_SEND_FETCH || s_mode == MODE_SEND_WAV) {
        share_send_tick(&s_send);
        if (s_send.state == SHARE_TX_DONE) {
            set_status("Send complete (%u KB).",
                       (unsigned)(s_send_len / 1024));
            return_to_idle();
        } else if (s_send.state == SHARE_TX_FAILED) {
            set_status("Send failed (timeout).");
            return_to_idle();
        }
    } else if (s_mode == MODE_RECV_UPLOAD) {
        share_recv_tick(&s_recv);
        if (s_recv.state == SHARE_RX_DONE) {
            int rc = moment_store_write_bytes(s_upload_filename,
                                              s_recv_buf, s_recv.bytes_recv);
            if (rc == 0) {
                set_status("Saved upload: %s (%u KB).", s_upload_filename,
                           (unsigned)(s_recv.bytes_recv / 1024));
            } else {
                set_status("Upload save failed: %s", s_upload_filename);
            }
            return_to_idle();
        } else if (s_recv.state == SHARE_RX_FAILED) {
            set_status("Receive failed (timeout).");
            return_to_idle();
        }
    }
}

// ---------------------------------------------------------------------------
// AppState vtable
// ---------------------------------------------------------------------------
static void share_host_enter(AppState *self, AppContext *ctx)
{
    (void)self;
    (void)ctx;
    s_started = (uds_host_start(1) == 0);
    s_failed  = !s_started;
    s_mode    = MODE_IDLE;
    s_send_buf = NULL;
    s_recv_buf = NULL;
    s_send_len = 0;
    s_recv_cap = 0;
    s_upload_filename[0] = '\0';
    set_status(s_started ? "Waiting for peer..."
                         : "Failed to start network");
}

static void share_host_exit(AppState *self, AppContext *ctx)
{
    (void)self;
    (void)ctx;
    clear_send_buf();
    clear_recv_buf();
    if (s_started) {
        uds_host_stop();
        s_started = false;
    }
}

static void share_host_update(AppState *self, AppContext *ctx)
{
    (void)self;

    if (s_started && !s_failed) {
        poll_and_dispatch();
        advance_session();
    }

    u32 kDown = hidKeysDown();
    if (kDown & KEY_B) {
        state_manager_transition(ctx,
                                 app_next_state(ctx->current_state, TRIGGER_KEY_B));
    }
}

static void share_host_render_top(AppState *self, AppContext *ctx)
{
    (void)self;

    C3D_RenderTarget *target = ctx->top_target;
    u32 clrBg     = C2D_Color32(0x2D, 0x1B, 0x3D, 0xFF);
    u32 clrAccent = C2D_Color32(0xE8, 0x6D, 0x50, 0xFF);
    u32 clrGold   = C2D_Color32(0xF2, 0xC1, 0x4E, 0xFF);

    C2D_TargetClear(target, clrBg);
    C2D_SceneBegin(target);

    C2D_DrawRectSolid(0, 0, 0, TOP_SCREEN_W, 4, clrAccent);
    C2D_DrawRectSolid(0, TOP_SCREEN_H - 4, 0, TOP_SCREEN_W, 4, clrAccent);

    float cx = TOP_SCREEN_W / 2.0f;
    float cy = TOP_SCREEN_H / 2.0f;

    ui_draw_centered(cx, cy - 40.0f, 0.0f, 0.65f, clrGold, "Hosting");

    int peers = uds_node_count();
    int clients = peers > 1 ? peers - 1 : 0;
    ui_draw_centered(cx, cy - 6.0f, 0.0f, 0.45f, clrAccent,
                     "Clients connected: %d", clients);
    ui_draw_centered(cx, cy + 18.0f, 0.0f, 0.40f,
                     C2D_Color32(0xF5, 0xF0, 0xE8, 0xFF), "%s", s_status);

    if (s_mode == MODE_SEND_FETCH || s_mode == MODE_SEND_WAV) {
        float pct = (s_send.total_seqs > 0)
                    ? (float)s_send.cur_seq / (float)s_send.total_seqs
                    : 0.0f;
        float bar_w = 220.0f;
        C2D_DrawRectSolid(cx - bar_w/2, cy + 50.0f, 0, bar_w, 8,
                          C2D_Color32(0x1A, 0x0F, 0x25, 0xFF));
        C2D_DrawRectSolid(cx - bar_w/2, cy + 50.0f, 0, bar_w * pct, 8, clrGold);
    } else if (s_mode == MODE_RECV_UPLOAD && s_recv.total_seqs > 0) {
        float pct = (float)s_recv.next_seq / (float)s_recv.total_seqs;
        float bar_w = 220.0f;
        C2D_DrawRectSolid(cx - bar_w/2, cy + 50.0f, 0, bar_w, 8,
                          C2D_Color32(0x1A, 0x0F, 0x25, 0xFF));
        C2D_DrawRectSolid(cx - bar_w/2, cy + 50.0f, 0, bar_w * pct, 8, clrGold);
    }
}

static void share_host_render_bottom(AppState *self, AppContext *ctx)
{
    (void)self;
    (void)ctx;

    ui_panel_bg_dark();
    ui_panel_title("Share — Host");

    if (s_failed) {
        ui_draw_centered(BOTTOM_W * 0.5f, ui_panel_content_top() + 20.0f,
                         0.0f, 0.50f, ui_color_accent(),
                         "Could not start network");
        ui_draw_centered(BOTTOM_W * 0.5f, ui_panel_content_top() + 50.0f,
                         0.0f, 0.40f, ui_color_dim(),
                         "Check Wi-Fi switch and try again");
    } else {
        ui_draw_centered(BOTTOM_W * 0.5f, 80.0f, 0.0f, 0.45f,
                         ui_color_text(), "%s", s_status);
        ui_draw_centered(BOTTOM_W * 0.5f, 110.0f, 0.0f, 0.40f,
                         ui_color_dim(),
                         "On the other 3DS, pick Discover");
    }

    ui_panel_footer_hint("[B] Stop & Back");
}

static AppState s_share_host = {
    .uses_direct_framebuffer = false,
    .enter         = share_host_enter,
    .exit          = share_host_exit,
    .update        = share_host_update,
    .render_top    = share_host_render_top,
    .render_bottom = share_host_render_bottom,
};

AppState *state_share_host_create(void) { return &s_share_host; }
