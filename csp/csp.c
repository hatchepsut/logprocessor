#include <stdio.h>
#include <poll.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <signal.h>
#include <netdb.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include "pollfd_vec.h"

#define DAEMON 0

#define BUFFERSIZE 4096


typedef struct {
    long bytes_read;
    long bytes_left_to_read;
    int ofd;
    char *buf;
    char *body;
    uint64_t read_time; // Used for read timeout
    uint64_t write_time; // Used for write timeout
} session_t;

char *logfilename = "error.log";

int close_connection( session_t sessions[], int esock) {
    session_t *s;
    
    close(esock); // Socket is automatically deregistered from /dev/poll on close */
    if(sessions[esock].ofd) close(sessions[esock].ofd);
    free(sessions[esock].buf);
    
    /* Clear memory for session */
    s = &sessions[esock];
    memset(s, 0, sizeof(session_t));
    return(0);
}

long seconds_to_midnight() {
    time_t stm, t, tt1, tt2;
    
    struct tm *lt;
    
    t = time(NULL);
    lt = localtime(&t);
    tt1 = mktime(lt);
    
    lt->tm_sec = 59;
    lt->tm_min = 59;
    lt->tm_hour = 23;
    tt2 = mktime(lt);
    
    stm = tt2 - tt1 + 1;
    
    return(stm);
    
}

int open_error_log(char *filename) {
    int fd=STDOUT_FILENO;
    
    if(filename != NULL) {
        fd = open(filename, O_WRONLY|O_CREAT|O_APPEND, S_IRWXU|S_IRWXG);
        if(fd < 0) {
            fprintf(stderr, "Can't open error log: %s : %s\n", filename, strerror(errno));
            exit(-1);
        }
    }
    
    close(STDERR_FILENO);
    
    return dup2(fd, STDERR_FILENO);
    
}

static void logrotate(int signum)
{
    int fd;
    long stm;
    
    char newlogfilename[1024];
    strncpy(newlogfilename, logfilename, 1024);
    strcat(newlogfilename, ".old");
    rename(logfilename, newlogfilename);
    fd = open_error_log(logfilename);
    char buf[256];
    stm = seconds_to_midnight();
    sprintf(buf, "Logrotation in %ld seconds\n", stm);
    alarm((int)stm); /* Set alarm to next logrotate */
    fprintf(stderr, "%s", buf);
}

char *timestr() {
    static char *tstr;
    
    time_t t = time(0);
    tstr = ctime(&t);
    tstr[strlen(tstr)-1] = '\0';
    return(tstr);
}


