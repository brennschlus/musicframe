#ifndef SHARE_PROTOCOL_H
#define SHARE_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "uds_transport.h"

// ---------------------------------------------------------------------------
// share_protocol — application-layer protocol on top of uds_transport.
//
// All multi-byte fields are little-endian (3DS native). Payload max per
// packet is SHARE_PAYLOAD_MAX bytes; large blobs (.moment, .wav) are split
// into chunks numbered 0..total-1 and reassembled on the receiver side.
//
// The reliability layer is intentionally simple: stop-and-wait per chunk
// with a 500 ms timeout and 3 retries. UDS over local wireless is
// reasonably reliable in the same-room scenario; this is enough for MVP.
// ---------------------------------------------------------------------------

#define SHARE_MAGIC          0x4D465348u  // 'MFSH'
#define SHARE_HEADER_SIZE    16
#define SHARE_PAYLOAD_MAX    (UDS_TRANSPORT_MTU - SHARE_HEADER_SIZE)
#define SHARE_FRAME_MAX      UDS_TRANSPORT_MTU

#define SHARE_TIMEOUT_US     500000ULL    // 500 ms per ack
#define SHARE_MAX_RETRIES    3

typedef enum {
    MSG_NONE              = 0,
    MSG_REQ_LIST          = 1,
    MSG_RESP_LIST_ITEM    = 2,
    MSG_RESP_LIST_END     = 3,

    MSG_REQ_FETCH         = 10, // payload: filename (no NUL terminator required)
    MSG_RESP_FETCH_CHUNK  = 11, // payload: chunk bytes (seq, total set)
    MSG_RESP_FETCH_DONE   = 12,

    MSG_REQ_UPLOAD_BEGIN  = 20, // payload: filename
    MSG_REQ_UPLOAD_CHUNK  = 21,
    MSG_REQ_UPLOAD_END    = 22,

    MSG_REQ_WAV           = 30, // payload: basename
    MSG_RESP_WAV_CHUNK    = 31,
    MSG_RESP_WAV_DONE     = 32,
    MSG_RESP_WAV_MISSING  = 33,

    MSG_ACK               = 40, // for chunk seq in `seq` field
    MSG_NAK               = 41,
    MSG_ERR               = 42,
} ShareMsgType;

// MomentInfo wire layout for RESP_LIST_ITEM payload:
// - char[60]  filename (NUL-padded)
// - i32       filter_id
// - i32       frame_id
// - char[60]  music_name (NUL-padded; "" if none)
// = 132 bytes
#define SHARE_LIST_ITEM_SIZE 132

typedef struct {
    uint32_t magic;
    uint8_t  type;
    uint8_t  flags;
    uint32_t req_id;
    uint16_t seq;
    uint16_t total;
    uint16_t payload_len;
} ShareHeader;

// ---------------------------------------------------------------------------
// Pack header + optional payload into out_buf. Returns total bytes written
// or 0 on overflow. out_buf must be at least SHARE_HEADER_SIZE+payload_len.
// ---------------------------------------------------------------------------
size_t share_pack(uint8_t *out_buf, size_t cap,
                  ShareMsgType type, uint32_t req_id,
                  uint16_t seq, uint16_t total,
                  const void *payload, uint16_t payload_len);

// Parse a frame. Returns true on success and points payload_out into buf.
bool share_unpack(const uint8_t *buf, size_t len,
                  ShareHeader *out_header, const uint8_t **payload_out);

// One-shot send (constructs header, calls uds_send_frame).
int share_send(uint16_t dst_node, ShareMsgType type, uint32_t req_id,
               uint16_t seq, uint16_t total,
               const void *payload, uint16_t payload_len);

// ---------------------------------------------------------------------------
// Stop-and-wait blob sender.
// ---------------------------------------------------------------------------
typedef enum {
    SHARE_TX_IDLE,
    SHARE_TX_SEND_CHUNK,
    SHARE_TX_AWAIT_ACK,
    SHARE_TX_SEND_DONE,
    SHARE_TX_DONE,
    SHARE_TX_FAILED,
} ShareTxState;

typedef struct {
    ShareTxState state;
    uint16_t     dst_node;
    uint32_t     req_id;
    ShareMsgType chunk_type;
    ShareMsgType done_type;
    const uint8_t *data;
    size_t       data_len;
    uint16_t     cur_seq;
    uint16_t     total_seqs;
    int          retries_left;
    uint64_t     deadline_us;
} ShareSend;

void share_send_begin(ShareSend *s, uint16_t dst_node, uint32_t req_id,
                      ShareMsgType chunk_type, ShareMsgType done_type,
                      const void *data, size_t data_len);

// Drive one step of the state machine. Pulls/processes at most one ACK packet
// and sends at most one chunk. Call once per frame.
void share_send_tick(ShareSend *s);

// Hand-deliver a frame to a sender (e.g. an ACK pulled by another loop).
void share_send_feed(ShareSend *s, const ShareHeader *h);

// ---------------------------------------------------------------------------
// Stop-and-wait blob receiver.
// ---------------------------------------------------------------------------
typedef enum {
    SHARE_RX_IDLE,
    SHARE_RX_RECEIVING,
    SHARE_RX_DONE,
    SHARE_RX_FAILED,
} ShareRxState;

typedef struct {
    ShareRxState state;
    uint32_t     req_id;
    ShareMsgType chunk_type;
    ShareMsgType done_type;
    uint8_t     *buf;
    size_t       cap;
    size_t       bytes_recv;
    uint16_t     next_seq;
    uint16_t     total_seqs;
    uint16_t     src_node;        // resolved on first chunk
    uint64_t     deadline_us;
} ShareRecv;

void share_recv_begin(ShareRecv *r, uint32_t req_id,
                      ShareMsgType chunk_type, ShareMsgType done_type,
                      uint8_t *buf, size_t cap);

// Hand-deliver an already-parsed frame (typically pulled by the host/client
// state's main poll loop). The receiver responds with ACK on success.
void share_recv_feed(ShareRecv *r, const ShareHeader *h,
                     const uint8_t *payload, uint16_t src_node);

// Time-based check: marks RX_FAILED when no chunk arrived within timeout.
void share_recv_tick(ShareRecv *r);

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Time helper (microseconds since boot) for deadlines.
uint64_t share_now_us(void);

// Pack/unpack a list item (132 bytes) used in MSG_RESP_LIST_ITEM payloads.
void share_list_item_pack(uint8_t out[SHARE_LIST_ITEM_SIZE],
                          const char *filename, int filter_id, int frame_id,
                          const char *music_name);
void share_list_item_unpack(const uint8_t in[SHARE_LIST_ITEM_SIZE],
                            char *filename_out, int *filter_id_out,
                            int *frame_id_out, char *music_name_out);

#endif // SHARE_PROTOCOL_H
