#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <poll.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <errno.h>
#include <sys/epoll.h>

#include "poller.h"
#include "aux.h"




#define POLLER_BSEARCH(b,l,k,e)	mybsearch(b, l, sizeof(struct pollfd), k, OFFSET_OF(struct pollfd, fd), sizeof(*(k)), e)




struct timer_item {
	void		(*func)(void *);
	void		*ptr;
};





struct poller *poller_new(void) {
	struct poller *p;

	p = malloc(sizeof(*p));
	memset(p, 0, sizeof(*p));
	p->now = time(NULL);
	p->fd = epoll_create1(0);
	if (p->fd == -1)
		abort();

	return p;
}


static int epoll_events(struct poller_item *i) {
	return EPOLLHUP | EPOLLERR | ((i->writeable && i->blocked) ? EPOLLOUT : 0) | (i->readable ? EPOLLIN : 0);
}


int poller_add_item(struct poller *p, struct poller_item *i) {
	struct poller_item *ip;
	unsigned int u;
	struct epoll_event e;

	if (!p || !i)
		return -1;
	if (i->fd < 0)
		return -1;
	if (!i->readable && !i->writeable)
		return -1;
	if (!i->closed)
		return -1;

	if (i->fd < p->items_size && p->items[i->fd])
		return -1;

	e.events = epoll_events(i);
	e.data.fd = i->fd;
	if (epoll_ctl(p->fd, EPOLL_CTL_ADD, i->fd, &e))
		abort();

	if (i->fd >= p->items_size) {
		u = p->items_size;
		p->items_size = i->fd + 1;
		p->items = realloc(p->items, sizeof(*p->items) * p->items_size);
		memset(p->items + u, 0, sizeof(*p->items) * (p->items_size - u - 1));
	}

	ip = malloc(sizeof(*ip));
	memcpy(ip, i, sizeof(*ip));
	p->items[i->fd] = ip;

	return 0;
}


int poller_del_item(struct poller *p, int fd) {
	if (!p || fd < 0)
		return -1;
	if (fd >= p->items_size)
		return -1;
	if (!p->items || !p->items[fd])
		return -1;

	if (epoll_ctl(p->fd, EPOLL_CTL_DEL, fd, NULL))
		abort();

	free(p->items[fd]);
	p->items[fd] = NULL;

	return 0;
}


int poller_update_item(struct poller *p, struct poller_item *i) {
	struct poller_item *np;

	if (!p || !i)
		return -1;
	if (i->fd < 0)
		return -1;
	if (!i->readable && !i->writeable)
		return -1;
	if (!i->closed)
		return -1;

	if (i->fd >= p->items_size || !(np = p->items[i->fd]))
		return poller_add_item(p, i);

	np->ptr = i->ptr;
	np->readable = i->readable;
	np->writeable = i->writeable;
	np->closed = i->closed;
	np->timer = i->timer;

	return 0;
}


int poller_poll(struct poller *p, int timeout) {
	int ret, i;
	struct poller_item *it;
	time_t last;
	GList *li;
	struct timer_item *ti;
	struct epoll_event evs[128], *ev, e;

	if (!p)
		return -1;
	if (!p->items || !p->items_size)
		return -1;

	last = p->now;
	p->now = time(NULL);
	if (last != p->now) {
		for (li = p->timers; li; li = li->next) {
			ti = li->data;
			ti->func(ti->ptr);
		}

		for (i = 0; i < p->items_size; i++) {
			it = p->items[i];
			if (!it)
				continue;
			if (!it->timer)
				continue;
			it->timer(it->fd, it->ptr);
		}
		return p->items_size;
	}

	errno = 0;
	ret = epoll_wait(p->fd, evs, sizeof(evs) / sizeof(*evs), timeout);

	if (errno == EINTR)
		ret = 0;
	if (ret < 0)
		goto out;

	for (i = 0; i < ret; i++) {
		ev = &evs[i];

		if (ev->data.fd < 0)
			continue;

		it = (ev->data.fd < p->items_size) ? p->items[ev->data.fd] : NULL;
		if (!it)
			continue;

		if (it->error) {
			it->closed(it->fd, it->ptr);
			continue;
		}

		if ((ev->events & (POLLERR | POLLHUP)))
			it->closed(it->fd, it->ptr);
		else if ((ev->events & POLLOUT)) {
			it->blocked = 0;

			e.events = epoll_events(it);
			e.data.fd = it->fd;
			if (epoll_ctl(p->fd, EPOLL_CTL_MOD, it->fd, &e))
				abort();

			it->writeable(it->fd, it->ptr);
		}
		else if ((ev->events & POLLIN))
			it->readable(it->fd, it->ptr);
		else if (!ev->events)
			continue;
		else
			abort();
	}


out:
	return ret;
}


void poller_blocked(struct poller *p, int fd) {
	struct epoll_event e;

	if (!p || fd < 0)
		return;
	if (fd >= p->items_size)
		return;
	if (!p->items || !p->items[fd])
		return;
	if (!p->items[fd]->writeable)
		return;

	p->items[fd]->blocked = 1;

	e.events = epoll_events(p->items[fd]);
	e.data.fd = fd;
	if (epoll_ctl(p->fd, EPOLL_CTL_MOD, fd, &e))
		abort();
}

void poller_error(struct poller *p, int fd) {
	if (!p || fd < 0)
		return;
	if (fd >= p->items_size)
		return;
	if (!p->items || !p->items[fd])
		return;
	if (!p->items[fd]->writeable)
		return;

	p->items[fd]->error = 1;
	p->items[fd]->blocked = 1;
}

int poller_isblocked(struct poller *p, int fd) {
	if (!p || fd < 0)
		return -1;
	if (fd >= p->items_size)
		return -1;
	if (!p->items || !p->items[fd])
		return -1;
	if (!p->items[fd]->writeable)
		return -1;

	return p->items[fd]->blocked;
}




int poller_timer(struct poller *p, void (*f)(void *), void *ptr) {
	struct timer_item *i;

	if (!p || !f)
		return -1;

	i = malloc(sizeof(*i));
	ZERO(*i);

	i->func = f;
	i->ptr = ptr;

	p->timers = g_list_prepend(p->timers, i);

	return 0;
}