int main(int argc, char *argv[]) {
    int err;
    long n;
    int nsocks;
    //int evpfd;
    int lsock, csock, esock;
    int lognr=0;
    int pid;
    int polltimeout = 1000;
    int loghealthcheck = 0;
    //int ti;
    struct timespec now, lastcheck;
    pollfds_t pollfds;
    
    
    char ip[INET_ADDRSTRLEN];
    char *logpath=".";
    
    //ssize_t retval;
    
    session_t sessions[65536];
    
    int content_length=0;
    char clbuf[16];
    char *cl, *cle;
    
    char *vvptr; // Used to overwrite VVSTAT cookie.
    
    char ofile[40];
    
    struct protoent *proto;
    
    struct sockaddr_in serv_addr;
    struct sockaddr_in peer_addr;
    unsigned int peer_addr_length;
    
    char c;
    extern int optind, optopt;
    
    char timebuf[256];
    
    /* Set up signalhandlning to support logfile rotation */
    struct sigaction sa;
    sa.sa_handler = logrotate;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    
    if(sigaction ( SIGALRM, &sa, NULL) == -1) {
        perror("Can't install signal handler!");
        return(-1);
    }
    
    while ((c = getopt(argc, argv, "td:l:r:")) != -1) {
        switch (c) {
            case 'd':
                logpath = strdup(optarg);
                break;
            case 'h':
                loghealthcheck = 1;
            case 'l':
                logfilename = strdup(optarg);
                break;
            case 'r':
                polltimeout = atoi(optarg);
                break;
            case 't':
                fprintf(stderr, "OK\n");
                exit(0);
            default:
                fprintf(stderr, "Usage: %s [-t]\n", argv[0]);
                exit(-1);
        }
        
    }
    
    // Open logfile and set timer for logrotate
    open_error_log(logfilename);
    int stm = (int)seconds_to_midnight();
    alarm(stm); /* Set alarm to when logrotation should happen */
    
    pid = getpid();
    
    proto = getprotobyname("TCP");
    lsock = socket(AF_INET, SOCK_STREAM, proto->p_proto);
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(8090);
    
    if(bind(lsock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0 ) {
        perror("ERROR on binding");
        exit(1);
    }
    
    err = listen(lsock, SOMAXCONN);
    if(err <0) {
        perror("listen");
        exit(-1);
    }
    
    pollfd_add(&pollfds, lsock);
    
    memset(sessions, 0, sizeof(sessions));
    
#if DAEMON
    // Daemonize here
    for (int fd = 0; fd < _NFILE; fd++)
        close(fd); /* close all file descriptors */
    
    pid = fork();
    if(pid > 0) exit(0); // Let parent terminate
    
    if(pid < 0) {
        perror("Can't fork! (first)");
        exit(-1);
    }
    
    setsid(); // Child becomed session leader
    
    pid = fork();
    if(pid > 0) exit(0); // Let parent terminate
    if(pid < 0) {
        perror("Can't fork! (second)");
        exit(-1);
    }
#endif
    fprintf(stderr, "Entering endless loop!\n");
    while(1) {
        
        nsocks = poll(pollfds.fds, pollfds.size, 100000);
        
        if(nsocks == -1) {
            perror("poll");
            if (errno == EINTR) {
                fprintf(stderr, "poll vart avbruten! På'n igen!\n");
                continue;
            }
        }
        
        clock_gettime(CLOCK_MONOTONIC, &now);
        
        /* Check each socket for timeout every 3 seconds */
        if(now.tv_sec - lastcheck.tv_sec > 3 ) {
            lastcheck.tv_sec = now.tv_sec;
            for(int sock=lsock+1; sock < 65536; sock++) {
                if(sessions[sock].read_time == 0) continue; // Skip not used sockets
                if(now.tv_sec - sessions[sock].read_time >= 300) {
                    fprintf(stderr, "%s: Closing socket: %d timeout!\n", timestr(), sock);
                    close_connection(sessions, sock);
                }
            }
        }
        
        for(int i=0; i < pollfds.size; i++) {
            
            if(pollfds.fds[i].revents == 0 ) {
                // nothing has happend with this socket. Nothing to do.
                continue;
            }

            pollfds.fds[i].revents = 0; // Reset event for this socket. Because I belive I have to..
            
            esock = pollfds.fds[i].fd; // Current socket to process
            
            if( esock == lsock ) {
                
                /* Accept new connection and register socket to /dev/poll */
                csock = accept(lsock, NULL, 0);
                fprintf(stderr, "%s: Accepting client on socket %d.\n", timestr(), csock);
                
                pollfd_add(&pollfds, csock);
                pollfds.fds[csock].events = POLLIN|POLLHUP|POLLERR;
                pollfds.fds[csock].revents = 0;
                
                sessions[csock].ofd = 0;
                sessions[csock].body = 0;
                sessions[csock].bytes_read = 0;
                sessions[csock].bytes_left_to_read = BUFFERSIZE;
                sessions[csock].read_time = now.tv_sec;
                sessions[csock].write_time = now.tv_sec;
                sessions[csock].buf = (char *)malloc(BUFFERSIZE);
                memset(sessions[csock].buf, 0, BUFFERSIZE);
                continue;
                
            } else {
                
                n = read(esock, sessions[esock].buf+sessions[esock].bytes_read, sessions[esock].bytes_left_to_read);
                //fprintf(stderr, "Bytes left to read(%d): %d    Read %d bytes from %d\n", esock, sessions[esock].bytes_left_to_read, n, esock );
                
                if(n == 0) {
                    
                    if(!sessions[esock].body) {
                        fprintf(stderr, "%s: Closing connection (No data)\n", timestr());
                        close_connection(sessions, esock);
                        pollfd_remove(&pollfds, esock);
                    } else {
                        fprintf(stderr, "%s: Premature end if data on connection %d.\n", timestr(), esock);
                        close_connection(sessions, esock);
                        pollfd_remove(&pollfds, esock);
                    }
                }
                
                if(n < 0) {
                    fprintf(stderr, "%s: read error esock: %d\n", timestr(), esock);
                    close_connection(sessions, esock);
                    pollfd_remove(&pollfds, esock);
                }
                
                // If data is received on socket 'esock'.
                if(n > 0) {
                    
                    sessions[esock].bytes_left_to_read -= n;
                    sessions[esock].bytes_read += n;
                    //fprintf(stderr, "bytes_left_to_read: %d.  bytes_read: %d\n", sessions[esock].bytes_left_to_read, sessions[esock].bytes_read );
                    
                    sessions[esock].read_time = now.tv_sec; // Set read time (used for timeout)
                    sessions[esock].buf[sessions[esock].bytes_read] = '\0'; // null terminate buffer
                    
                    // If reading headers
                    if(!sessions[esock].body) {
                        sessions[esock].body = strstr(sessions[esock].buf, "\r\n\r\n");
                        if(sessions[esock].body) {
                            sessions[esock].body += 4; // Skip past \r\n\r\n
                            cl = strcasestr(sessions[esock].buf, "Content-Length:");
                            if(cl > '\0') {
                                cl += 15; // Jump past header name
                                while(*cl == ' ') cl++; // Skip optional space
                                cle = strstr(cl, "\r\n");
                                strncpy(clbuf, cl, cle-cl);
                                clbuf[cle-cl] = '\0';
                                content_length=atoi(clbuf);
                                
                                if(content_length > (BUFFERSIZE - (sessions[esock].body - sessions[esock].buf))) {
                                    fprintf(stderr, "%s: Content-Length(%d) larger than free buffer space! Closing socket %d.\n", timestr(), content_length, esock);
                                    close_connection(sessions, esock);
                                    pollfd_remove(&pollfds, esock);
                                }
                                
                                sessions[esock].bytes_left_to_read = content_length - (sessions[esock].bytes_read - (sessions[esock].body - sessions[esock].buf));
                                    //fprintf(stderr, "Content-Length: %d Bytes left to read: %d\n", content_length, sessions[esock].bytes_left_to_read);
                                
                                // Overwrite confidential information
                                vvptr = strstr(sessions[esock].buf, "VVSTATE=");
                                if(vvptr) {
                                    vvptr += 8; // Jump to beginning of value
                                    for(char *ch = vvptr; ch < sessions[esock].buf + BUFFERSIZE; ch++) {
                                        if(*ch == ';') break;
                                        if(*ch == '\r') break;
                                        *ch = 'x';
                                    }
                                }
                            } else {
                                write(esock, "HTTP/1.1 200 OK\r\n\r\n", 19);
                                close_connection(sessions, esock);
                                pollfd_remove(&pollfds, esock);
                            }
                        }
                    }
                    
                    // If reading body
                    if(sessions[esock].body) {
                        
                        // Open outfile
                        if(!sessions[esock].ofd) {
                            peer_addr_length = sizeof(struct sockaddr_in);
                            n = getpeername(esock, (struct sockaddr *)&peer_addr, &peer_addr_length);
                            if(n != 0) perror("getpeername");
                            
                            sprintf(ofile, "%s/csplog-%s-%d-%d.log", logpath, inet_ntop(AF_INET, &peer_addr.sin_addr.s_addr, ip, sizeof(ip)), pid, lognr++);
                            fprintf(stderr, "%s: Opening file %s for socket %d!\n", timestr(), ofile, esock);
                            sessions[esock].ofd = open(ofile, O_WRONLY|O_CREAT, S_IRWXU|S_IRWXG);
                            if(sessions[esock].ofd < 0) {
                                close_connection(sessions, esock);
                                pollfd_remove(&pollfds, esock);
                                continue;
                            }
                            // Write timestamp to output file and update buffer state (bytes_read)
                            sprintf(timebuf, "TIME: %ld\n", time(0));
                            write(sessions[esock].ofd, timebuf, strlen(timebuf));
                        }
                    }
                    
                    //If finished reading body
                    if(sessions[esock].bytes_left_to_read <= 0) {
                        fprintf(stderr, "%s: Closing socket %d. Bytes read: %ld\n", timestr(), esock, sessions[esock].bytes_read);
                        write(sessions[esock].ofd, sessions[esock].buf, sessions[esock].bytes_read);
                        write(sessions[esock].ofd, "\n", 1);
                        /* log to syslog*/
                        write(esock, "HTTP/1.1 200 OK\r\n\r\n", 19);
                        close_connection(sessions, esock);
                        pollfd_remove(&pollfds, esock);
                    }
                }
            }
        }
    }
}
