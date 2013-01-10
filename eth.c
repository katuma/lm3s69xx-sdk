/*
 * ethernet interface, fitted onto the lwIP skeleton.
 * BSD licensed
 */

/*
 * Copyright (c) 2001-2004 Swedish Institute of Computer Science.
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
 * This file is part of the lwIP TCP/IP stack.
 * 
 * Author: Adam Dunkels <adam@sics.se>
 *
 */

#define TXQ 16

#include "lwip/opt.h"

#include "lwip/def.h"
#include "lwip/mem.h"
#include "lwip/pbuf.h"
#include "lwip/stats.h"
#include "lwip/snmp.h"
#include "lwip/ethip6.h"
#include "lwip/sys.h"

#include "netif/etharp.h"
#include "netif/ppp_oe.h"

#include "inc/hw_ethernet.h"
#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "driverlib/ethernet.h"
#include "driverlib/interrupt.h"
#include "driverlib/sysctl.h"


#include "eth.h"

/* Define those to better describe your network interface. */
#define IFNAME0 'e'
#define IFNAME1 'n'

struct txdesc {
    struct eth_addr *ethaddr;
    struct pbuf *ring[TXQ];
    u32_t p; // position
    u32_t n; // total
};

// main packet transmit function
// we'll try to copy words from pbufs as fast as possible,
// resort to byte-shuffling only if necessary
static void pkt_xmit(struct pbuf *p)
{
    // first u16_t is packet length
    *((u16_t*)(p->payload)) = p->tot_len - 16;

    for (struct pbuf *q = p; q; q = q->next) {
        u32_t n = q->len / 4;
        u32_t rem = q->len % 4;
        for (u32_t i = 0; i < n; i++)
            HWREG(ETH_BASE + MAC_O_DATA) = ((u32_t*)q->payload)[i];
        // uh-oh, slow path
        if (rem) {
            u32_t ptr = n * 4; // q->len - rem?
            u32_t acc = 0;
            u32_t i = 0;
            u8_t *ap = (u8_t*)&acc;
            do {
                for (n = q->len; ptr < n; ptr++) {
                    ap[i++] = ((u8_t*)q->payload)[ptr];
                    if (i == 4) {
                        HWREG(ETH_BASE + MAC_O_DATA) = acc;
                        acc = i =0;
                    }
                }
                // ended up word-aligned again, retry
                if (!i) break;

                // continue byte-shuffling
                ptr = 0;
            } while (q = q->next);
            // at this point we'll continue word aligned

            if (i) {
                LWIP_ASSERT("q != NULL and i != 0", (q != NULL));
                HWREG(ETH_BASE + MAC_O_DATA) = acc;
            }
            // end of chain
            if (!q) break;
        }
    }
    LINK_STATS_INC(link.xmit);
    pbuf_free(p);
}

// should be interrupt protected
static void txq_flush(struct netif *netif)
{
    struct txdesc *txq = netif->state;
    while ((txq->p < txq->n) && ((HWREG(ETH_BASE + MAC_O_TR) & MAC_TR_NEWTX) == 0))
        pkt_xmit(txq->ring[txq->p++ % TXQ]);
}

// interrupt-protects itself
static err_t ethernetif_output(struct netif *netif, struct pbuf *p)
{
    struct txdesc *txq = netif->state;
    SYS_ARCH_DECL_PROTECT(lev);
    SYS_ARCH_PROTECT(lev);
    u32_t qlen = txq->n - txq->p;
    if ((qlen == 0) && ((HWREG(ETH_BASE + MAC_O_TR) & MAC_TR_NEWTX) == 0)) {
        pkt_xmit(p);
    } else {
        // queue overflow
        if (qlen >= TXQ) {
            pbuf_free(p);
            SYS_ARCH_UNPROTECT(lev);
            return ERR_MEM;
        }
        // enqueue
        txq->ring[txq->n++ % TXQ] = p;
    }
    SYS_ARCH_UNPROTECT(lev);
    return ERR_OK;
}

// receive packet (from interrupt ctx)
static struct pbuf *pkt_recv(struct netif *netif)
{
    u32_t len = (HWREG(ETH_BASE + MAC_O_DATA)) & 0xffff;
    struct pbuf *p = pbuf_alloc(PBUF_RAW, len, PBUF_POOL);

