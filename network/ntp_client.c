/* ntp_client.c
 * Sends a UDP request to an NTP server and extracts the current time.
 * Uses lwIP netconn API. Called from ntp_task on every keypress.
 *
 * NTP packet is 48 bytes; transmit timestamp is at bytes 40-43.
 * NTP epoch starts 1 Jan 1900; subtract 70 years to get UNIX time.
 * IST = UTC + 5:30 = +19800 s
 */

#include "lwip/api.h"
#include "lwip/ip_addr.h"
#include "lwip/netbuf.h"
#include "FreeRTOS.h"
#include "task.h"
#include "../include/time_msg.h"
#include <string.h>
#include <stdio.h>

/* Servers are tried in order.
 * The assignment recommends *.in.pool.ntp.org and time.nplindia.org, so
 * those are listed first.  Cloudflare/Google/Apple are kept as high-capacity
 * fallbacks in case pool servers are unreachable or rate-limited (the pool
 * throttles ~once/minute per source IP, and QEMU SLIRP shares the host IP).
 */
static const char *ntp_servers[] = {
    "0.in.pool.ntp.org",     /* assignment-recommended (pool)     */
    "1.in.pool.ntp.org",
    "2.in.pool.ntp.org",
    "time.nplindia.org",     /* assignment-recommended (NPL)      */
    "time.cloudflare.com",   /* fallback – no per-IP rate limit   */
    "time.google.com"        /* fallback – no per-IP rate limit   */
};
#define NTP_SERVER_COUNT (sizeof(ntp_servers) / sizeof(ntp_servers[0]))

#define NTP_PORT            123
#define NTP_PACKET_SIZE     48

/* 70 years of seconds between NTP epoch (1900) and UNIX epoch (1970) */
#define NTP_TO_UNIX_OFFSET  2208988800UL

/* IST = UTC + 5:30 */
#define IST_OFFSET_SEC      19800UL

#define NTP_RECV_TIMEOUT_MS 5000

/* Minimum gap between successive NTP calls.
 * Under QEMU SLIRP, all UDP traffic leaves the host with the host's IP.
 * High-capacity servers (Cloudflare/Google) tolerate rapid queries, but
 * pool.ntp.org enforces a ~60 s per-IP rate limit.  15 s is a reasonable
 * interactive floor: fast enough to feel responsive, safe for all servers. */
#define NTP_MIN_INTERVAL_MS 15000

/* QEMU SLIRP ties a UDP socket to (src_ip, src_port, dst_ip, dst_port).
 * After the first exchange SLIRP's host socket expires/closes.  If we let
 * lwIP reuse the same ephemeral port on the next call, SLIRP sees it as the
 * same stale flow and silently drops the outgoing packet.
 * Fix: bind each new conn to a distinct local port so SLIRP always opens a
 * fresh host socket.  Ports 50000-59999 are well clear of system ranges. */
static uint16_t ntp_local_port = 50000;

static TickType_t last_ntp_tick = 0;

/*
 * Try one NTP server: resolve → bind local port → sendto → receive → parse.
 * Returns 1 and fills msg on success, 0 on any failure.
 *
 * IMPORTANT – why we use netconn_sendto() instead of netconn_connect() +
 * netconn_send():
 *
 * Cloudflare (time.cloudflare.com), Google (time.google.com) and Apple
 * (time.apple.com) are ANYCAST services.  The outgoing NTP packet is
 * forwarded by the network to whichever physical replica is nearest, but
 * the REPLY may come back from a *different* IP in the same anycast pool
 * (e.g. we send to 162.159.200.1 but receive from 162.159.200.123).
 *
 * When the lwIP UDP PCB is in the "connected" state, udp_input() compares
 * the incoming source IP against pcb->remote_ip and silently DROPS the
 * packet if they differ.  The netconn_recv() call therefore times out even
 * though the server did respond – this is the exact bug that caused every
 * second-and-later NTP request to fail.
 *
 * Fix: keep the PCB UNCONNECTED and use netconn_sendto() to specify the
 * destination per-send.  An unconnected PCB accepts any incoming UDP on
 * the bound local port regardless of source IP, so anycast replies are
 * delivered correctly.
 */
