#ifndef UDS_TRANSPORT_H
#define UDS_TRANSPORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <3ds/types.h>
#include <3ds/services/uds.h>

// ---------------------------------------------------------------------------
// uds_transport
//
// Thin wrapper around libctru's UDS service that gives the rest of the app a
// connection-oriented "send a frame / receive a frame" API. One module-global
// host or client session at a time.
// ---------------------------------------------------------------------------

// Unique 32-bit wlancommID for "Music Photo Frame". UDS uses this to filter
// beacons so only other instances of this app discover each other.
#define UDS_APP_WLANCOMMID 0x4D465348u // 'MFSH'

// id8 / data_channel — arbitrary, must match between host and client.
#define UDS_APP_ID8         0x01
#define UDS_DATA_CHANNEL    0x01

// One-byte frame size budget for the application protocol on top of UDS.
// Must be <= UDS_DATAFRAME_MAXSIZE (0x5C6 = 1478).
#define UDS_TRANSPORT_MTU   1024

// Discovered peer (host advertising the app's wlancommID).
typedef struct {
    udsNetworkStruct network;     // pass to udsConnectNetwork
    char             username[16];
} UdsPeerInfo;

#define UDS_MAX_PEERS 4

typedef enum {
    UDS_ROLE_NONE = 0,
    UDS_ROLE_HOST,
    UDS_ROLE_CLIENT,
} UdsRole;

// Bring up libctru UDS service. Idempotent. Returns 0 on success.
int  uds_init(void);
void uds_shutdown(void);

UdsRole uds_current_role(void);

// Start hosting a new network. Allows up to `max_clients` clients (1..3).
// Returns 0 on success.
int  uds_host_start(uint8_t max_clients);

// Stop hosting (idempotent).
void uds_host_stop(void);

// Scan for peers advertising the app wlancommID. `out` must point to an array
// of at least `max` UdsPeerInfo elements. Returns the number of peers written
// (0 if none) or a negative value on transport error.
int  uds_client_scan(UdsPeerInfo *out, int max);

// Connect to the given peer (typically one returned by uds_client_scan).
// Returns 0 on success.
int  uds_client_connect(const UdsPeerInfo *peer);

// Disconnect (client) — idempotent.
void uds_client_disconnect(void);

// Send `len` bytes to a node on the network. Use UDS_BROADCAST_NETWORKNODEID
// for broadcast, UDS_HOST_NETWORKNODEID for the host. Returns 0 on success.
int  uds_send_frame(uint16_t dst_node, const uint8_t *buf, size_t len);

// Try to receive one frame. If wait_ms == 0 returns immediately when nothing
// is queued. Returns the number of bytes written into `buf`, 0 if no frame is
// available within the timeout, or negative on error.
int  uds_recv_frame(uint8_t *buf, size_t cap, uint16_t *src_node, int wait_ms);

// Number of currently connected nodes (incl. self). Returns 0 if not on a
// network.
int  uds_node_count(void);

#endif // UDS_TRANSPORT_H
