#include "vpoll.h"

#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/epoll.h>

struct veventd {
	void *udata;
	struct veventd *prev;
	struct veventd *next;
	int fd;
	short mode;
};

struct vpoll {
	int epfd;
	struct veventd *ped;
};

static uint32_t events_v2e(short vev) {
	uint32_t eev = 0;

	if (vev & VPOLL_READ)
		eev |= EPOLLIN;
	if (vev & VPOLL_WRITE)
		eev |= EPOLLOUT;
	if (vev & VPOLL_EOF)
		eev |= EPOLLHUP;
	if (vev & VPOLL_ERR)
		eev |= EPOLLERR;

	return eev;
}

static short events_e2v(uint32_t eev) {
	short vev = 0;

	if (eev & EPOLLIN)
		vev |= VPOLL_READ;
	if (eev & EPOLLOUT)
		vev |= VPOLL_WRITE;
	if (eev & EPOLLHUP)
		vev |= VPOLL_EOF;
	if (eev & EPOLLERR)
		vev |= VPOLL_ERR;

	return vev;
}

struct vpoll *vpoll_create(void) {
	struct vpoll *self = malloc(sizeof(struct vpoll));
	if (!self)
		goto vpoll_create_self;

	self->epfd = epoll_create(1);
	if (self->epfd < 0)
		goto vpoll_create_ep;

	return self;
vpoll_create_ep:
	free(self);
vpoll_create_self:
	return NULL;
}

void vpoll_destroy(struct vpoll *self) {
	struct veventd *vev = self->ped;
	while (vev) {
		struct veventd *pvev = vev;
		vev = vev->next;
		free(pvev);
	}

	close(self->epfd);
	free(self);
}

static void vpoll_remove_veventd(struct vpoll *self, struct veventd *vtd) {
	if (vtd->next)
		vtd->next->prev = vtd->prev;
	if (vtd->prev)
		vtd->prev->next = vtd->next;
	else
		self->ped = vtd->next;

	epoll_ctl(self->epfd, EPOLL_CTL_DEL, vtd->fd, NULL);
	free(vtd);
}

void vpoll_register(struct vpoll *self, struct vevent event) {
	struct veventd *vtd = malloc(sizeof(struct veventd));
	*vtd = (struct veventd){
		.fd = event.ident,
		.prev = NULL,
		.next = self->ped,
		.mode = event.mode,
		.udata = event.udata,
	};

	struct epoll_event eev = (struct epoll_event) {
		.events = events_v2e(event.events),
		.data.ptr = vtd,
	};

	epoll_ctl(self->epfd, EPOLL_CTL_ADD, event.ident, &eev);
	if (self->ped)
		self->ped->prev = vtd;
	self->ped = vtd;
}

void vpoll_unregister(struct vpoll *self, struct vevent event) {
	struct veventd *vtd = self->ped;
	while (vtd && vtd->fd != event.ident)
		vtd = vtd->next;

	if (!vtd)
		return;

	vpoll_remove_veventd(self, vtd);
}

int vpoll_wait(struct vpoll *self, struct vevent *events, int maxevents, int timeout) {
	struct epoll_event eevs[maxevents];
	int nev = epoll_wait(self->epfd, eevs, maxevents, timeout);

	for (int i = 0; i < nev; i++) {
		struct veventd *vtd = (struct veventd *)eevs[i].data.ptr;

		events[i] = (struct vevent) {
			.ident = vtd->fd,
			.events = events_e2v(eevs[i].events),
			.mode = vtd->mode,
			.udata = vtd->udata,
		};

		if (vtd->mode & VPOLL_KEEP)
			continue;

		vpoll_remove_veventd(self, vtd);
	}

	return nev;
}
