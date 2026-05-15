#include "state_share_client.h"
#include "../fs/moment_store.h"
#include "../image/image_filters.h"
#include "../image/image_frames.h"
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
// Share Client — discover hosts, browse remote moments, fetch them.
//
// Stages:
//   PHASE_PEERS    : scan + show peer list, A=connect to selected
//   PHASE_LIST     : connected, requesting & displaying remote moment list
//   PHASE_FETCH    : downloading a selected moment (and optionally its WAV)
//   PHASE_WAV_ASK  : asking the user whether to also download the missing WAV
//   PHASE_DONE     : fetch complete (status text)
//
// All stages can be aborted with KEY_B → unwind one stage (or back to menu).
// ---------------------------------------------------------------------------

#define VISIBLE_ROWS 7
#define MAX_REMOTE_ITEMS 32

typedef enum {
    PHASE_PEERS,
    PHASE_LIST_REQUEST,
    PHASE_LIST_COLLECT,
    PHASE_LIST_VIEW,
    PHASE_FETCH,
    PHASE_WAV_ASK,
    PHASE_WAV_FETCH,
    PHASE_DONE,
} Phase;

typedef struct {
    char filename[64];
    int  filter_id;
    int  frame_id;
    char music_name[64];
} RemoteItem;

static Phase        s_phase;
static char         s_status[96];

// Peers
static UdsPeerInfo  s_peers[UDS_MAX_PEERS];
static int          s_peer_count;
static int          s_peer_cursor;
static bool         s_scan_done;

// Remote list
static RemoteItem   s_items[MAX_REMOTE_ITEMS];
static int          s_item_count;
static int          s_item_cursor;
static int          s_item_scroll;
static uint16_t     s_list_total;     // total chunks expected
static int          s_list_received;
static uint64_t     s_list_deadline_us;

// Fetch session
static ShareRecv    s_recv;
static uint8_t     *s_recv_buf;
static size_t       s_recv_cap;
static uint32_t     s_next_req_id;
static char         s_pending_filename[64];
static char         s_pending_wav[64];