static int try_ntp_server(const char *hostname, time_msg *msg)
{
    ip_addr_t server;
    struct netconn *conn;
    struct netbuf  *send_buf;
    struct netbuf  *recv_buf;
    err_t err;

    printf("[NTP_CLIENT] Trying %s\r\n", hostname);

    err = netconn_gethostbyname(hostname, &server);
    if (err != ERR_OK) {
        printf("[NTP_CLIENT] DNS failed for %s: %d\r\n", hostname, err);
        return 0;
    }
    printf("[NTP_CLIENT] Resolved -> %s\r\n", ipaddr_ntoa(&server));

    /* fresh UNCONNECTED UDP conn every call */
    conn = netconn_new(NETCONN_UDP);
    if (conn == NULL) {
        printf("[NTP_CLIENT] netconn_new failed\r\n");
        return 0;
    }

    /* bind to an explicit local port so QEMU SLIRP treats this as a new
     * UDP flow and opens a fresh host socket */
    uint16_t lport = ntp_local_port++;
    if (ntp_local_port > 59999)
        ntp_local_port = 50000;

    err = netconn_bind(conn, IP_ADDR_ANY, lport);
    if (err != ERR_OK) {
        printf("[NTP_CLIENT] bind failed on port %u: %d\r\n", lport, err);
        netconn_delete(conn);
        return 0;
    }

    /* Build NTP client request packet (48 bytes).
     * Allocate via netbuf_alloc so the payload lives in the lwIP heap –
     * avoids any risk of a stack-vs-send-completion race. */
    send_buf = netbuf_new();
    if (send_buf == NULL) {
        netconn_delete(conn);
        return 0;
    }

    uint8_t *buf_data = (uint8_t *)netbuf_alloc(send_buf, NTP_PACKET_SIZE);
    if (buf_data == NULL) {
        netbuf_delete(send_buf);
        netconn_delete(conn);
        return 0;
    }
    memset(buf_data, 0, NTP_PACKET_SIZE);
    buf_data[0] = 0x23;   /* LI=0, VN=4, Mode=3 (client) */

    /* sendto: destination specified per-call; PCB stays unconnected */
    err = netconn_sendto(conn, send_buf, &server, NTP_PORT);
    netbuf_delete(send_buf);
    if (err != ERR_OK) {
        printf("[NTP_CLIENT] send failed: %d\r\n", err);
        netconn_delete(conn);
        return 0;
    }

    /* Receive: unconnected PCB accepts from any source IP on our bound port.
     * This is essential for anycast servers whose reply may arrive from a
     * different IP than the one we sent to. */
    netconn_set_recvtimeout(conn, NTP_RECV_TIMEOUT_MS);
    err = netconn_recv(conn, &recv_buf);
    if (err != ERR_OK) {
        printf("[NTP_CLIENT] recv timeout from %s\r\n", hostname);
        netconn_delete(conn);
        return 0;
    }

    void    *data_ptr = NULL;
    uint16_t data_len = 0;
    err = netbuf_data(recv_buf, &data_ptr, &data_len);
    if (err != ERR_OK || data_ptr == NULL) {
        netbuf_delete(recv_buf);
        netconn_delete(conn);
        return 0;
    }

    int success = 0;
    const uint8_t *data = (const uint8_t *)data_ptr;

    if (data_len >= NTP_PACKET_SIZE) {
        /* Sanity-check mode field: server response must have mode == 4 */
        uint8_t mode = data[0] & 0x07u;
        if (mode != 4u) {
            printf("[NTP_CLIENT] Unexpected NTP mode %u – ignoring\r\n", mode);
        } else {
            /* Transmit timestamp: big-endian 32-bit at bytes 40-43 */
            uint32_t secs = ((uint32_t)data[40] << 24)
                          | ((uint32_t)data[41] << 16)
                          | ((uint32_t)data[42] <<  8)
                          |  (uint32_t)data[43];

            if (secs < NTP_TO_UNIX_OFFSET) {
                printf("[NTP_CLIENT] Timestamp looks wrong: %lu\r\n",
                       (unsigned long)secs);
            } else {
                uint32_t unix_secs = secs - NTP_TO_UNIX_OFFSET;
                uint32_t ist_secs  = unix_secs + IST_OFFSET_SEC;

                msg->second = (int)(ist_secs % 60);
                ist_secs   /= 60;
                msg->minute = (int)(ist_secs % 60);
                ist_secs   /= 60;
                msg->hour   = (int)(ist_secs % 24);

                success = 1;
            }
        }
    } else {
        printf("[NTP_CLIENT] Short response: %u bytes\r\n", data_len);
    }

    netbuf_delete(recv_buf);
    netconn_delete(conn);
    return success;
}

int ntp_get_time(time_msg *msg)
{
    /* enforce minimum gap – pool servers rate-limit by source IP */
    TickType_t now       = xTaskGetTickCount();
    TickType_t elapsed   = now - last_ntp_tick;   /* in ticks */
    TickType_t min_ticks = pdMS_TO_TICKS(NTP_MIN_INTERVAL_MS);

    if (last_ntp_tick != 0 && elapsed < min_ticks) {
        TickType_t wait = min_ticks - elapsed;
        printf("[NTP_CLIENT] Cooling down for %lu ms\r\n",
               (unsigned long)(wait * portTICK_PERIOD_MS));
        vTaskDelay(wait);
    }
    last_ntp_tick = xTaskGetTickCount();

    /* try each server in turn; move on immediately after any timeout */
    for (size_t i = 0; i < NTP_SERVER_COUNT; ++i) {
        if (try_ntp_server(ntp_servers[i], msg))
            return 1;
        printf("[NTP_CLIENT] Trying next server...\r\n");
    }

    printf("[NTP_CLIENT] All servers failed\r\n");
    return 0;
}
