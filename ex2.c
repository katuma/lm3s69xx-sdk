// tiny tcp server via lwip raw api

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include <string.h>

#include "net.h"
#include "lwip/tcp.h"
enum {
    ACCEPTED,
    RECEIVED,
    TERMINATED,
};

struct client {
    u8_t st;
    struct tcp_pcb *pcb;
    struct pbuf *p;
};

// kill all data associated with client
err_t client_kill(struct client *c)
{
    struct tcp_pcb *pcb = c->pcb;
    tcp_close(pcb);
    mem_free(c);
    return ERR_OK;
}

// flush queues
err_t flushq(struct client *c) {
    struct tcp_pcb *pcb = c->pcb;
    while (c->p != NULL && c->p->len < tcp_sndbuf(pcb)) {
        if (tcp_write(pcb, c->p->payload, c->p->len, 1) != ERR_OK)
            break;   // defer to poll
        u32_t oldlen = c->p->len;
        struct pbuf *next = c->p->next;
        pbuf_free(c->p);
        if ((c->p = next))
            pbuf_ref(c->p);
        tcp_recved(pcb, oldlen);
    }
    if (c->st == TERMINATED)
        client_kill(c);
    return ERR_OK;
}


// sent data notifier
err_t client_sent(void *arg, struct tcp_pcb *pcb, u16_t len)
{
    return flushq(arg);
}


// on data received
err_t client_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err)
{
    struct client *c = arg;
    if (err!=ERR_OK) {
        pbuf_free(p);
        return client_kill(c);
    }
    // no buffer
    if (!p) {
        c->st = TERMINATED;
        // and no queued data -> close
        if (!c->p)
            return client_kill(c);
        return flushq(c);
    }
    switch (c->st) {
        case ACCEPTED: {
            c->st++;
        }
        case RECEIVED: {
            // this is a hack - we assume each line fits into a single packet
            // this could be done properly using pbuf header magic and keeping line state
            u8_t *dst = p->payload;
            u8_t *res = memchr(dst, '\n', p->len);

            // invalid data or exit requested
            if (!res || !memcmp(dst,"exit",4)) {
                pbuf_free(p);
                return client_kill(c);
            }

            // reverse the line
            if (res[-1] == '\r') res--;
            u32_t len = res-dst;
            for (int i = 0; i < (len)/2;i++) {
                u8_t t = dst[i];
                dst[i] = dst[len-i-1];
                dst[len-i-1] = t;
            }
            dst[len] = '\n';
            pbuf_realloc(p, len+1);


            // and enqueue it
            if (c->p) {
                pbuf_chain(c->p, p);
                return ERR_OK;
            }
            c->p = p;
            return flushq(c);
        }
        // unknown state
        default: {
            tcp_recved(pcb, p->tot_len);
            c->p = NULL; // XXX leaky?
            pbuf_free(p);
        }
    }
    return ERR_OK;
}

// error from driver
void client_err(void *arg, err_t err)
{
    struct client *c = arg;
//    if (c) client_kill(c); ??
}

// poll client
err_t client_poll(void *arg, struct tcp_pcb *pcb) {
    struct client *c = arg;
    if (!c) {
        tcp_abort(pcb);
        return ERR_ABRT;
    }
    return flushq(c);
}

// new client accepted
err_t server_newclient(void *arg, struct tcp_pcb *pcb, err_t err)
{
    struct client *c = mem_malloc(sizeof *c); // XXX should be pooled!
    if (!c) return ERR_MEM;

    c->st = ACCEPTED;
    c->pcb = pcb;
    c->p = NULL;

    // hook up callbacks
    tcp_arg(pcb, c);
    tcp_recv(pcb, client_recv); // on data received
    tcp_err(pcb, client_err); // on error
    tcp_poll(pcb, client_poll, 1); // poll status
    tcp_sent(pcb, client_sent); // queued data sent
    return ERR_OK;
}


int main()
{
    printf("testing network, hit 'x' to terminate...\n");

    net_init(NULL, 0, 0, 0, IP_DHCP);

    struct tcp_pcb *server;
    server = tcp_new();
    tcp_bind(server, IP_ADDR_ANY, 9000);
    server = tcp_listen(server);
    tcp_accept(server, server_newclient);

    while (1)
        if (getchar()=='x') break;
    exit(0);
}