static void set_status(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void set_status(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(s_status, sizeof(s_status), fmt, ap);
    va_end(ap);
}

static uint32_t next_req_id(void)
{
    s_next_req_id++;
    if (s_next_req_id == 0) s_next_req_id = 1;
    return s_next_req_id;
}

static void free_recv_buf(void)
{
    if (s_recv_buf) { free(s_recv_buf); s_recv_buf = NULL; }
    s_recv_cap = 0;
}

// ---------------------------------------------------------------------------
// Phase: peer discovery
// ---------------------------------------------------------------------------
static void start_scan(void)
{
    s_peer_count  = 0;
    s_peer_cursor = 0;
    s_scan_done   = false;
    set_status("Scanning for hosts...");

    int n = uds_client_scan(s_peers, UDS_MAX_PEERS);
    if (n < 0) {
        s_peer_count = 0;
        set_status("Scan error.");
    } else {
        s_peer_count = n;
        if (n == 0) set_status("No hosts found. [A] rescan");
        else        set_status("Found %d host(s).", n);
    }
    s_scan_done = true;
}

static void connect_to_selected_peer(void)
{
    if (s_peer_cursor < 0 || s_peer_cursor >= s_peer_count) return;

    set_status("Connecting...");
    if (uds_client_connect(&s_peers[s_peer_cursor]) != 0) {
        set_status("Connect failed. [A] rescan");
        return;
    }
    s_phase = PHASE_LIST_REQUEST;
    set_status("Connected.");
}

// ---------------------------------------------------------------------------
// Phase: remote list
// ---------------------------------------------------------------------------
static void request_list(void)
{
    s_item_count    = 0;
    s_item_cursor   = 0;
    s_item_scroll   = 0;
    s_list_total    = 0;
    s_list_received = 0;
    uint32_t rid    = next_req_id();
    share_send(UDS_HOST_NETWORKNODEID, MSG_REQ_LIST, rid, 0, 0, NULL, 0);
    s_list_deadline_us = share_now_us() + 5ULL * 1000000ULL; // 5s
    s_phase = PHASE_LIST_COLLECT;
    set_status("Requesting list...");
}

static void poll_list_response(void)
{
    uint8_t frame[SHARE_FRAME_MAX];
    uint16_t src = 0;
    for (int i = 0; i < 8; i++) {
        int n = uds_recv_frame(frame, sizeof(frame), &src, 0);
        if (n <= 0) break;
        ShareHeader h;
        const uint8_t *payload = NULL;
        if (!share_unpack(frame, (size_t)n, &h, &payload)) continue;

        if (h.type == MSG_RESP_LIST_ITEM &&
            h.payload_len == SHARE_LIST_ITEM_SIZE &&
            s_item_count < MAX_REMOTE_ITEMS) {
            RemoteItem *it = &s_items[s_item_count];
            share_list_item_unpack(payload, it->filename, &it->filter_id,
                                   &it->frame_id, it->music_name);
            s_item_count++;
            s_list_total = h.total;
            s_list_received++;
        } else if (h.type == MSG_RESP_LIST_END) {
            s_phase = PHASE_LIST_VIEW;
            set_status((s_item_count > 0)
                       ? "Pick a moment to download."
                       : "Host has no moments.");
            return;
        }
    }

    if (share_now_us() >= s_list_deadline_us) {
        s_phase = PHASE_LIST_VIEW;
        set_status("List timeout (%d/%d items).",
                   s_item_count, (int)s_list_total);
    }
}

// ---------------------------------------------------------------------------
// Phase: fetch a moment
// ---------------------------------------------------------------------------
static void start_fetch(const RemoteItem *item)
{
    free_recv_buf();
    s_recv_cap = 512 * 1024;
    s_recv_buf = (uint8_t *)malloc(s_recv_cap);
    if (!s_recv_buf) {
        set_status("Out of memory.");
        return;
    }

    strncpy(s_pending_filename, item->filename, sizeof(s_pending_filename) - 1);
    s_pending_filename[sizeof(s_pending_filename) - 1] = '\0';
    strncpy(s_pending_wav, item->music_name, sizeof(s_pending_wav) - 1);
    s_pending_wav[sizeof(s_pending_wav) - 1] = '\0';

    uint32_t rid = next_req_id();
    share_recv_begin(&s_recv, rid,
                     MSG_RESP_FETCH_CHUNK, MSG_RESP_FETCH_DONE,
                     s_recv_buf, s_recv_cap);
    s_recv.src_node = UDS_HOST_NETWORKNODEID;

    share_send(UDS_HOST_NETWORKNODEID, MSG_REQ_FETCH, rid, 0, 0,
               s_pending_filename, (uint16_t)strlen(s_pending_filename));

    s_phase = PHASE_FETCH;
    set_status("Downloading %s...", s_pending_filename);
}

static void start_wav_fetch(void)
{
    free_recv_buf();
    s_recv_cap = 8 * 1024 * 1024 + 1024; // matches audio cap
    s_recv_buf = (uint8_t *)malloc(s_recv_cap);
    if (!s_recv_buf) {
        set_status("WAV: out of memory.");
        s_phase = PHASE_DONE;
        return;
    }

    uint32_t rid = next_req_id();
    share_recv_begin(&s_recv, rid,
                     MSG_RESP_WAV_CHUNK, MSG_RESP_WAV_DONE,
                     s_recv_buf, s_recv_cap);
    s_recv.src_node = UDS_HOST_NETWORKNODEID;

    share_send(UDS_HOST_NETWORKNODEID, MSG_REQ_WAV, rid, 0, 0,
               s_pending_wav, (uint16_t)strlen(s_pending_wav));

    s_phase = PHASE_WAV_FETCH;
    set_status("Downloading WAV %s...", s_pending_wav);
}

static void poll_fetch(void)
{
    uint8_t frame[SHARE_FRAME_MAX];
    uint16_t src = 0;
    for (int i = 0; i < 8; i++) {
        int n = uds_recv_frame(frame, sizeof(frame), &src, 0);
        if (n <= 0) break;
        ShareHeader h;
        const uint8_t *payload = NULL;
        if (!share_unpack(frame, (size_t)n, &h, &payload)) continue;

        // WAV-specific reply: missing file
        if (s_phase == PHASE_WAV_FETCH && h.type == MSG_RESP_WAV_MISSING) {
            set_status("Host doesn't have the WAV.");
            s_phase = PHASE_DONE;
            return;
        }
        share_recv_feed(&s_recv, &h, payload, src);
    }

    share_recv_tick(&s_recv);

    if (s_recv.state == SHARE_RX_DONE) {
        if (s_phase == PHASE_FETCH) {
            int rc = moment_store_write_bytes(s_pending_filename,
                                              s_recv_buf, s_recv.bytes_recv);
            if (rc != 0) {
                set_status("Save failed: %s", s_pending_filename);
                s_phase = PHASE_DONE;
                return;
            }
            // Need WAV too?
            if (s_pending_wav[0] != '\0') {
                char path[256];
                if (moment_store_resolve_wav(s_pending_wav, path,
                                             sizeof(path)) == 0) {
                    FILE *f = fopen(path, "rb");
                    if (f) {
                        fclose(f);
                        set_status("Saved %s.", s_pending_filename);
                        s_phase = PHASE_DONE;
                        return;
                    }
                }
                s_phase = PHASE_WAV_ASK;
                set_status("Music %s missing locally.", s_pending_wav);
            } else {
                set_status("Saved %s.", s_pending_filename);
                s_phase = PHASE_DONE;
            }
        } else if (s_phase == PHASE_WAV_FETCH) {
            int rc = moment_store_write_wav(s_pending_wav,
                                            s_recv_buf, s_recv.bytes_recv);
            set_status((rc == 0) ? "Saved %s + WAV." : "WAV save failed.",
                       s_pending_filename);
            s_phase = PHASE_DONE;
        }
    } else if (s_recv.state == SHARE_RX_FAILED) {
        set_status("Download timeout.");
        s_phase = PHASE_DONE;
    }
}

// ---------------------------------------------------------------------------
// AppState vtable
// ---------------------------------------------------------------------------
static void share_client_enter(AppState *self, AppContext *ctx)
{
    (void)self;
    (void)ctx;
    s_phase       = PHASE_PEERS;
    s_peer_count  = 0;
    s_peer_cursor = 0;
    s_item_count  = 0;
    s_item_cursor = 0;
    s_item_scroll = 0;
    s_recv_buf    = NULL;
    s_recv_cap    = 0;
    s_next_req_id = 1;
    s_pending_filename[0] = '\0';
    s_pending_wav[0]      = '\0';
    set_status("Press [A] to scan.");
    s_scan_done = false;
}

static void share_client_exit(AppState *self, AppContext *ctx)
{
    (void)self;
    (void)ctx;
    free_recv_buf();
    uds_client_disconnect();
}

static void update_peers_phase(AppContext *ctx)
{
    u32 kDown = hidKeysDown();
    if (!s_scan_done) {
        if (kDown & KEY_A) start_scan();
        return;
    }

    if (s_peer_count == 0) {
        if (kDown & KEY_A) start_scan();
        return;
    }
    if (kDown & KEY_DOWN && s_peer_cursor < s_peer_count - 1) s_peer_cursor++;
    if (kDown & KEY_UP   && s_peer_cursor > 0)               s_peer_cursor--;
    if (kDown & KEY_A) connect_to_selected_peer();
    if (kDown & KEY_X) start_scan();
    (void)ctx;
}

static void update_list_view_phase(AppContext *ctx)
{
    (void)ctx;
    u32 kDown = hidKeysDown();
    if (s_item_count == 0) return;

    if (kDown & KEY_DOWN) {
        if (s_item_cursor < s_item_count - 1) {
            s_item_cursor++;
            if (s_item_cursor >= s_item_scroll + VISIBLE_ROWS) s_item_scroll++;
        }
    }
    if (kDown & KEY_UP) {
        if (s_item_cursor > 0) {
            s_item_cursor--;
            if (s_item_cursor < s_item_scroll) s_item_scroll--;
        }
    }
    if (kDown & KEY_A) start_fetch(&s_items[s_item_cursor]);
}

static void update_wav_ask_phase(void)
{
    u32 kDown = hidKeysDown();
    if (kDown & KEY_A) start_wav_fetch();
    if (kDown & KEY_B) {
        set_status("Saved %s without music.", s_pending_filename);
        s_phase = PHASE_DONE;
    }
}

static void share_client_update(AppState *self, AppContext *ctx)
{
    (void)self;

    switch (s_phase) {
        case PHASE_PEERS:        update_peers_phase(ctx); break;
        case PHASE_LIST_REQUEST: request_list(); break;
        case PHASE_LIST_COLLECT: poll_list_response(); break;
        case PHASE_LIST_VIEW:    update_list_view_phase(ctx); break;
        case PHASE_FETCH:        poll_fetch(); break;
        case PHASE_WAV_ASK:      update_wav_ask_phase(); break;
        case PHASE_WAV_FETCH:    poll_fetch(); break;
        case PHASE_DONE:         break;
    }

    u32 kDown = hidKeysDown();
    if (kDown & KEY_B) {
        // From DONE / LIST_VIEW we go straight back to menu (disconnects in exit).
        state_manager_transition(ctx,
                                 app_next_state(ctx->current_state, TRIGGER_KEY_B));
    }
}

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------
static void render_top_default(C3D_RenderTarget *target, const char *title,
                               float pct)
{
    u32 clrBg     = C2D_Color32(0x2D, 0x1B, 0x3D, 0xFF);
    u32 clrAccent = C2D_Color32(0xE8, 0x6D, 0x50, 0xFF);
    u32 clrGold   = C2D_Color32(0xF2, 0xC1, 0x4E, 0xFF);

    C2D_TargetClear(target, clrBg);
    C2D_SceneBegin(target);

    C2D_DrawRectSolid(0, 0, 0, TOP_SCREEN_W, 4, clrAccent);
    C2D_DrawRectSolid(0, TOP_SCREEN_H - 4, 0, TOP_SCREEN_W, 4, clrAccent);

    float cx = TOP_SCREEN_W / 2.0f;
    float cy = TOP_SCREEN_H / 2.0f;

    ui_draw_centered(cx, cy - 40.0f, 0.0f, 0.65f, clrGold, "%s", title);
    ui_draw_centered(cx, cy + 4.0f,  0.0f, 0.40f,
                     C2D_Color32(0xF5, 0xF0, 0xE8, 0xFF), "%s", s_status);

    if (pct >= 0.0f) {
        float bar_w = 220.0f;
        C2D_DrawRectSolid(cx - bar_w/2, cy + 50.0f, 0, bar_w, 8,
                          C2D_Color32(0x1A, 0x0F, 0x25, 0xFF));
        C2D_DrawRectSolid(cx - bar_w/2, cy + 50.0f, 0, bar_w * pct, 8, clrGold);
    }
}

static void share_client_render_top(AppState *self, AppContext *ctx)
{
    (void)self;

    float pct = -1.0f;
    const char *title = "Discover";
    switch (s_phase) {
        case PHASE_LIST_REQUEST:
        case PHASE_LIST_COLLECT:
        case PHASE_LIST_VIEW:
            title = "Connected"; break;
        case PHASE_FETCH:
            title = "Downloading";
            if (s_recv.total_seqs > 0)
                pct = (float)s_recv.next_seq / (float)s_recv.total_seqs;
            break;
        case PHASE_WAV_FETCH:
            title = "Downloading WAV";
            if (s_recv.total_seqs > 0)
                pct = (float)s_recv.next_seq / (float)s_recv.total_seqs;
            break;
        case PHASE_WAV_ASK: title = "Music missing"; break;
        case PHASE_DONE:    title = "Done";          break;
        default: break;
    }
    render_top_default(ctx->top_target, title, pct);
}

static void render_peers_bottom(void)
{
    ui_panel_title("Discover — Peers");

    if (!s_scan_done) {
        ui_draw_centered(BOTTOM_W * 0.5f, 100.0f, 0.0f, 0.50f,
                         ui_color_text(), "Press [A] to scan");
        ui_panel_footer_hint("[A] Scan   [B] Back");
        return;
    }

    if (s_peer_count == 0) {
        ui_draw_centered(BOTTOM_W * 0.5f, 80.0f, 0.0f, 0.50f,
                         ui_color_accent(), "No hosts found");
        ui_draw_centered(BOTTOM_W * 0.5f, 110.0f, 0.0f, 0.40f,
                         ui_color_dim(), "Make sure peer is on Host screen");
        ui_panel_footer_hint("[A] Rescan   [B] Back");
        return;
    }

    int end = s_peer_count;
    for (int i = 0; i < end; i++) {
        char row[96];
        snprintf(row, sizeof(row), "%s",
                 (s_peers[i].username[0] != '\0')
                 ? s_peers[i].username : "(host)");
        ui_panel_list_row(i, i == s_peer_cursor, row);
    }

    char hint[64];
    snprintf(hint, sizeof(hint), "[A] Connect  [X] Rescan  [B] Back  %d/%d",
             s_peer_cursor + 1, s_peer_count);
    ui_panel_footer_hint(hint);
}

static void render_list_bottom(void)
{
    ui_panel_title("Discover — Remote");

    if (s_phase == PHASE_LIST_REQUEST || s_phase == PHASE_LIST_COLLECT) {
        ui_draw_centered(BOTTOM_W * 0.5f, 90.0f, 0.0f, 0.50f,
                         ui_color_text(), "Receiving list...");
        ui_draw_centered(BOTTOM_W * 0.5f, 116.0f, 0.0f, 0.40f,
                         ui_color_dim(), "%d / %d",
                         s_item_count, (int)s_list_total);
        ui_panel_footer_hint("[B] Back");
        return;
    }

    if (s_item_count == 0) {
        ui_draw_centered(BOTTOM_W * 0.5f, 100.0f, 0.0f, 0.50f,
                         ui_color_accent(), "Host has no moments");
        ui_panel_footer_hint("[B] Back");
        return;
    }

    int end = s_item_scroll + VISIBLE_ROWS;
    if (end > s_item_count) end = s_item_count;
    for (int i = s_item_scroll; i < end; i++) {
        RemoteItem *m = &s_items[i];
        char row[128];
        if (m->music_name[0] != '\0') {
            snprintf(row, sizeof(row), "%s | %s | %s",
                     image_filter_name(m->filter_id),
                     image_frame_name(m->frame_id),
                     m->music_name);
        } else {
            snprintf(row, sizeof(row), "%s | %s | (no track)",
                     image_filter_name(m->filter_id),
                     image_frame_name(m->frame_id));
        }
        ui_panel_list_row(i - s_item_scroll, i == s_item_cursor, row);
    }

    char hint[64];
    snprintf(hint, sizeof(hint), "[A] Download   [B] Back   %d/%d",
             s_item_cursor + 1, s_item_count);
    ui_panel_footer_hint(hint);
}

static void render_progress_bottom(const char *header)
{
    ui_panel_title(header);
    ui_draw_centered(BOTTOM_W * 0.5f, 90.0f, 0.0f, 0.45f,
                     ui_color_text(), "%s", s_status);
    if (s_recv.total_seqs > 0) {
        ui_draw_centered(BOTTOM_W * 0.5f, 120.0f, 0.0f, 0.40f,
                         ui_color_dim(), "%u / %u KB",
                         (unsigned)(s_recv.bytes_recv / 1024),
                         (unsigned)(((size_t)s_recv.total_seqs *
                                     SHARE_PAYLOAD_MAX) / 1024));
    }
    ui_panel_footer_hint("[B] Cancel");
}

static void render_wav_ask_bottom(void)
{
    ui_panel_title("Music missing");
    ui_draw_centered(BOTTOM_W * 0.5f, 80.0f, 0.0f, 0.45f, ui_color_text(),
                     "%s", s_pending_wav);
    ui_draw_centered(BOTTOM_W * 0.5f, 110.0f, 0.0f, 0.40f, ui_color_dim(),
                     "Download from peer?");
    ui_panel_footer_hint("[A] Yes   [B] No");
}

static void render_done_bottom(void)
{
    ui_panel_title("Done");
    ui_draw_centered(BOTTOM_W * 0.5f, 100.0f, 0.0f, 0.45f, ui_color_text(),
                     "%s", s_status);
    ui_panel_footer_hint("[B] Back");
}

static void share_client_render_bottom(AppState *self, AppContext *ctx)
{
    (void)self;
    (void)ctx;

    ui_panel_bg_dark();
    switch (s_phase) {
        case PHASE_PEERS:        render_peers_bottom();             break;
        case PHASE_LIST_REQUEST:
        case PHASE_LIST_COLLECT:
        case PHASE_LIST_VIEW:    render_list_bottom();              break;
        case PHASE_FETCH:        render_progress_bottom("Downloading");     break;
        case PHASE_WAV_FETCH:    render_progress_bottom("Downloading WAV"); break;
        case PHASE_WAV_ASK:      render_wav_ask_bottom();           break;
        case PHASE_DONE:         render_done_bottom();              break;
    }
}

static AppState s_share_client = {
    .uses_direct_framebuffer = false,
    .enter         = share_client_enter,
    .exit          = share_client_exit,
    .update        = share_client_update,
    .render_top    = share_client_render_top,
    .render_bottom = share_client_render_bottom,
};

AppState *state_share_client_create(void) { return &s_share_client; }
