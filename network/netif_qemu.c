/* netif_qemu.c
 * Ethernet driver for the LAN9118 (SMSC/Microchip) NIC emulated by QEMU.
 *
 * CRITICAL: on QEMU mps2-an500 the LAN9118 is at 0xA0000000,
 *           NOT at 0x40200000.  Wrong address = immediate HardFault.
 *
 * Register layout from the SMSC LAN9118 datasheet and QEMU hw/net/lan9118.c.
 */

#include "lwip/netif.h"
#include "lwip/etharp.h"
#include "lwip/pbuf.h"
#include "netif/ethernet.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>
#include <stdint.h>
#include <stdio.h>

/* ---- LAN9118 base address on mps2-an500 ------------------------------ */
#define LAN9118_BASE        0xA0000000UL   /* IRQ 13 on mps2-an500 */

/* ---- Register offsets ------------------------------------------------ */
#define RX_DATA_FIFO        (LAN9118_BASE + 0x00)
#define TX_DATA_FIFO        (LAN9118_BASE + 0x20)
#define RX_STATUS_FIFO      (LAN9118_BASE + 0x40)
#define RX_STATUS_FIFO_PEEK (LAN9118_BASE + 0x44)
#define TX_STATUS_FIFO      (LAN9118_BASE + 0x48)
#define TX_STATUS_FIFO_PEEK (LAN9118_BASE + 0x4C)
#define ID_REV              (LAN9118_BASE + 0x50)
#define IRQ_CFG             (LAN9118_BASE + 0x54)
#define INT_STS             (LAN9118_BASE + 0x58)
#define INT_EN              (LAN9118_BASE + 0x5C)
#define BYTE_TEST           (LAN9118_BASE + 0x64)
#define FIFO_INT            (LAN9118_BASE + 0x68)
#define RX_CFG              (LAN9118_BASE + 0x6C)
#define TX_CFG              (LAN9118_BASE + 0x70)
#define HW_CFG              (LAN9118_BASE + 0x74)
#define RX_DP_CTRL          (LAN9118_BASE + 0x78)
#define RX_FIFO_INF         (LAN9118_BASE + 0x7C)
#define TX_FIFO_INF         (LAN9118_BASE + 0x80)
#define PMT_CTRL            (LAN9118_BASE + 0x84)
#define GPIO_CFG            (LAN9118_BASE + 0x88)
#define FREE_RUN            (LAN9118_BASE + 0x9C)
#define MAC_CSR_CMD         (LAN9118_BASE + 0xA4)
#define MAC_CSR_DATA        (LAN9118_BASE + 0xA8)

/* ---- MAC CSR register indices ---------------------------------------- */
#define MAC_CR              1
#define MAC_ADDRH           2
#define MAC_ADDRL           3

/* ---- Register bit fields --------------------------------------------- */
#define HW_CFG_SRST         (1u << 0)
#define HW_CFG_MBO          (1u << 20)
#define TX_CFG_ON           (1u << 1)
#define MAC_CSR_BUSY        (1u << 31)
#define MAC_CSR_READ        (1u << 30)
#define MAC_CR_RXEN         (1u << 2)
#define MAC_CR_TXEN         (1u << 3)
#define TX_CMD_A_FIRST_SEG  (1u << 13)
#define TX_CMD_A_LAST_SEG   (1u << 12)
#define RX_STS_PKT_LEN_MASK  0x3FFF0000u
#define RX_STS_PKT_LEN_SHIFT 16
#define RX_STS_ES           (1u << 15)

/*
 * TX command overhead: Command A + Command B = 2 × 4 bytes = 8 bytes.
 * (LAN9118 datasheet Table 5-2)
 */
#define TX_CMD_OVERHEAD_BYTES   8u

/*
 * MAC CSR busy-wait iteration limit.
 *
 * The MAC indirect access mechanism signals completion by clearing the BUSY
 * bit in MAC_CSR_CMD.  Each iteration reads a MMIO register; on QEMU this
 * completes instantly (within a handful of host CPU cycles), so 10 000
 * iterations is far more than sufficient and ensures we never hang even if
 * the emulated hardware is slow.
 *
 * On real silicon the LAN9118 datasheet (§5.4.3) guarantees the BUSY bit
 * clears within 35 MAC clock cycles (~3.5 µs at 10 MHz).  At a 25 MHz
 * Cortex-M7 that is < 100 CPU cycles, so 10 000 iterations is still safe.
 * If porting to a much faster host, increase this constant proportionally
 * or replace with a timer-based timeout.
 */
