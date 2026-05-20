#ifndef LWIPOPTS_H
#define LWIPOPTS_H

/* ---- OS mode ---------------------------------------------------------- */
#define NO_SYS                      0   /* use FreeRTOS */
#define SYS_LIGHTWEIGHT_PROT        1   /* use sys_arch_protect/unprotect */

/* ---- Errno ------------------------------------------------------------ */
#define LWIP_PROVIDE_ERRNO          1   /* provide errno defines */

/* ---- API -------------------------------------------------------------- */
#define LWIP_NETCONN                1   /* netconn API (used by ntp_client) */
#define LWIP_SOCKET                 0   /* no BSD sockets (saves ~8KB) */
#define LWIP_NETIF_API              1

/* ---- Protocols -------------------------------------------------------- */
#define LWIP_TCP                    0   /* NTP only needs UDP */
#define LWIP_UDP                    1
#define LWIP_DHCP                   1
#define LWIP_AUTOIP                 0
#define LWIP_DNS                    1   /* dns_init() called by lwip_init() unconditionally */
#define LWIP_ICMP                   1
#define LWIP_ARP                    1
#define LWIP_IGMP                   0
#define LWIP_RAW                    0

/* ---- Memory ----------------------------------------------------------- */
#define MEM_LIBC_MALLOC             0
#define MEM_ALIGNMENT               4
#define MEM_SIZE                    (16 * 1024)

/* ---- Memory pools ----------------------------------------------------- */
#define PBUF_POOL_SIZE              8
#define PBUF_POOL_BUFSIZE           512
#define MEMP_NUM_PBUF               16
#define MEMP_NUM_UDP_PCB            4
#define MEMP_NUM_TCP_PCB            0
#define MEMP_NUM_TCP_PCB_LISTEN     0
#define MEMP_NUM_SYS_TIMEOUT        10
#define MEMP_NUM_NETBUF             8
#define MEMP_NUM_NETCONN            4
#define MEMP_NUM_TCPIP_MSG_API      8
#define MEMP_NUM_TCPIP_MSG_INPKT    8
#define MEMP_NUM_ARP_QUEUE          5

/* ---- IP --------------------------------------------------------------- */
#define IP_FORWARD                  0
#define IP_REASSEMBLY               0
#define IP_FRAG                     0

/* ---- DHCP ------------------------------------------------------------- */
#define DHCP_DOES_ARP_CHECK         0

/* ---- DNS -------------------------------------------------------------- */
#define DNS_MAX_NAME_LENGTH         64
#define DNS_TABLE_SIZE              4

/* ---- Threading -------------------------------------------------------- */
#define TCPIP_THREAD_NAME           "tcp/ip"
#define TCPIP_THREAD_STACKSIZE      1024    /* words (arm: 4 bytes each) */
#define TCPIP_THREAD_PRIO           5
#define TCPIP_MBOX_SIZE             16

#define DEFAULT_UDP_RECVMBOX_SIZE   8
#define DEFAULT_ACCEPTMBOX_SIZE     4
#define DEFAULT_THREAD_PRIO         3
#define DEFAULT_THREAD_STACKSIZE    512

/* ---- Checksums -------------------------------------------------------- */
#define CHECKSUM_GEN_IP             1
#define CHECKSUM_GEN_UDP            1
#define CHECKSUM_CHECK_IP           1
#define CHECKSUM_CHECK_UDP          1

/* ---- Netif callbacks -------------------------------------------------- */
#define LWIP_NETIF_STATUS_CALLBACK  1
#define LWIP_NETIF_LINK_CALLBACK    1

/* ---- Mutex compat ----------------------------------------------------- */
#define LWIP_COMPAT_MUTEX           0
#define LWIP_COMPAT_MUTEX_ALLOWED   0

/* ---- Socket options (needed for netconn recv timeout) ---------------- */
#define LWIP_SO_RCVTIMEO            1   /* enables netconn_set_recvtimeout */
#define LWIP_SO_SNDTIMEO            0

/* ---- Random number for DNS (port allocation) -------------------------- */
/* Simple LCG seeded from a counter in sys_arch.c                         */
/* u32_t not yet defined when lwipopts.h is parsed; use unsigned int */
unsigned int lwip_rand_func(void);
#define LWIP_RAND() ((unsigned int)lwip_rand_func())

/* ---- Stats / debug ---------------------------------------------------- */
#define LWIP_STATS                  0
#define LWIP_STATS_DISPLAY          0
#define LWIP_DEBUG                  0

#endif /* LWIPOPTS_H */
