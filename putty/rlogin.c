/*
 * Rlogin backend.
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "putty.h"

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

#define RLOGIN_MAX_BACKLOG 4096

typedef struct rlogin_tag {
    const struct plug_function_table *fn;
    void *frontend;
    /* the above field _must_ be first in the structure */

    Socket s;
    int bufsize;
    int firstbyte;
    int cansize;
    int term_width, term_height;
} *Rlogin;

static void rlogin_size(void *handle, int width, int height);

static void c_write(Rlogin rlogin, const char *buf, int len)
{
    int backlog = from_backend(rlogin->frontend, 0, buf, len);
    sk_set_frozen(rlogin->s, backlog > RLOGIN_MAX_BACKLOG);
}

static void rlogin_log(Plug plug, int type, SockAddr addr, int port,
		       const char *error_msg, int error_code)
{
    Rlogin rlogin = (Rlogin) plug;
    char addrbuf[256], *msg;

    sk_getaddr(addr, addrbuf, lenof(addrbuf));

    if (type == 0)
	msg = dupprintf("Connecting to %s port %d", addrbuf, port);
    else
	msg = dupprintf("Failed to connect to %s: %s", addrbuf, error_msg);

    logevent(rlogin->frontend, msg);
}

static int rlogin_closing(Plug plug, const char *error_msg, int error_code,
			  int calling_back)
{
    Rlogin rlogin = (Rlogin) plug;
    if (error_msg) {
		/* A socket error has occurred. */
		logevent(rlogin->frontend, error_msg);
		connection_fatal(rlogin->frontend, "%s", error_msg);
    }	
    if (rlogin->s) {
        sk_close(rlogin->s);
        rlogin->s = NULL;
		notify_remote_exit(rlogin->frontend);
    }			       /* Otherwise, the remote side closed the connection normally. */
    return 0;
}

static int rlogin_receive(Plug plug, int urgent, char *data, int len)
{
    Rlogin rlogin = (Rlogin) plug;
    if (urgent == 2) {
	char c;

	c = *data++;
	len--;
	if (c == '\x80') {
	    rlogin->cansize = 1;
	    rlogin_size(rlogin, rlogin->term_width, rlogin->term_height);
        }
	/*
	 * We should flush everything (aka Telnet SYNCH) if we see
	 * 0x02, and we should turn off and on _local_ flow control
	 * on 0x10 and 0x20 respectively. I'm not convinced it's
	 * worth it...
	 */
    } else {
	/*
	 * Main rlogin protocol. This is really simple: the first
	 * byte is expected to be NULL and is ignored, and the rest
	 * is printed.
	 */
	if (rlogin->firstbyte) {
	    if (data[0] == '\0') {
		data++;
		len--;
	    }
	    rlogin->firstbyte = 0;
	}
	if (len > 0)
            c_write(rlogin, data, len);
    }
    return 1;
}

static void rlogin_sent(Plug plug, int bufsize)
{
    Rlogin rlogin = (Rlogin) plug;
    rlogin->bufsize = bufsize;
}

/*
 * Called to set up the rlogin connection.
 * 
 * Returns an error message, or NULL on success.
 *
 * Also places the canonical host name into `realhost'. It must be
 * freed by the caller.
 */
static const char *rlogin_init(void *frontend_handle, void **backend_handle,
			       Config *cfg,
			       char *host, int port, char **realhost,
			       int nodelay, int keepalive)
{
    static const struct plug_function_table fn_table = {
	rlogin_log,
	rlogin_closing,
	rlogin_receive,
	rlogin_sent
    };
    SockAddr addr;
    const char *err;
    Rlogin rlogin;

    rlogin = snew(struct rlogin_tag);
    rlogin->fn = &fn_table;
    rlogin->s = NULL;
    rlogin->frontend = frontend_handle;
    rlogin->term_width = cfg->width;
    rlogin->term_height = cfg->height;
    rlogin->firstbyte = 1;
    rlogin->cansize = 0;
    *backend_handle = rlogin;

    /*
     * Try to find host.
     */
    {
	char *buf;
	buf = dupprintf("Looking up host \"%s\"%s", host,
			(cfg->addressfamily == ADDRTYPE_IPV4 ? " (IPv4)" :
			 (cfg->addressfamily == ADDRTYPE_IPV6 ? " (IPv6)" :
			  "")));
	logevent(rlogin->frontend, buf);
	sfree(buf);
    }
    addr = name_lookup(host, port, realhost, cfg, cfg->addressfamily);
    if ((err = sk_addr_error(addr)) != NULL) {
	sk_addr_free(addr);
	return err;
    }

    if (port < 0)
	port = 513;		       /* default rlogin port */

    /*
     * Open socket.
     */
    rlogin->s = new_connection(addr, *realhost, port, 1, 0,
			       nodelay, keepalive, (Plug) rlogin, cfg);
    if ((err = sk_socket_error(rlogin->s)) != NULL)
	return err;

    /*
     * Send local username, remote username, terminal/speed
     */

    {
	char z = 0;
	char *p;
	char ruser[sizeof(cfg->username)];
	(void) get_remote_username(cfg, ruser, sizeof(ruser));
	sk_write(rlogin->s, &z, 1);
	sk_write(rlogin->s, cfg->localusername,
		 strlen(cfg->localusername));
	sk_write(rlogin->s, &z, 1);
	sk_write(rlogin->s, ruser,
		 strlen(ruser));
	sk_write(rlogin->s, &z, 1);
	sk_write(rlogin->s, cfg->termtype,
		 strlen(cfg->termtype));
	sk_write(rlogin->s, "/", 1);
	for (p = cfg->termspeed; isdigit((unsigned char)*p); p++) continue;
	sk_write(rlogin->s, cfg->termspeed, p - cfg->termspeed);
	rlogin->bufsize = sk_write(rlogin->s, &z, 1);
    }

    if (*cfg->loghost) {
	char *colon;

	sfree(*realhost);
	*realhost = dupstr(cfg->loghost);
	colon = strrchr(*realhost, ':');
	if (colon) {
	    /*
	     * FIXME: if we ever update this aspect of ssh.c for
	     * IPv6 literal management, this should change in line
	     * with it.
	     */
	    *colon++ = '\0';
	}
    }

    return NULL;
}

