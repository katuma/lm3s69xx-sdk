#include "lwip/opt.h"
#undef LWIP_DHCP_AUTOIP_COOP
#define LWIP_DHCP_AUTOIP_COOP   ((LWIP_DHCP) && (LWIP_AUTOIP))

#include "lwip/api.h"
#include "lwip/netifapi.h"
#include "lwip/tcp.h"
#include "lwip/udp.h"
#include "lwip/tcpip.h"
#include "lwip/sockets.h"
#include "lwip/mem.h"
#include "lwip/stats.h"

#include "driverlib/ethernet.h"

#define IP_STATIC       0
#define IP_DHCP         1
#define IP_AUTOIP       2

extern void net_init(const u8_t *, u32_t, u32_t, u32_t, int);
extern void net_change(u32_t, u32_t, u32_t, int);

// TI stuff
extern void lwIPTimer(u32_t);
extern void lwIPEthernetIntHandler(void);

