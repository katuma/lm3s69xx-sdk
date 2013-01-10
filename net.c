// TI SDK hw access
#include "glue.h"

#include "arch/hw_ints.h"
#include "arch/hw_ethernet.h"
#include "arch/hw_memmap.h"
#include "arch/hw_nvic.h"
#include "arch/hw_sysctl.h"
#include "arch/hw_types.h"
#include "drivers/debug.h"
#include "drivers/ethernet.h"
#include "drivers/gpio.h"
#include "drivers/sysctl.h"
#include "drivers/interrupt.h"
#include "drivers/rom.h"
#include "drivers/rom_map.h"

#include "lwip/opt.h"
#include "lwip/sys.h"

static struct netif net_if; // our single network interface
static int net_if_up = 0; // netif is up?
static uint g_ipmode = IP_STATIC; // method to obtain ip
static ulong g_ip, g_mask, g_defgw; // ip settings


// cable detect timer
static void mdi_timer()
{
    int i;
    static ulong mdix_last;
    // deal with cross-cable (MDI-X) detect
    // every 200ms check if there is no link (to prevent hammering the register every packet)
    if ((now - mdix_last) > 200 && ((EthernetPHYRead(ETH_BASE, PHY_MR1) & PHY_MR1_LINK) == 0)) {
        // ok, no link
        if((local_timer - mdix_last) >= 2000) { // and at least for 2 seconds?
             // hmm, switch the pairs
            HWREG(ETH_BASE + MAC_O_MDIX) ^= MAC_MDIX_EN;
            mdix_last = now;
        }
    } else {
        // wait for next probe
        mdix_last = now;
    }

}

////////////////// public /////////////
// (re)initialize the network with new settings
void net_change(ulong ip,
         unsigned long mask, unsigned long defgw,
         int ipmode)
{

    struct ip_addr ip_addr;
    struct ip_addr net_mask;
    struct ip_addr gw_addr;

    if (ipmode == IP_STATIC)
    {
        ip_addr.addr = htonl(g_ip);
        net_mask.addr = htonl(g_mask);
        gw_addr.addr = htonl(g_defgw);
    } else ip_addr.addr = net_mask.addr = gw_addr.addr = 0;

    switch (g_ipmode)
    {
        case IP_STATIC: // previous mode static
            netif_set_addr(&net_if, &ip_addr, &net_mask, &gw_addr);

            // transition to dhcp
            if (ipmode == IP_DHCP) {
                dhcp_start(&net_if);
            } else if (ipmode == IP_AUTOIP) {
                autoip_start(&net_if);
            }
            break;

        case IP_DHCP: // previous dhcp
            if (ipmode == IP_STATIC) {
                dhcp_stop(&net_if);
                netif_set_addr(&net_if, &ip_addr, &net_mask, &gw_addr);
            } else if (ipmode == IP_AUTOIP) {
                dhcp_stop(&net_if);
                netif_set_addr(&net_if, &ip_addr, &net_mask, &gw_addr);
                autoip_start(&net_if);
            }
            break;

        case IP_AUTOIP:
            if (ipmode == IP_STATIC) {
                autoip_stop(&net_if);
                netif_set_addr(&net_if, &ip_addr, &net_mask, &gw_addr);
            } else if (ipmode == IP_DHCP) {
                autoip_stop(&net_if);
                netif_set_addr(&net_if, &ip_addr, &net_mask, &gw_addr);
                dhcp_start(&net_if);
            }
            break;
    }
    netif_set_up(&net_if);

    // remember new settings
    g_ipmode = ipmode;
    g_ip = ip;
    g_mask = mask;
    g_defgw = defgw;
}

void net_init(const u8 *mac, ulong ip,
         ulong mask, ulong defgw,
         ulong ipmode)
{
    struct ip_addr dummy;
    dummy.addr = 0;

    // start ethernet
    SysCtlPeripheralEnable(SYSCTL_PERIPH_ETH);

    // set mac
    EthernetMACAddrSet(ETH_BASE, (u8*)mac);

    // fire up lwip
    lwip_init();

    // add the interface. no ips yet - net_change will do it.
    netif_add(&net_if, &dummy, &dummy, &dummy, NULL, stellarisif_init, ip_input);
    netif_set_default(&net_if);

    // this will config ips / start dhcp etc as needed
    return net_change(ip, mask, defgw, ipmode);
}


////////////////// TI interface (thus the hungarian notation) /////////////

// TI API - called from driver
void lwIPTimer(unsigned long ms)
{
    now += ms;

    // that's right, we can't do timer processing here, because it calls into
    // guts of lwip. ethernet interrupt might happen at the same time and trash stuff
    // without some sort of locking. oops.

    // = aka linux ksoftirq
    // since interrupts provide "thread safety", this ensures that the
    // function below is called within the same (interrupt) context at all times.
    HWREG(NVIC_SW_TRIG) |= INT_ETH - 16;
}

// TI API - interrupt handler
void lwIPEthernetIntHandler(void)
{
    unsigned long rc;

    // clear int
    rc = EthernetIntStatus(ETH_BASE, false);
    EthernetIntClear(ETH_BASE, rc);

    // interrupt claimed by eth if - run driver
    if (rc)
        ethernetif_interrupt(&net_if);

    // periodicity is ensured by soft irq trigger via lwIPTimer above
    mdi_timer();
    sys_check_timeouts();
}

//////////////////// LwIP interface //////////////////////////
// lwip needs to know current time
ulong sys_now(void)
{
    return now;
}

// CLI
sys_prot_t sys_arch_protect(void)
{
    return((sys_prot_t)MAP_IntMasterDisable());
}

// STI
void sys_arch_unprotect(sys_prot_t lev)
{
    if((lev&1)==0) MAP_IntMasterEnable();
}


