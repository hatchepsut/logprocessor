//
//  vec.c
//  csp
//
//  Created by Peter Söderlind on 2017-02-14.
//
//

#include "pollfd_vec.h"

int pollfd_init(pollfds_t *pollfds) {
    pollfds->size = 0;
    return(0);
}

int pollfd_add(pollfds_t *pollfds, int fd) {
    pollfds->fds[pollfds->size].fd = fd;
    pollfds->fds[pollfds->size].events = POLLIN;
    pollfds->fds[pollfds->size].revents = 0;
    pollfds->size++;
    return(0);
}

int pollfd_remove(pollfds_t *pollfds, int fd) {
    int shifting=0;
    for(int i=0; i <= pollfds->size; i++) {
        if(pollfds->fds[i].fd == fd) {
            shifting=1;
        }
        if(shifting) {
            pollfds->fds[i] = pollfds->fds[i+1];
        }
    }
    pollfds->size--;
    return(0);
}
