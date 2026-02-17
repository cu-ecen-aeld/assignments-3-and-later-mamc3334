/**
 * aesdsocket program
 * Date: 2/13/2026
 * Author: Mason McGaffin
 */

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

volatile sig_atomic_t stop_server = 0;
// Receive data and write to file

void signal_handler(int signum) {
   if (signum == SIGINT || signum == SIGTERM)
   {
      syslog(LOG_INFO, "Caught signal, exiting");
      stop_server = 1;
   }
   else
   {
      syslog(LOG_WARNING, "Caught unexpected signal %d", signum);
   }
}

void usage()
{
   fprintf(stdout, "The program \"aesdsocket\" allows users to start a socket application with the option to configure it as a daemon.\n"
                   "\nUsage: aesdsocket [-d]\n"
                   "\t-d: Run in daemon mode\n");
}

int main(int argc, char *argv[])
{
   int daemon_mode = 0;

   printf("Logging to syslog with identifier 'aesdsocket'\n");
   openlog("aesdsocket", 0, LOG_USER);

   if (argc == 1) {
      printf("Starting aesdsocket server\n");
      syslog(LOG_INFO, "Starting aesdsocket server");
   }
   else if (argc == 2 && strcmp(argv[1], "-d") == 0) {
      printf("Starting aesdsocket server in daemon mode\n");
      syslog(LOG_INFO, "Starting aesdsocket server in daemon mode");
      daemon_mode = 1;
   }
   else {
      fprintf(stderr, "Invalid command line arguments\n");
      syslog(LOG_ERR, "Invalid command line arguments");
      usage();
      closelog();

      return -1;
   }

   // Set up signal handler
   signal(SIGINT, signal_handler);
   signal(SIGTERM, signal_handler);

   // Set up socket
   int status;
   struct addrinfo hints, *servInfo;
   memset(&hints, 0, sizeof(hints));
   hints.ai_family = AF_INET;
   hints.ai_socktype = SOCK_STREAM;
   hints.ai_flags = AI_PASSIVE;
   status = getaddrinfo(NULL, "9000", &hints, &servInfo);

   if (status != 0)
   {
      syslog(LOG_ERR, "getaddrinfo failed: %s", gai_strerror(status));
      freeaddrinfo(servInfo);

      return -1;
   }

   // Create socket
   int sock_fd = socket(servInfo->ai_family, servInfo->ai_socktype, servInfo->ai_protocol);

   if (sock_fd < 0)
   {
      syslog(LOG_ERR, "Could not create socket: %s", strerror(errno));
      freeaddrinfo(servInfo);

      return -1;
   }

   syslog(LOG_INFO, "Socket created successfully");

   // Allow socket reuse
   int optval = 1;
   status = setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
   
   if (status < 0)
   {
      syslog(LOG_ERR, "setsockopt failed: %s", strerror(errno));
      close(sock_fd);
      freeaddrinfo(servInfo);

      return -1;
   }

   // Set receive timeout for accept() to allow signal termination
   struct timeval timeout;
   timeout.tv_sec = 1;  // 1 second timeout
   timeout.tv_usec = 0;

   status = setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
   if (status < 0)
   {
      syslog(LOG_ERR, "setsockopt SO_RCVTIMEO failed: %s", strerror(errno));
   }

   // Bind socket
   if (bind(sock_fd, servInfo->ai_addr, servInfo->ai_addrlen) < 0)
   {
      close(sock_fd);
      syslog(LOG_ERR, "Socket bind failed: %s", strerror(errno));
      freeaddrinfo(servInfo);

      return -1;
   }

   syslog(LOG_INFO, "Socket bound to port %d successfully", PORT);

   freeaddrinfo(servInfo); // No longer needed after bind

   // Daemonize if requested
   if (daemon_mode)
   {
      pid_t pid = fork();
      if (pid < 0)
      {
         syslog(LOG_ERR, "Fork failed: %s", strerror(errno));
         close(sock_fd);

         return -1;
      }
      else if (pid > 0)
      {
         // Parent process exits
         syslog(LOG_INFO, "Daemon process started with PID %d", pid);
         close(sock_fd);
         exit(0);
      }
      // Child process continues as daemon
      int status = setsid();

      if (status < 0)
      {
         syslog(LOG_ERR, "setsid failed: %s", strerror(errno));
         close(sock_fd);

         return -1;
      }
      
      umask(0);

      status = chdir("/");

      if (status < 0)
      {
         syslog(LOG_ERR, "chdir failed: %s", strerror(errno));
         close(sock_fd);

         return -1;
      }
      
      close(STDIN_FILENO);
      close(STDOUT_FILENO);
      close(STDERR_FILENO);

      syslog(LOG_INFO, "Daemon process session created successfully");
   }

   // Listen for connections
   if (listen(sock_fd, BACKLOG) < 0)
   {
      syslog(LOG_ERR, "Socket listen failed: %s", strerror(errno));
      close(sock_fd);

      return -1;
   }

   syslog(LOG_INFO, "Socket is listening for connections");

   while (!stop_server)
   {
      // Accept a connection
      struct sockaddr_in address;
      socklen_t addrlen = sizeof(address);
      int conn = accept(sock_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);

      if (conn < 0)
      {
         if (stop_server)
         {
               break; // Interrupted by signal
         }
         else if (errno == EWOULDBLOCK || errno == EAGAIN)
         {
               continue; // Expected timeout
         }
         else
         {
               syslog(LOG_ERR, "Accept failed: %s", strerror(errno));
               break; // Accept failed, exit loop
         }
      }

      syslog(LOG_INFO, "Accepted connection from %s", inet_ntoa(address.sin_addr));

      char *packet = NULL;
      size_t packet_size = 0;

      while (1)
      {
         char recv_buffer[BUFFER_SIZE];
         ssize_t bytes_received = recv(conn, recv_buffer, sizeof(recv_buffer), 0);

         if (bytes_received < 0)
         {
            syslog(LOG_ERR, "Receive failed: %s", strerror(errno));
            break; 
         }
         else if (bytes_received == 0)
         {
            syslog(LOG_INFO, "Connection closed by client");
            break; 
         }

         // Create packet
         packet = realloc(packet, packet_size + bytes_received);
         
         if (packet == NULL)
         {
            syslog(LOG_ERR, "Memory allocation failed");
            free(packet);
            break;
         }
         
         // copy received data to packet
         memcpy(packet + packet_size, recv_buffer, bytes_received);
         packet_size += bytes_received;

         // Check for newline - end of packet
         if (memchr(recv_buffer, '\n', bytes_received))
         {
            break; 
         }
      }

      if (packet != NULL)
      {
         // Write packet to file
         int fd = open(FILE_PATH, O_WRONLY | O_CREAT | O_APPEND, 0644);
         
         if (fd < 0)
         {
            syslog(LOG_ERR, "Could not open file: %s", strerror(errno));
            free(packet);
            close(fd);
            close(conn);
            continue;
         }

         ssize_t bytes_written = write(fd, packet, packet_size);
         
         if (bytes_written < 0)
         {
            syslog(LOG_ERR, "Write to file failed: %s", strerror(errno));
            free(packet);
            close(fd);
            close(conn);
            continue;
         }
         

         syslog(LOG_INFO, "Wrote %zd bytes to file", bytes_written);
         close(fd);

         // Send file contents back to client
         char file_buffer[BUFFER_SIZE];
         ssize_t bytes_read;
         fd = open(FILE_PATH, O_RDONLY);

         while (1)
         {
            bytes_read = read(fd, file_buffer, sizeof(file_buffer));
            
            if (bytes_read < 0)
            {
               syslog(LOG_ERR, "Read from file failed: %s", strerror(errno));
               break;
            }
            else if (bytes_read == 0)
            {
               break; // End of file
            }

            ssize_t bytes_sent = send(conn, file_buffer, bytes_read, 0);
            
            if (bytes_sent < 0)
            {
               syslog(LOG_ERR, "Send failed: %s", strerror(errno));
               break;
            }
         }

         free(packet);
         close(fd);
      }
      close(conn);
      syslog(LOG_INFO, "Closed connection from %s", inet_ntoa(address.sin_addr));
   }

   // Close socket, remove file, and close syslog
   syslog(LOG_INFO, "Shutting down server");
   close(sock_fd);
   remove(FILE_PATH);
   closelog();

   return 0;
}

