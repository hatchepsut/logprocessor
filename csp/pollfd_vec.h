//
//  vec.h
//  csp
//
//  Created by Peter Söderlind on 2017-02-14.
//
//

#ifndef pollfd_vec_h
#define pollfd_vec_h

#include <poll.h>

#define POLLFDS_SIZE 256

typedef struct {
    int size;
    struct pollfd fds[POLLFDS_SIZE];
} pollfds_t;

int pollfd_init(pollfds_t *pollfds);
int pollfd_add(pollfds_t *pollfds, int fd);
int pollfd_remove(pollfds_t *pollfds, int fd);

#endif /* vec_h */
