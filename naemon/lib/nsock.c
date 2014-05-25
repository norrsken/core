#define _GNU_SOURCE 1
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdarg.h>
#include <fcntl.h>
#include "snprintf.h"
#include "lnae-utils.h"
#include "nsock.h"

struct nsock_sock {
	int sd;
	unsigned int flags;
	struct sockaddr *sa;
	size_t sa_size;
};

const char *nsock_strerror(int code)
{
	switch (code) {
	case NSOCK_EBIND: return "bind() failed";
	case NSOCK_ELISTEN: return "listen() failed";
	case NSOCK_ESOCKET: return "socket() failed";
	case NSOCK_EUNLINK: return "unlink() failed";
	case NSOCK_ECONNECT: return "connect() failed";
	case NSOCK_EFCNTL: return "fcntl() failed";
	case NSOCK_EINVAL: return "Invalid arguments";
	}

	return "Unknown error";
}

static int
nsock_create_sockaddr(sa_family_t family, const char *path, int flags, struct sockaddr **sa, size_t *size)
{
	if (family == AF_UNIX) {
		int slen;
		struct sockaddr_un *saun = calloc(1, sizeof(struct sockaddr_un));
		saun->sun_family = AF_UNIX;
		slen = strlen(path);
		memcpy(&saun->sun_path, path, slen);
		slen += offsetof(struct sockaddr_un, sun_path);

		/* unlink if we're supposed to, but not if we're connecting */
		/* (this special case is bananas) */
		if (flags & NSOCK_UNLINK && !(flags & NSOCK_CONNECT)) {
			if (unlink(path) < 0 && errno != ENOENT) {
				free(saun);
				return NSOCK_EUNLINK;
			}
		}
		*sa = (struct sockaddr *)saun;
		*size = sizeof(struct sockaddr_un);
	} else if (0) {
		//return nsock_ip6();
		return NSOCK_EINVAL;
	} else if (family == AF_INET) {
		char *ip, *portstr;
		int port;
		struct sockaddr_in *sain = calloc(1, sizeof(struct sockaddr_in));
		ip = strdup(path + 6);
		portstr = strchr(ip, ':');
		if (!portstr) {
			free(sain);
			return NSOCK_EINVAL;
		}
		*portstr = 0;
		portstr++;
		port = atoi(portstr);

		if (inet_aton(ip, &sain->sin_addr) == 0) {
			free(sain);
			return NSOCK_EINVAL;
		}

		sain->sin_family = AF_INET;
		sain->sin_port = htons(port);
		*sa = (struct sockaddr *)sain;
		*size = sizeof(struct sockaddr_in);
	} else {
		return NSOCK_EINVAL;
	}

	return 0;
}

void
nsock_destroy(struct nsock_sock *sock)
{
	free(sock->sa);
	sock->sa = NULL;
	if (sock->sd > 0)
		close(sock->sd);
	free(sock);
}

int
nsock_create(const char *path, unsigned int flags, struct nsock_sock **sock)
{
	int mode;
	sa_family_t family;
	*sock = calloc(1, sizeof(struct nsock_sock));
	if (!*sock)
		return NSOCK_EINVAL;
	(*sock)->flags = flags;
	if (path[0] == '/') {
		family = AF_UNIX;
	} else if (!strncmp(path, "tcp://[", 7)) {
		//return nsock_ip6();
		nsock_destroy(*sock);
		return NSOCK_EINVAL;
	} else if (!strncmp(path, "tcp://", 6)) {
		family = AF_INET;
	} else {
		nsock_destroy(*sock);
		return NSOCK_EINVAL;
	}

	if (flags & NSOCK_TCP) {
		mode = SOCK_STREAM;
	}
	else if (flags & NSOCK_UDP) {
		mode = SOCK_DGRAM;
	}
	else {
		nsock_destroy(*sock);
		return NSOCK_EINVAL;
	}


	if (((*sock)->sd = socket(family, mode, 0)) < 0) {
		nsock_destroy(*sock);
		return NSOCK_ESOCKET;
	}

	if (nsock_create_sockaddr(family, path, flags, &(*sock)->sa, &(*sock)->sa_size) < 0) {
		nsock_destroy(*sock);
		return NSOCK_EINVAL;
	}
	return 0;
}

int
nsock_get_fd(struct nsock_sock *sock)
{
	return sock->sd;
}

int nsock_connect(struct nsock_sock *sock)
{
	int res = connect(sock->sd, sock->sa, sock->sa_size);
	if (res < 0 && errno != EINPROGRESS) {
		return NSOCK_ECONNECT;
	}
	return 0;
}

int nsock_listen(struct nsock_sock *sock)
{
	int res;
	res = bind(sock->sd, sock->sa, sock->sa_size);
	if (res < 0) {
		close(sock->sd);
		return NSOCK_EBIND;
	}

	if (sock->flags & NSOCK_UDP) {
		return 0;
	}

	if (listen(sock->sd, 3) < 0) {
		close(sock->sd);
		return NSOCK_ELISTEN;
	}

	return 0;
}

int nsock_accept(struct nsock_sock *sock)
{
	return accept(sock->sd, NULL, NULL);
}

static inline int nsock_vdprintf(int sd, const char *fmt, va_list ap, int plus)
{
	char *buf = NULL;
	int len, ret;

	len = vasprintf(&buf, fmt, ap);
	if (len < 0)
		return len;
	ret = write(sd, buf, len + plus);
	free(buf);
	return ret;
}

int nsock_printf_nul(int sd, const char *fmt, ...)
{
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = nsock_vdprintf(sd, fmt, ap, 1);
	va_end(ap);
	return ret;
}

int nsock_printf(int sd, const char *fmt, ...)
{
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = nsock_vdprintf(sd, fmt, ap, 0);
	va_end(ap);
	return ret;
}