#define MAC_CSR_BUSY_WAIT_ITERS  10000

/*
 * Hardware soft-reset busy-wait limit (same rationale as above;
 * LAN9118 datasheet §4.3.2 states reset completes within 1 µs).
 */
#define HW_SRST_WAIT_ITERS       100000

#define REG32(addr)  ( *((volatile uint32_t *)(addr)) )

/* Locally administered MAC for the virtual NIC */
static uint8_t mac_addr[6] = { 0x52, 0x54, 0x00, 0x12, 0x34, 0x56 };

/* ---- MAC CSR indirect access ----------------------------------------- */

static void mac_csr_write(uint8_t reg, uint32_t val)
{
    int t;
    for (t = MAC_CSR_BUSY_WAIT_ITERS;
         (REG32(MAC_CSR_CMD) & MAC_CSR_BUSY) && t > 0; t--);

    REG32(MAC_CSR_DATA) = val;
    REG32(MAC_CSR_CMD)  = MAC_CSR_BUSY | (uint32_t)reg;

    for (t = MAC_CSR_BUSY_WAIT_ITERS;
         (REG32(MAC_CSR_CMD) & MAC_CSR_BUSY) && t > 0; t--);
}

static uint32_t mac_csr_read(uint8_t reg)
{
    int t;
    for (t = MAC_CSR_BUSY_WAIT_ITERS;
         (REG32(MAC_CSR_CMD) & MAC_CSR_BUSY) && t > 0; t--);

    REG32(MAC_CSR_CMD) = MAC_CSR_BUSY | MAC_CSR_READ | (uint32_t)reg;

    for (t = MAC_CSR_BUSY_WAIT_ITERS;
         (REG32(MAC_CSR_CMD) & MAC_CSR_BUSY) && t > 0; t--);

    return REG32(MAC_CSR_DATA);
}

/* ---- Hardware initialisation ----------------------------------------- */

static void lan9118_hw_init(void)
{
    uint32_t id = REG32(ID_REV);
    printf("[NIC] LAN9118 ID_REV = 0x%08lX\r\n", (unsigned long)id);

    /* Soft reset */
    REG32(HW_CFG) = HW_CFG_SRST;
    int t;
    for (t = HW_SRST_WAIT_ITERS; (REG32(HW_CFG) & HW_CFG_SRST) && t > 0; t--);
    if (t <= 0)
        printf("[NIC] WARNING: soft reset timed out\r\n");

    /* Must-be-one bit */
    REG32(HW_CFG) = HW_CFG_MBO;

    /* Program MAC address */
    uint32_t mac_lo = ((uint32_t)mac_addr[0])        |
                      ((uint32_t)mac_addr[1] <<  8)   |
                      ((uint32_t)mac_addr[2] << 16)   |
                      ((uint32_t)mac_addr[3] << 24);
    uint32_t mac_hi = ((uint32_t)mac_addr[4])        |
                      ((uint32_t)mac_addr[5] <<  8);

    mac_csr_write(MAC_ADDRL, mac_lo);
    mac_csr_write(MAC_ADDRH, mac_hi);

    /* Enable TX and RX in MAC control register */
    uint32_t cr = mac_csr_read(MAC_CR);
    cr |= MAC_CR_TXEN | MAC_CR_RXEN;
    mac_csr_write(MAC_CR, cr);

    /* Enable TX data path */
    REG32(TX_CFG) = TX_CFG_ON;

    /* Clear and disable all interrupts (we poll, no IRQ handler needed) */
    REG32(INT_STS) = 0xFFFFFFFFu;
    REG32(INT_EN)  = 0;

    printf("[NIC] LAN9118 ready, MAC %02X:%02X:%02X:%02X:%02X:%02X\r\n",
           mac_addr[0], mac_addr[1], mac_addr[2],
           mac_addr[3], mac_addr[4], mac_addr[5]);
}

