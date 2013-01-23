/*
 * Copyright (c) 2012-2013 Karel Tuma <karel.tuma@omsquare.com>
 * All rights reserved. 
 * 
 * Redistribution and use in source and binary forms, with or without modification, 
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED 
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT 
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT 
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING 
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY 
 * OF SUCH DAMAGE.
 * 
 */


//////////////////////////////////////////////////////////////////////////
// mid-layer networking code - dhcp, mdi-x, timers etc
//////////////////////////////////////////////////////////////////////////


#include "lwip/opt.h"

#include "inc/hw_ints.h"
#include "inc/hw_ethernet.h"
#include "inc/hw_memmap.h"
#include "inc/hw_nvic.h"
#include "inc/hw_sysctl.h"
#include "inc/hw_types.h"
#include "inc/hw_flash.h"

#include "driverlib/debug.h"
#include "driverlib/ethernet.h"
#include "driverlib/gpio.h"
#include "driverlib/sysctl.h"
#include "driverlib/interrupt.h"
#include "driverlib/rom.h"
#include "driverlib/rom_map.h"

#include "lwip/sys.h"


#include "lwip/dhcp.h"
#include "lwip/autoip.h"
#include "lwip/init.h"
#include "lwip/timers.h"

#include "net.h"
#include "eth.h"

static struct netif net_if; // our single network interface
static int net_if_up = 0; // netif is up?
static unsigned int g_ipmode = IP_STATIC; // method to obtain ip
static u32_t g_ip, g_mask, g_defgw; // ip settings

extern u32_t now;


// cable detect timer
static void mdi_timer()
{
    int i;
    static u32_t mdix_last;
    // deal with cross-cable (MDI-X) detect
    // every 200ms check if there is no link (to prevent hammering the register every packet)
    if ((now - mdix_last) > 200 && ((EthernetPHYRead(ETH_BASE, PHY_MR1) & PHY_MR1_LINK) == 0)) {
        // ok, no link
        if((now - mdix_last) >= 2000) { // and at least for 2 seconds?
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
void net_change(u32_t ip,
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

void net_init(const u8_t *mac, u32_t ip,
         u32_t mask, u32_t defgw,
         int ipmode)
{
    struct ip_addr dummy;
    dummy.addr = 0;

    // start ethernet
    SysCtlPeripheralEnable(SYSCTL_PERIPH_ETH);

    // no mac specified - use nvram
    if (!mac) {
        // read mac addr from flash
        u32_t mac1 = HWREG(FLASH_USERREG0);
        u32_t mac2 = HWREG(FLASH_USERREG1);
        static u8_t macp[6];
        mac = macp;

        // convert mac from weird nvram format
        for (int i = 0; i < 3; i++) {
            macp[i] = (mac1 >> (i*8)) & 0xff;
            macp[i+3] = (mac2 >> (i*8)) & 0xff;
        }
    }
#if DEBUG
    printf("mac:");
	for (int i = 0; i < 6; i++)
		printf("%02hhx:", mac[i]);
    printf("\n");
#endif

    LWIP_DEBUGF(NETIF_DEBUG, ("set mac\n"));

    // set mac
    EthernetMACAddrSet(ETH_BASE, (u8_t*)mac);

    // fire up lwip
    lwip_init();

    // add the interface. no ips yet - net_change will do it.
    netif_add(&net_if, &dummy, &dummy, &dummy, NULL, ethernetif_init, ip_input);
    netif_set_default(&net_if);

    // this will config ips / start dhcp etc as needed
    return net_change(ip, mask, defgw, ipmode);
}


void net_timer(u32_t ms)
{
    now += ms;
    IntPendSet(INT_ETH);
}

void net_inthandler(void)
{
    unsigned long rc;

    SYS_ARCH_DECL_PROTECT(lev);
    SYS_ARCH_PROTECT(lev);

    // clear int
    rc = EthernetIntStatus(ETH_BASE, false);
    EthernetIntClear(ETH_BASE, rc);

    // interrupt claimed by eth if - run driver
    if (rc)
        ethernetif_interrupt(&net_if);

    // periodicity is ensured by soft irq trigger via lwIPTimer above
    //mdi_timer();
    sys_check_timeouts();
    SYS_ARCH_UNPROTECT(lev);
}


