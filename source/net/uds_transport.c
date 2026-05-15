#include "uds_transport.h"

#include <3ds.h>
#include <malloc.h>
#include <string.h>

// 0x3000 sharedmem buffer (must be 0x1000-aligned). Plan suggests >= 0x3000
// to leave headroom over UDS_DEFAULT_RECVBUFSIZE (0x2E30).
#define SHAREDMEM_SIZE 0x3000

// Scratch buffer for udsScanBeacons (host scratch + parsed network list).
// Large enough for the small number of peers we expect (UDS_MAX_PEERS).
#define SCAN_BUFFER_SIZE 0x4000

static bool          s_uds_inited = false;
static UdsRole       s_role       = UDS_ROLE_NONE;
static udsBindContext s_bind      = {0};
static bool          s_bind_open  = false;

// ---------------------------------------------------------------------------
int uds_init(void)
{
    if (s_uds_inited) return 0;

    Result rc = udsInit(SHAREDMEM_SIZE, NULL);
    if (R_FAILED(rc)) return -1;
    s_uds_inited = true;
    return 0;
}

void uds_shutdown(void)
{
    if (!s_uds_inited) return;

    if (s_role == UDS_ROLE_HOST) uds_host_stop();
    else if (s_role == UDS_ROLE_CLIENT) uds_client_disconnect();

    udsExit();
    s_uds_inited = false;
}

UdsRole uds_current_role(void) { return s_role; }

// ---------------------------------------------------------------------------
int uds_host_start(uint8_t max_clients)
{
    if (!s_uds_inited) return -1;
    if (s_role != UDS_ROLE_NONE) return -1;
    if (max_clients < 1) max_clients = 1;
    if (max_clients > UDS_MAXNODES - 1) max_clients = UDS_MAXNODES - 1;

    udsNetworkStruct network;
    udsGenerateDefaultNetworkStruct(&network, UDS_APP_WLANCOMMID, UDS_APP_ID8,
                                    (u8)(max_clients + 1)); // +1 for host

    Result rc = udsCreateNetwork(&network, NULL, 0, &s_bind,
                                 UDS_DATA_CHANNEL, UDS_DEFAULT_RECVBUFSIZE);
    if (R_FAILED(rc)) return -1;

    s_bind_open = true;
    s_role      = UDS_ROLE_HOST;
    return 0;
}

void uds_host_stop(void)
{
    if (s_role != UDS_ROLE_HOST) return;

    if (s_bind_open) {
        udsUnbind(&s_bind);
        s_bind_open = false;
    }
    udsDestroyNetwork();
    s_role = UDS_ROLE_NONE;
}

// ---------------------------------------------------------------------------
int uds_client_scan(UdsPeerInfo *out, int max)
{
    if (!s_uds_inited || !out || max <= 0) return -1;
    if (s_role != UDS_ROLE_NONE) return -1;

    void *scratch = memalign(0x1000, SCAN_BUFFER_SIZE);
    if (!scratch) return -1;

    udsNetworkScanInfo *networks = NULL;
    size_t total = 0;

    Result rc = udsScanBeacons(scratch, SCAN_BUFFER_SIZE,
                               &networks, &total,
                               UDS_APP_WLANCOMMID, UDS_APP_ID8,
                               NULL, false);
    if (R_FAILED(rc)) {
        free(scratch);
        return -1;
    }

    int written = 0;
    for (size_t i = 0; i < total && written < max; i++) {
        UdsPeerInfo *p = &out[written];
        memcpy(&p->network, &networks[i].network, sizeof(p->network));

        // Use host node username as a friendly label.
        p->username[0] = '\0';
        if (networks[i].network.total_nodes >= 1) {
            udsGetNodeInfoUsername(&networks[i].nodes[0], p->username);
        }
        written++;
    }

    free(networks); // allocated by libctru
    free(scratch);
    return written;
}

int uds_client_connect(const UdsPeerInfo *peer)
{
    if (!s_uds_inited || !peer) return -1;
    if (s_role != UDS_ROLE_NONE) return -1;

    Result rc = udsConnectNetwork(&peer->network, NULL, 0, &s_bind,
                                  UDS_BROADCAST_NETWORKNODEID,
                                  UDSCONTYPE_Client,
                                  UDS_DATA_CHANNEL,
                                  UDS_DEFAULT_RECVBUFSIZE);
    if (R_FAILED(rc)) return -1;

    s_bind_open = true;
    s_role      = UDS_ROLE_CLIENT;
    return 0;
}

void uds_client_disconnect(void)
{
    if (s_role != UDS_ROLE_CLIENT) return;

    if (s_bind_open) {
        udsUnbind(&s_bind);
        s_bind_open = false;
    }
    udsDisconnectNetwork();
    s_role = UDS_ROLE_NONE;
}

// ---------------------------------------------------------------------------
int uds_send_frame(uint16_t dst_node, const uint8_t *buf, size_t len)
{
    if (!s_uds_inited || s_role == UDS_ROLE_NONE) return -1;
    if (!buf || len == 0 || len > UDS_DATAFRAME_MAXSIZE) return -1;

    u8 flags = (dst_node == UDS_BROADCAST_NETWORKNODEID)
               ? (UDS_SENDFLAG_Default | UDS_SENDFLAG_Broadcast)
               :  UDS_SENDFLAG_Default;

    Result rc = udsSendTo(dst_node, UDS_DATA_CHANNEL, flags, buf, len);
    if (UDS_CHECK_SENDTO_FATALERROR(rc)) return -1;
    return 0;
}

int uds_recv_frame(uint8_t *buf, size_t cap, uint16_t *src_node, int wait_ms)
{
    if (!s_uds_inited || s_role == UDS_ROLE_NONE || !s_bind_open) return -1;
    if (!buf || cap == 0) return -1;

    // Coarse polling — udsWaitDataAvailable doesn't accept a timeout, so we
    // poll in small chunks. The numbers here are conservative; UDS callbacks
    // typically fire well below 50 ms in practice.
    const int poll_step_ms = 5;
    int waited = 0;
    while (true) {
        size_t actual = 0;
        u16 src = 0;
        Result rc = udsPullPacket(&s_bind, buf, cap, &actual, &src);
        if (R_FAILED(rc)) return -1;
        if (actual > 0) {
            if (src_node) *src_node = src;
            return (int)actual;
        }
        if (waited >= wait_ms) return 0;

        svcSleepThread((s64)poll_step_ms * 1000000LL);
        waited += poll_step_ms;
    }
}

int uds_node_count(void)
{
    if (!s_uds_inited || s_role == UDS_ROLE_NONE) return 0;

    udsConnectionStatus status = {0};
    if (R_FAILED(udsGetConnectionStatus(&status))) return 0;
    return (int)status.total_nodes;
}