/* ---- TX --------------------------------------------------------------- */

static err_t low_level_output(struct netif *netif, struct pbuf *p)
{
    (void)netif;
    uint16_t total = p->tot_len;

    /* Check TX FIFO has enough space for the frame plus the two command words.
     * TX_FIFO_INF bits [15:0] = free space in bytes. */
    uint32_t inf        = REG32(TX_FIFO_INF);
    uint32_t free_bytes = inf & 0xFFFFu;
    if (free_bytes < (uint32_t)(total + TX_CMD_OVERHEAD_BYTES)) {
        printf("[NIC] TX FIFO full (%lu free, need %u)\r\n",
               (unsigned long)free_bytes,
               (unsigned int)(total + TX_CMD_OVERHEAD_BYTES));
        return ERR_MEM;
    }

    /* TX Command A: first segment | last segment | data size */
    REG32(TX_DATA_FIFO) = TX_CMD_A_FIRST_SEG | TX_CMD_A_LAST_SEG | total;
    /* TX Command B: packet tag=0 | packet length */
    REG32(TX_DATA_FIFO) = (uint32_t)total;

    /* Write pbuf chain 4 bytes at a time, handling unaligned tail */
    for (struct pbuf *q = p; q != NULL; q = q->next) {
        const uint8_t *ptr = (const uint8_t *)q->payload;
        uint16_t len = q->len;
        uint16_t i;
        for (i = 0; i + 3 < len; i += 4) {
            uint32_t w = (uint32_t)ptr[i]
                       | ((uint32_t)ptr[i+1] << 8)
                       | ((uint32_t)ptr[i+2] << 16)
                       | ((uint32_t)ptr[i+3] << 24);
            REG32(TX_DATA_FIFO) = w;
        }
        /* Write any remaining 1-3 bytes in a padded final word */
        if (i < len) {
            uint32_t w = 0;
            for (uint16_t j = 0; i + j < len; j++)
                w |= (uint32_t)ptr[i + j] << (j * 8);
            REG32(TX_DATA_FIFO) = w;
        }
    }
    return ERR_OK;
}

/* ---- RX --------------------------------------------------------------- */

static struct pbuf *low_level_input(struct netif *netif)
{
    (void)netif;

    /* Check RX status FIFO level (bits [23:16] = number of status entries) */
    uint32_t inf = REG32(RX_FIFO_INF);
    if (((inf >> 16) & 0xFFu) == 0)
        return NULL;   /* no packet waiting */

    uint32_t rx_status = REG32(RX_STATUS_FIFO);

    if (rx_status & RX_STS_ES) {
        /* Error in frame – flush the RX data FIFO for this packet */
        REG32(RX_DP_CTRL) = 0x80000000u;
        int t;
        for (t = 10000; (REG32(RX_DP_CTRL) & 0x80000000u) && t > 0; t--);
        return NULL;
    }

    /* RX Status bits [29:16] = packet length including 4-byte FCS/CRC */
    uint16_t pkt_len = (uint16_t)((rx_status & RX_STS_PKT_LEN_MASK)
                                  >> RX_STS_PKT_LEN_SHIFT);

    if (pkt_len < 14 || pkt_len > 1522) {
        /* Invalid length – flush */
        REG32(RX_DP_CTRL) = 0x80000000u;
        int t;
        for (t = 10000; (REG32(RX_DP_CTRL) & 0x80000000u) && t > 0; t--);
        return NULL;
    }

    /*
     * CRC/FCS stripping – DOCUMENTED.
     *
     * QEMU's LAN9118 model (hw/net/lan9118.c) reports pkt_len INCLUDING the
     * 4-byte Ethernet FCS in the RX status word, matching real LAN9118
     * hardware behaviour (RX Status Frame Length field, datasheet §5.3.4).
     *
     * We strip the 4 FCS bytes here so lwIP only sees the Ethernet payload
     * (header + data, no CRC).  lwIP does not re-validate the CRC; it relies
     * on the NIC having already checked it.
     *
     * To verify this assumption when porting to a different QEMU version:
     *   1. Enable LWIP_DEBUG and watch for "Illegal packet len" warnings from
     *      etharp.c – those indicate the CRC was NOT included and we over-stripped.
     *   2. Alternatively, log pkt_len before and after stripping for a known
     *      frame size (e.g., a 64-byte minimum frame: pkt_len should be 64
     *      before stripping, giving data_len = 60).
     */
    uint16_t data_len = pkt_len - 4u;  /* strip 4-byte Ethernet FCS */

