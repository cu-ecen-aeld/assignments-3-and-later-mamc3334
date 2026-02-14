#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <errno.h>
#include <netdb.h>

#define PORT 9000
#define BUFFER_SIZE 1024
#define FILE_PATH "/var/tmp/aesdsocketdata"
#define BACKLOG 5

void usage();
int start_daemon();
int start_server_socket(bool daemon_mode);
int run_server_socket(int sock_fd);
int handle_client_connection(int client_fd);
void signal_handler(int signum);
