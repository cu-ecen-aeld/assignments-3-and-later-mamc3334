/**
 * aesdsocket program
 * Date: 2/13/2026
 * Author: Mason McGaffin
 */

#include "aesdsocket.h"

volatile sig_atomic_t stop_server = 0;
// Receive data and write to file

void signal_handler(int signum) 
{
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

int handle_client_connection(int client_fd)
{
   char *packet = NULL;
   size_t packet_size = 0;

   while (1)
   {
      char recv_buffer[BUFFER_SIZE];
      ssize_t bytes_received = recv(client_fd, recv_buffer, sizeof(recv_buffer), 0);

      if (bytes_received < 0)
      {
         syslog(LOG_ERR, "Receive failed: %s", strerror(errno));
         free(packet);
         
         return -1;
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

         return -1;
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

         return -1;
      }

      ssize_t bytes_written = write(fd, packet, packet_size);
      
      if (bytes_written < 0)
      {
         syslog(LOG_ERR, "Write to file failed: %s", strerror(errno));
         free(packet);
         close(fd);

         return -1;
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
            free(packet);
            close(fd);
            
            return -1;
         }
         else if (bytes_read == 0)
         {
            break; // End of file
         }

         ssize_t bytes_sent = send(client_fd, file_buffer, bytes_read, 0);
         
         if (bytes_sent < 0)
         {
            syslog(LOG_ERR, "Send failed: %s", strerror(errno));
            free(packet);
            close(fd);

            return -1;
         }
      }

      free(packet);
      close(fd);
   }

   return 0;
}


int start_daemon()
{
   // fork
   pid_t pid = fork();
   
   if (pid < 0)
   {
      syslog(LOG_ERR, "Failed to fork process");

      return -1;
   }
   else if (pid == 0)
   {
      // Child process
      int status = setsid();
      if (status < 0)
      {
            syslog(LOG_ERR, "Failed to create new session");
            return -1;
      }

      // Change working directory to root
      status = chdir("/");
      if (status < 0)
      {
            syslog(LOG_ERR, "Failed to change working directory to root");
            return -1;
      }

      // Redirect standard file descriptors to /dev/null
      int fd = open("/dev/null", O_RDWR);
      if (fd < 0)
      {
            syslog(LOG_ERR, "Failed to open /dev/null: %s", strerror(errno));
            return -1;
      }
      
      dup2(fd, STDIN_FILENO);
      dup2(fd, STDOUT_FILENO);
      dup2(fd, STDERR_FILENO);
   }
   else
   {
      exit(0); // Exit parent process
   }

   return 0;
}

int start_server_socket(bool daemon_mode)
{
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
      status = start_daemon();
      if (status < 0)
      {
         syslog(LOG_ERR, "Failed to daemonize");
         close(sock_fd);
         return -1;
      }
   }

   // Listen for connections
   if (listen(sock_fd, BACKLOG) < 0)
   {
      syslog(LOG_ERR, "Socket listen failed: %s", strerror(errno));
      close(sock_fd);

      return -1;
   }

   syslog(LOG_INFO, "Socket is listening for connections");

   return sock_fd;
}

int run_server_socket(int sock_fd)
{
   while (!stop_server)
   {
      struct sockaddr_storage client_addr;
      socklen_t addr_size = sizeof(client_addr);
      int client_fd = accept(sock_fd, (struct sockaddr *)&client_addr, &addr_size);

      if (client_fd < 0)
      {
         if (errno == EWOULDBLOCK || errno == EAGAIN)
         {
            continue; // Timeout occurred, check stop_server flag again
         }
         else
         {
            syslog(LOG_ERR, "Socket accept failed: %s", strerror(errno));
            return -1;
         }
      }

      syslog(LOG_INFO, "Accepted connection from client");

      // Handle client connection in a separate function
      int status = handle_client_connection(client_fd);
      if (status < 0)
      {
         syslog(LOG_ERR, "Error handling client connection");
         close(client_fd);
         return -1;
      }

      close(client_fd);
   }

   return 0;
}

void usage()
{
   fprintf(stdout, "The program \"aesdsocket\" allows users to start a socket application with the option to configure it as a daemon.\n"
                   "\nUsage: aesdsocket [-d]\n"
                   "\t-d: Run in daemon mode\n");
}

int main(int argc, char *argv[])
{
   bool daemon_mode = false;

   printf("Logging to syslog with identifier 'aesdsocket'\n");
   openlog("aesdsocket", 0, LOG_USER);

   if (argc == 1) {
      printf("Starting aesdsocket server\n");
      syslog(LOG_INFO, "Starting aesdsocket server");
   }
   else if (argc == 2 && strcmp(argv[1], "-d") == 0) {
      printf("Starting aesdsocket server in daemon mode\n");
      syslog(LOG_INFO, "Starting aesdsocket server in daemon mode");
      daemon_mode = true;
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

   int sock_fd = start_server_socket(daemon_mode);
   if (sock_fd < 0)
   {
      syslog(LOG_ERR, "Failed to start server socket");
      closelog();

      return -1;
   }

   int status = run_server_socket(sock_fd);
   if (status < 0)
   {
      syslog(LOG_ERR, "Error running server socket");
   }

   // Close socket, remove file, and close syslog
   syslog(LOG_INFO, "Shutting down server");
   close(sock_fd);
   remove(FILE_PATH);
   closelog();

   return 0;
}

