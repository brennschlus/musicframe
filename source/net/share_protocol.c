#include "share_protocol.h"

#include <3ds.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Little-endian field accessors (3DS is LE so memcpy works, but be explicit).
// ---------------------------------------------------------------------------
static inline void put_u16_le(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
}
static inline void put_u32_le(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
    p[2] = (uint8_t)((v >> 16) & 0xFF);
    p[3] = (uint8_t)((v >> 24) & 0xFF);
}
static inline uint16_t get_u16_le(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}
static inline uint32_t get_u32_le(const uint8_t *p) {
    return (uint32_t)p[0]        | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

// ---------------------------------------------------------------------------
size_t share_pack(uint8_t *out_buf, size_t cap,
                  ShareMsgType type, uint32_t req_id,
                  uint16_t seq, uint16_t total,
                  const void *payload, uint16_t payload_len)
{
    if (!out_buf) return 0;
    if (payload_len > SHARE_PAYLOAD_MAX) return 0;
    if (cap < (size_t)(SHARE_HEADER_SIZE + payload_len)) return 0;

    put_u32_le(out_buf + 0, SHARE_MAGIC);
    out_buf[4] = (uint8_t)type;
    out_buf[5] = 0; // flags
    put_u32_le(out_buf + 6,  req_id);
    put_u16_le(out_buf + 10, seq);
    put_u16_le(out_buf + 12, total);
    put_u16_le(out_buf + 14, payload_len);

    if (payload_len > 0 && payload) {
        memcpy(out_buf + SHARE_HEADER_SIZE, payload, payload_len);
    }
    return (size_t)SHARE_HEADER_SIZE + payload_len;
}

bool share_unpack(const uint8_t *buf, size_t len,
                  ShareHeader *out_header, const uint8_t **payload_out)
{
    if (!buf || len < SHARE_HEADER_SIZE || !out_header) return false;

    uint32_t magic = get_u32_le(buf + 0);
    if (magic != SHARE_MAGIC) return false;

    out_header->magic       = magic;
    out_header->type        = buf[4];
    out_header->flags       = buf[5];
    out_header->req_id      = get_u32_le(buf + 6);
    out_header->seq         = get_u16_le(buf + 10);
    out_header->total       = get_u16_le(buf + 12);
    out_header->payload_len = get_u16_le(buf + 14);

    if ((size_t)SHARE_HEADER_SIZE + out_header->payload_len > len) return false;
    if (out_header->payload_len > SHARE_PAYLOAD_MAX) return false;

    if (payload_out) {
        *payload_out = (out_header->payload_len > 0)
                       ? buf + SHARE_HEADER_SIZE : NULL;
    }
    return true;
}

int share_send(uint16_t dst_node, ShareMsgType type, uint32_t req_id,
               uint16_t seq, uint16_t total,
               const void *payload, uint16_t payload_len)
{
    uint8_t buf[SHARE_FRAME_MAX];
    size_t n = share_pack(buf, sizeof(buf), type, req_id, seq, total,
                          payload, payload_len);
    if (n == 0) return -1;
    return uds_send_frame(dst_node, buf, n);
}

// ---------------------------------------------------------------------------
uint64_t share_now_us(void)
{
    // svcGetSystemTick returns ticks at SYSCLOCK_ARM11 (268.111856 MHz).
    u64 ticks = svcGetSystemTick();
    // Avoid 64-bit floating-point: ticks * 1000000 / SYSCLOCK_ARM11
    // SYSCLOCK_ARM11 = 268111856
    return (uint64_t)((ticks * 1000000ULL) / 268111856ULL);
}

// ---------------------------------------------------------------------------
// List item packing
// ---------------------------------------------------------------------------
static void copy_field(char *dst, size_t dst_size, const char *src)
{
    memset(dst, 0, dst_size);
    if (src) {
        size_t n = strnlen(src, dst_size - 1);
        memcpy(dst, src, n);
    }
}

void share_list_item_pack(uint8_t out[SHARE_LIST_ITEM_SIZE],
                          const char *filename, int filter_id, int frame_id,
                          const char *music_name)
{
    memset(out, 0, SHARE_LIST_ITEM_SIZE);
    copy_field((char *)out + 0, 60, filename);
    put_u32_le(out + 60, (uint32_t)filter_id);
    put_u32_le(out + 64, (uint32_t)frame_id);
    copy_field((char *)out + 68, 60, music_name);
    // 132 bytes total (60 + 4 + 4 + 60 = 128, 4 bytes pad)
}

void share_list_item_unpack(const uint8_t in[SHARE_LIST_ITEM_SIZE],
                            char *filename_out, int *filter_id_out,
                            int *frame_id_out, char *music_name_out)
{
    if (filename_out) {
        memcpy(filename_out, in + 0, 60);
        filename_out[60] = '\0';
    }
    if (filter_id_out)   *filter_id_out = (int)get_u32_le(in + 60);
    if (frame_id_out)    *frame_id_out  = (int)get_u32_le(in + 64);
    if (music_name_out) {
        memcpy(music_name_out, in + 68, 60);
        music_name_out[60] = '\0';
    }
}

// ---------------------------------------------------------------------------
// Sender state machine (stop-and-wait, ACK per chunk)
// ---------------------------------------------------------------------------
static uint16_t compute_total(size_t data_len)
{
    if (data_len == 0) return 0;
    return (uint16_t)((data_len + SHARE_PAYLOAD_MAX - 1) / SHARE_PAYLOAD_MAX);
}

static void send_chunk(ShareSend *s)
{
    size_t off = (size_t)s->cur_seq * SHARE_PAYLOAD_MAX;
    size_t remaining = (off < s->data_len) ? s->data_len - off : 0;
    uint16_t chunk_len = (uint16_t)((remaining > SHARE_PAYLOAD_MAX)
                                    ? SHARE_PAYLOAD_MAX : remaining);

    share_send(s->dst_node, s->chunk_type, s->req_id,
               s->cur_seq, s->total_seqs,
               (const uint8_t *)s->data + off, chunk_len);
    s->deadline_us = share_now_us() + SHARE_TIMEOUT_US;
    s->state = SHARE_TX_AWAIT_ACK;
}

void share_send_begin(ShareSend *s, uint16_t dst_node, uint32_t req_id,
                      ShareMsgType chunk_type, ShareMsgType done_type,
                      const void *data, size_t data_len)
{
    if (!s) return;

    s->dst_node     = dst_node;
    s->req_id       = req_id;
    s->chunk_type   = chunk_type;
    s->done_type    = done_type;
    s->data         = (const uint8_t *)data;
    s->data_len     = data_len;
    s->cur_seq      = 0;
    s->total_seqs   = compute_total(data_len);
    s->retries_left = SHARE_MAX_RETRIES;
    s->state        = (s->total_seqs == 0)
                      ? SHARE_TX_SEND_DONE
                      : SHARE_TX_SEND_CHUNK;
    s->deadline_us  = 0;
}

void share_send_tick(ShareSend *s)
{
    if (!s) return;

    switch (s->state) {
        case SHARE_TX_SEND_CHUNK:
            send_chunk(s);
            break;

        case SHARE_TX_AWAIT_ACK:
            if (share_now_us() >= s->deadline_us) {
                if (--s->retries_left <= 0) {
                    s->state = SHARE_TX_FAILED;
                } else {
                    // Retransmit current chunk.
                    s->state = SHARE_TX_SEND_CHUNK;
                }
            }
            break;

        case SHARE_TX_SEND_DONE:
            share_send(s->dst_node, s->done_type, s->req_id,
                       0, 0, NULL, 0);
            s->state = SHARE_TX_DONE;
            break;

        default:
            break;
    }
}

void share_send_feed(ShareSend *s, const ShareHeader *h)
{
    if (!s || !h) return;
    if (s->state != SHARE_TX_AWAIT_ACK) return;
    if (h->req_id != s->req_id) return;
    if (h->type == MSG_NAK || h->type == MSG_ERR) {
        s->state = SHARE_TX_FAILED;
        return;
    }
    if (h->type != MSG_ACK) return;
    if (h->seq != s->cur_seq) return;

    s->cur_seq++;
    s->retries_left = SHARE_MAX_RETRIES;
    if (s->cur_seq >= s->total_seqs) {
        s->state = SHARE_TX_SEND_DONE;
    } else {
        s->state = SHARE_TX_SEND_CHUNK;
    }
}

// ---------------------------------------------------------------------------
// Receiver state machine
// ---------------------------------------------------------------------------
void share_recv_begin(ShareRecv *r, uint32_t req_id,
                      ShareMsgType chunk_type, ShareMsgType done_type,
                      uint8_t *buf, size_t cap)
{
    if (!r) return;
    r->req_id      = req_id;
    r->chunk_type  = chunk_type;
    r->done_type   = done_type;
    r->buf         = buf;
    r->cap         = cap;
    r->bytes_recv  = 0;
    r->next_seq    = 0;
    r->total_seqs  = 0;
    r->src_node    = 0;
    r->deadline_us = share_now_us() + SHARE_TIMEOUT_US * (SHARE_MAX_RETRIES + 1);
    r->state       = SHARE_RX_RECEIVING;
}

void share_recv_feed(ShareRecv *r, const ShareHeader *h,
                     const uint8_t *payload, uint16_t src_node)
{
    if (!r || !h) return;
    if (r->state != SHARE_RX_RECEIVING) return;
    if (h->req_id != r->req_id) return;

    if (h->type == MSG_NAK || h->type == MSG_ERR) {
        r->state = SHARE_RX_FAILED;
        return;
    }

    if (h->type == r->done_type) {
        r->state = SHARE_RX_DONE;
        return;
    }

    if (h->type != r->chunk_type) return;

    // Lock src_node on first chunk so out-of-band frames can't hijack the session.
    if (r->next_seq == 0) {
        r->src_node = src_node;
        r->total_seqs = h->total;
    }
    if (h->total != r->total_seqs) return;

    if (h->seq == r->next_seq) {
        size_t off = (size_t)r->next_seq * SHARE_PAYLOAD_MAX;
        if (off + h->payload_len > r->cap) {
            r->state = SHARE_RX_FAILED;
            return;
        }
        if (h->payload_len > 0 && payload) {
            memcpy(r->buf + off, payload, h->payload_len);
        }
        r->bytes_recv = off + h->payload_len;
        // ACK first, then advance — peer needs the ACK to send the next chunk.
        share_send(r->src_node, MSG_ACK, r->req_id, r->next_seq, 0, NULL, 0);
        r->next_seq++;
        r->deadline_us = share_now_us()
                         + SHARE_TIMEOUT_US * (SHARE_MAX_RETRIES + 1);
    } else if (h->seq + 1 == r->next_seq) {
        // Duplicate — re-ACK so the sender doesn't time out.
        share_send(r->src_node, MSG_ACK, r->req_id, h->seq, 0, NULL, 0);
    }
    // Out-of-order future seqs: ignore (sender will time out and retransmit).
}

void share_recv_tick(ShareRecv *r)
{
    if (!r || r->state != SHARE_RX_RECEIVING) return;
    if (share_now_us() >= r->deadline_us) {
        r->state = SHARE_RX_FAILED;
    }
}
