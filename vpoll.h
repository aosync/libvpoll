#ifndef VPOLL_H
#define VPOLL_H

#define VPOLL_KEEP (1 << 0)

#define VPOLL_READ (1 << 0)
#define VPOLL_WRITE (1 << 1)
#define VPOLL_EOF (1 << 2)
#define VPOLL_ERR (1 << 3)

struct vpoll;
struct vevent {
	int ident; // change this to whatever a "fd" could be
	short events; // events mask
	short mode; // event mode
	void *udata; // user data
};

struct vpoll *vpoll_create(void);
void vpoll_destroy(struct vpoll *self);

void vpoll_register(struct vpoll *self, struct vevent event);
void vpoll_unregister(struct vpoll *self, struct vevent event);

int vpoll_wait(struct vpoll *self, struct vevent *events, int maxevents, int timeout);

#endif