    struct pbuf *p = pbuf_alloc(PBUF_RAW, data_len, PBUF_POOL);
    if (p == NULL) {
        /* Pool exhausted – flush and drop */
        REG32(RX_DP_CTRL) = 0x80000000u;
        int t;
        for (t = 10000; (REG32(RX_DP_CTRL) & 0x80000000u) && t > 0; t--);
        return NULL;
    }

    /* Read pkt_len bytes from FIFO (rounded up to 32-bit words).
     * We read all words including the 4 FCS bytes, but only copy data_len
     * bytes into the pbuf so the FCS is naturally discarded. */
    uint32_t words    = (pkt_len + 3u) / 4u;
    uint32_t word_idx = 0;
    uint32_t cur_word = 0;
    uint8_t  byte_pos = 4u;   /* force first FIFO read on first byte */

    for (struct pbuf *q = p; q != NULL; q = q->next) {
        uint8_t *ptr = (uint8_t *)q->payload;
        for (uint16_t i = 0; i < q->len; i++) {
            if (byte_pos >= 4u) {
                if (word_idx < words) {
                    cur_word = REG32(RX_DATA_FIFO);
                    word_idx++;
                }
                byte_pos = 0;
            }
            ptr[i] = (uint8_t)(cur_word >> (byte_pos * 8u));
            byte_pos++;
        }
    }
    /* Drain any remaining FIFO words (padding + FCS words not copied) */
    while (word_idx < words) {
        (void)REG32(RX_DATA_FIFO);
        word_idx++;
    }

    return p;
}

/* ---- RX polling task ------------------------------------------------- */
/*
 * Polling strategy:
 *   - When a packet is available, deliver it immediately and check again
 *     right away (no sleep).  This gives zero additional latency during
 *     bursts (e.g., DHCP exchange, ARP + NTP response).
 *   - When the FIFO is empty, yield the CPU for one FreeRTOS tick (1 ms
 *     at 1 kHz tick rate) before re-checking.  This prevents the task from
 *     monopolising the CPU when there is no incoming traffic, while keeping
 *     idle overhead to just 1 ms per wake-up instead of burning a full
 *     scheduling quantum in a hot busy-loop.
 *
 * This replaces the previous fixed 10 ms delay which added up to 10 ms of
 * unnecessary latency for every incoming packet (e.g. the NTP response).
 */
static void netif_rx_task(void *arg)
{
    struct netif *netif = (struct netif *)arg;
    while (1) {
        struct pbuf *p = low_level_input(netif);
        if (p != NULL) {
            /* Packet received – hand to lwIP and immediately check for more */
            if (netif->input(p, netif) != ERR_OK)
                pbuf_free(p);
            /* No delay: loop back immediately to drain any further queued
             * packets.  taskYIELD() would also work here but is unnecessary
             * because the next FIFO read will happen within a few cycles. */
        } else {
            /* FIFO empty – yield for 1 tick so other tasks can run */
            vTaskDelay(1);
        }
    }
}

/* ---- netif init callback (called by netif_add) ----------------------- */

err_t qemu_netif_init(struct netif *netif)
{
    netif->hwaddr_len = 6;
    memcpy(netif->hwaddr, mac_addr, 6);
    netif->mtu        = 1500;
    netif->name[0]    = 'e';
    netif->name[1]    = '0';
    netif->flags      = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP
                      | NETIF_FLAG_LINK_UP;
    netif->output     = etharp_output;
    netif->linkoutput = low_level_output;

    lan9118_hw_init();

    /* RX polling task: priority 4 so packets are handled promptly */
    xTaskCreate(netif_rx_task, "NICRX", 512, (void *)netif, 4, NULL);

    return ERR_OK;
}
