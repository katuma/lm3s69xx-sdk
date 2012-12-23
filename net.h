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

#define IP_STATIC       0
#define IP_DHCP         1
#define IP_AUTOIP       2

extern void net_init(const u8 *, ulong, ulong, ulong, int);
extern void net_change(ulong, ulong, ulong, int);

// TI stuff
extern void lwIPTimer(ulong);
extern void lwIPEthernetIntHandler(void);

