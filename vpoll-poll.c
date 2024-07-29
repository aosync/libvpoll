#include "vpoll.h"

#include <stdlib.h>
#include <stdio.h>
#include <poll.h>
#include <sys/poll.h>

struct veventd {
	void *udata;
	short mode;
};

struct vpoll {
	struct pollfd *pfds;
	struct veventd *vtds;
	int ev_len;
	int ev_cap;
};

static short events_v2p(short vev) {
	short pev = 0;

	if (vev & VPOLL_READ)
		pev |= POLLIN;
	if (vev & VPOLL_WRITE)
		pev |= POLLOUT;
	if (vev & VPOLL_EOF)
		pev |= POLLHUP;
	if (vev & VPOLL_ERR)
		pev |= POLLERR;

	return pev;
}

static short events_p2v(short pev) {
	short vev = 0;

	if (pev & POLLIN)
		vev |= VPOLL_READ;
	if (pev & POLLOUT)
		vev |= VPOLL_WRITE;
	if (pev & POLLHUP)
		vev |= VPOLL_EOF;
	if (pev & POLLERR)
		vev |= VPOLL_ERR;

	return vev;
}

struct vpoll *vpoll_create(void) {
	struct vpoll *self = malloc(sizeof(struct vpoll));
	if (!self)
		goto vpoll_create_self;

	*self = (struct vpoll) {
		.pfds = malloc(sizeof(struct pollfd)),
		.vtds = malloc(sizeof(struct veventd)),
		.ev_len = 0,
		.ev_cap = 1,
	};

	if (!self->pfds || !self->vtds)
		goto vpoll_create_ds;

	return self;
vpoll_create_ds:
	vpoll_destroy(self);
vpoll_create_self:
	return NULL;
}

void vpoll_destroy(struct vpoll *self) {
	free(self->pfds);
	free(self->vtds);
	free(self);
}

void vpoll_register(struct vpoll *self, struct vevent event) {
	struct pollfd pfd = (struct pollfd) {
		.fd = event.ident,
		.events = events_v2p(event.events),
	};

	struct veventd vtd = (struct veventd) {
		.mode = event.mode,
		.udata = event.udata,
	};

	self->pfds[self->ev_len] = pfd;
	self->vtds[self->ev_len] = vtd;
	self->ev_len++;

	if (2 * self->ev_len > self->ev_cap) {
		self->ev_cap *= 2;
		self->pfds = realloc(self->pfds, self->ev_cap * sizeof(struct pollfd));
		self->vtds = realloc(self->vtds, self->ev_cap * sizeof(struct veventd));
	}
}

static void vpoll_unregister_by_id(struct vpoll *self, int id) {
	self->pfds[id] = self->pfds[self->ev_len - 1];
	self->vtds[id] = self->vtds[self->ev_len - 1];
	self->ev_len--;

	if (2 * self->ev_len <= self->ev_cap) {
		self->ev_cap /= 2;
		self->pfds = realloc(self->pfds, self->ev_cap * sizeof(struct pollfd));
		self->vtds = realloc(self->vtds, self->ev_cap * sizeof(struct veventd));
	}
}

void vpoll_unregister(struct vpoll *self, struct vevent event) {
	int i = 0;
	for (; self->pfds[i].fd != event.ident; i++);
	vpoll_unregister_by_id(self, i);
}

int vpoll_wait(struct vpoll *self, struct vevent *events, int maxevents, int timeout) {
	int nev = poll(self->pfds, self->ev_len, timeout);

	int iev = 0;
	for (int i = 0; i < self->ev_len && iev < maxevents; i++) {
		if (!self->pfds[i].revents)
			continue;

		events[iev] = (struct vevent) {
			.ident = self->pfds[i].fd,
			.events = events_p2v(self->pfds[i].revents),
			.mode = self->vtds[i].mode,
			.udata = self->vtds[i].udata,
		};

		if (self->vtds[i].mode & VPOLL_KEEP)
			self->pfds[i].revents = 0;
		else
			vpoll_unregister_by_id(self, i--);

		iev++;
	}

	return iev;
}