static void rlogin_free(void *handle)
{
    Rlogin rlogin = (Rlogin) handle;

    if (rlogin->s)
	sk_close(rlogin->s);
    sfree(rlogin);
}

/*
 * Stub routine (we don't have any need to reconfigure this backend).
 */
static void rlogin_reconfig(void *handle, Config *cfg)
{
}

/*
 * Called to send data down the rlogin connection.
 */
static int rlogin_send(void *handle, const char *buf, int len)
{
    Rlogin rlogin = (Rlogin) handle;

    if (rlogin->s == NULL)
	return 0;

    rlogin->bufsize = sk_write(rlogin->s, buf, len);

    return rlogin->bufsize;
}

/*
 * Called to query the current socket sendability status.
 */
static int rlogin_sendbuffer(void *handle)
{
    Rlogin rlogin = (Rlogin) handle;
    return rlogin->bufsize;
}

/*
 * Called to set the size of the window
 */
static void rlogin_size(void *handle, int width, int height)
{
    Rlogin rlogin = (Rlogin) handle;
    char b[12] = { '\xFF', '\xFF', 0x73, 0x73, 0, 0, 0, 0, 0, 0, 0, 0 };

    rlogin->term_width = width;
    rlogin->term_height = height;

    if (rlogin->s == NULL || !rlogin->cansize)
	return;

    b[6] = rlogin->term_width >> 8;
    b[7] = rlogin->term_width & 0xFF;
    b[4] = rlogin->term_height >> 8;
    b[5] = rlogin->term_height & 0xFF;
    rlogin->bufsize = sk_write(rlogin->s, b, 12);
    return;
}

/*
 * Send rlogin special codes.
 */
static void rlogin_special(void *handle, Telnet_Special code)
{
    /* Do nothing! */
    return;
}

/*
 * Return a list of the special codes that make sense in this
 * protocol.
 */
static const struct telnet_special *rlogin_get_specials(void *handle)
{
    return NULL;
}

static int rlogin_connected(void *handle)
{
    Rlogin rlogin = (Rlogin) handle;
    return rlogin->s != NULL;
}

static int rlogin_sendok(void *handle)
{
    /* Rlogin rlogin = (Rlogin) handle; */
    return 1;
}

static void rlogin_unthrottle(void *handle, int backlog)
{
    Rlogin rlogin = (Rlogin) handle;
    sk_set_frozen(rlogin->s, backlog > RLOGIN_MAX_BACKLOG);
}

static int rlogin_ldisc(void *handle, int option)
{
    /* Rlogin rlogin = (Rlogin) handle; */
    return 0;
}

static void rlogin_provide_ldisc(void *handle, void *ldisc)
{
    /* This is a stub. */
}

static void rlogin_provide_logctx(void *handle, void *logctx)
{
    /* This is a stub. */
}

static int rlogin_exitcode(void *handle)
{
    Rlogin rlogin = (Rlogin) handle;
    if (rlogin->s != NULL)
        return -1;                     /* still connected */
    else
        /* If we ever implement RSH, we'll probably need to do this properly */
        return 0;
}

/*
 * cfg_info for rlogin does nothing at all.
 */
static int rlogin_cfg_info(void *handle)
{
    return 0;
}

Backend rlogin_backend = {
    rlogin_init,
    rlogin_free,
    rlogin_reconfig,
    rlogin_send,
    rlogin_sendbuffer,
    rlogin_size,
    rlogin_special,
    rlogin_get_specials,
    rlogin_connected,
    rlogin_exitcode,
    rlogin_sendok,
    rlogin_ldisc,
    rlogin_provide_ldisc,
    rlogin_provide_logctx,
    rlogin_unthrottle,
    rlogin_cfg_info,
    "rlogin",
    PROT_RLOGIN,
    513
};