    // no buffer to drain into
    if (!p) {
        for (u32_t i = 4; i < len; i += 4)
            HWREG(ETH_BASE + MAC_O_DATA);
        LINK_STATS_INC(link.memerr);
        LINK_STATS_INC(link.drop);
        return NULL;
    }

    *(u32_t *) p->payload = len;

    // fetch the packet (pbufs can be fragmented, so it is odd like this)
    u32_t first = 1;
    for (struct pbuf *q = p; q; q = q->next) {
        u32_t n = q->len / 4;
        for (u32_t i = first; i < n; i++)
            ((u32_t *)q->payload)[i] = HWREG(ETH_BASE + MAC_O_DATA);
        first = 0;
    }
    LINK_STATS_INC(link.recv);

}

// interrupt handler (must be setup and called externally)
void ethernetif_interrupt(struct netif *netif)
{
    // packet waiting in rx fifo
    while ((HWREG(ETH_BASE + MAC_O_NP) & MAC_NP_NPR_M)) {
        struct pbuf *p = pkt_recv(netif);
        // not enough pools for packet
        if (!p) continue;
        if (netif->input(p, netif) != ERR_OK) {
            LWIP_DEBUGF(NETIF_DEBUG, ("ethernetif_interrupt: input error\n"));
            pbuf_free(p);
            LINK_STATS_INC(link.memerr);
            LINK_STATS_INC(link.drop);
        }
        txq_flush(netif); // XXX should protect against other interrupts?
    }
}

// alloc ring, init hardware
err_t ethernetif_init(struct netif *netif)
{
    LWIP_ASSERT("netif != NULL", (netif != NULL));
    struct txdesc *txq;
    txq = mem_malloc(sizeof(*txq));
    if (!txq) {
        LWIP_DEBUGF(NETIF_DEBUG, ("ethernetif_init: out of memory\n"));
        return ERR_MEM;
    }

#if LWIP_NETIF_HOSTNAME
    /* Initialize interface hostname */
    netif->hostname = "lwip";
#endif /* LWIP_NETIF_HOSTNAME */
    
    // tx ring state
    txq->n = txq->p = 0;
    netif->state = txq;

    /*
     * Initialize the snmp variables and counters inside the struct netif.
     * The last argument should be replaced with your link speed, in units
     * of bits per second.
     */
    NETIF_INIT_SNMP(netif, snmp_ifType_ethernet_csmacd, 1000000);

    netif->name[0] = IFNAME0;
    netif->name[1] = IFNAME1;
    /* We directly use etharp_output() here to save a function call.
     * You can instead declare your own function an call etharp_output()
     * from it if you have to do some checks before sending (e.g. if link
     * is available...) */
    netif->output = etharp_output;
#if LWIP_IPV6
    netif->output_ip6 = ethip6_output;
#endif /* LWIP_IPV6 */
    netif->linkoutput = ethernetif_output;
 
    // hardware init 
    netif->hwaddr_len = ETHARP_HWADDR_LEN;
    EthernetMACAddrGet(ETH_BASE, &(netif->hwaddr[0]));
    netif->mtu = 1500;
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP;
    EthernetIntDisable(ETH_BASE, (ETH_INT_PHY | ETH_INT_MDIO | ETH_INT_RXER | ETH_INT_RXOF | ETH_INT_TX | ETH_INT_TXER | ETH_INT_RX));
    u32_t n = EthernetIntStatus(ETH_BASE, false);
    EthernetIntClear(ETH_BASE, n);
    EthernetInitExpClk(ETH_BASE, SysCtlClockGet());
    EthernetConfigSet(ETH_BASE, (ETH_CFG_TX_DPLXEN |ETH_CFG_TX_CRCEN | ETH_CFG_TX_PADEN | ETH_CFG_RX_AMULEN));
    EthernetEnable(ETH_BASE);
    IntEnable(INT_ETH);
    EthernetIntEnable(ETH_BASE, ETH_INT_RX | ETH_INT_TX);

    return ERR_OK;
}


