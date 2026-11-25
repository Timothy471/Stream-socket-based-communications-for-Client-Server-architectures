#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define IPV4_PORT 22031
#define IPV4_BUFFER_SIZE 2048
#define IPV6_PORT "80"
#define IPV6_BUFFER_SIZE 4096

// Integrated IPv6 client function triggered by command "01#"
int run_ipv6_client(int ipv4_client_fd)
{
    // STEP 1: Resolve the IPv6 address using getaddrinfo()
    const char *hostname = "www.gmail.com"; // www.bbc.co.uk does not work on ipv6
    struct addrinfo hints, *res;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET6;       // IPv6 only
    hints.ai_socktype = SOCK_STREAM;  // TCP stream socket

    int status = getaddrinfo(hostname, IPV6_PORT, &hints, &res);
    if (status != 0) {
        fprintf(stderr, "[IPv6] getaddrinfo error: %s\n", gai_strerror(status));
        return -1;
    }
    printf("[IPv6] Valid IPv6 address resolved for %s\n", hostname);

    // Create the IPv6 TCP socket using the resolved address info
    int sock6 = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock6 < 0) {
        perror("[IPv6] Socket creation error");
        freeaddrinfo(res);
        return -2;
    }

    // Connect to the resolved IPv6 HTTP server
    if (connect(sock6, res->ai_addr, res->ai_addrlen) == 0) {
        printf("[IPv6] Connection to %s has been successful!\n", hostname);
    } else {
        perror("[IPv6] Connection unsuccessful to website");
        close(sock6);
        freeaddrinfo(res);
        return -3;
    }

    // Free the memory allocated by getaddrinfo()
    // after a successful socket connection, the address structure is not needed anymore
    freeaddrinfo(res);

    // STEP 2: Send HTTP GET request
    // \r\n\r\n
    const char *request = "GET / HTTP/1.0\r\n\r\n";

    printf("[IPv6] Sending HTTP request...\n");
    if (send(sock6, request, strlen(request), 0) < 0) {
        perror("[IPv6] Error at send()");
        close(sock6);
        return -4;
    }

    // Build path for ~/index.html in the user's home directory
    const char *home_dir = getenv("HOME");
    if (home_dir == NULL) {
        fprintf(stderr, "[IPv6] Error: HOME environment variable not found\n");
        close(sock6);
        return -5;
    }

    char filepath[512];
    int path_status = snprintf(filepath, sizeof(filepath), "%s/index.html", home_dir);
    if (path_status < 0 || (size_t)path_status >= sizeof(filepath)) {
        fprintf(stderr, "[IPv6] Error formatting file path\n");
        close(sock6);
        return -6;
    }

    // Open ~/index.html for saving the web page
    FILE *fp = fopen(filepath, "w");
    if (fp == NULL) {
        perror("[IPv6] Error opening ~/index.html");
        close(sock6);
        return -7;
    }

    // Receive data from IPv6 server, save to ~/index.html, and forward to IPv4 client
    char buffer[IPV6_BUFFER_SIZE];
    ssize_t bytes_received;
    int transfer_err = 0;

    printf("[IPv6] Waiting for server response...\n");

    while ((bytes_received = recv(sock6, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytes_received] = '\0';

        // 1. Save chunk to ~/index.html
        if (fwrite(buffer, 1, (size_t)bytes_received, fp) < (size_t)bytes_received) {
            perror("[IPv6] Error writing to ~/index.html");
            transfer_err = -8;
            break;
        }

        // 2. Forward chunk to ClientIPv4_win_2026.exe
        if (send(ipv4_client_fd, buffer, (size_t)bytes_received, 0) < 0) {
            perror("[IPv4] Error forwarding data to Windows client");
            transfer_err = -9;
            break;
        }
    }

    if (bytes_received < 0) {
        perror("[IPv6] Error at recv()");
        transfer_err = -10;
    }

    // Close output file and check for errors
    if (fclose(fp) != 0) {
        perror("[IPv6] Error closing ~/index.html");
        if (transfer_err == 0) transfer_err = -11;
    }

    // Close the IPv6 socket to release system resources
    if (close(sock6) < 0) {
        perror("[IPv6] Error at close()");
        if (transfer_err == 0) transfer_err = -12;
    }

    if (transfer_err == 0) {
        printf("[IPv6] Transfer finished. Saved to %s and forwarded to IPv4 client.\n", filepath);
    }

    return transfer_err;
}

int main()
{
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    char buffer[IPV4_BUFFER_SIZE];
    int opt = 1;

    // 1. SOCKET(): Create an IPv4 TCP socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("[IPv4] Error at socket()");
        return 1;
    }
    printf("[1] IPv4 socket created successfully.\n");

    // Allow immediate reuse of the port after server restart
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("[IPv4] Error at setsockopt()");
        if (close(server_fd) < 0) {
            perror("[IPv4] Error at close(server_fd)");
        }
        return 2;
    }

    // Configure the server IPv4 address structure (all assigned interfaces)
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(IPV4_PORT);

    // 2. BIND(): Bind the socket to the specified IP and port
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("[IPv4] Error at bind()");
        if (close(server_fd) < 0) {
            perror("[IPv4] Error at close(server_fd)");
        }
        return 3;
    }
    printf("[2] Bind successful on port %d.\n", IPV4_PORT);

    // 3. LISTEN(): Set the socket to passive listening mode
    if (listen(server_fd, 5) < 0) {
        perror("[IPv4] Error at listen()");
        if (close(server_fd) < 0) {
            perror("[IPv4] Error at close(server_fd)");
        }
        return 4;
    }
    printf("[3] Server listening for ClientIPv4_win_2026.exe...\n");

    // Outer loop: continuously accept new IPv4 client connections
    while (1) {
        // 4. ACCEPT(): Wait for and accept an incoming client connection
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            perror("[IPv4] Error at accept()");
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
        if (inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN) == NULL) {
            perror("[IPv4] Error at inet_ntop()");
            strcpy(client_ip, "Unknown");
        }
        printf("\n[4] Client connected from %s:%d\n", client_ip, ntohs(client_addr.sin_port));

        // Inner loop: keep the connection alive while the client sends commands
        while (1) {
            // 5. RECV(): Receive command from the connected client
            memset(buffer, 0, IPV4_BUFFER_SIZE);
            ssize_t bytes_received = recv(client_fd, buffer, IPV4_BUFFER_SIZE - 1, 0);

            if (bytes_received < 0) {
                perror("[IPv4] Error at recv()");
                break;
            } else if (bytes_received == 0) {
                printf("[IPv4] Client closed the connection.\n");
                break;
            }

            // Null-terminate and strip trailing \r or \n
            buffer[bytes_received] = '\0';
            buffer[strcspn(buffer, "\r\n")] = '\0';
            printf("[5] Received command: '%s'\n", buffer);

            // Process command "01#"
            if (strcmp(buffer, "01#") == 0) {
                printf("[IPv4] Command 01# accepted. Starting IPv6 client...\n");
                int ipv6_res = run_ipv6_client(client_fd);

                if (ipv6_res < 0) {
                    const char *err_msg = "Error executing IPv6 request\n";
                    if (send(client_fd, err_msg, strlen(err_msg), 0) < 0) {
                        perror("[IPv4] Error at send()");
                    }
                    break;
                }
            } else {
                // Send exact required error message for unrecognized commands
                const char *not_impl = "Command not implemented\n";
                ssize_t bytes_sent = send(client_fd, not_impl, strlen(not_impl), 0);
                if (bytes_sent < 0) {
                    perror("[IPv4] Error at send()");
                    break;
                }
                printf("[6] Response sent (%zd bytes): %s", bytes_sent, not_impl);
            }
        }

        // 7. CLOSE(): Close the current client session
        if (close(client_fd) < 0) {
            perror("[IPv4] Error at close(client_fd)");
        }
        printf("[7] Client session closed.\n");
    }

    if (close(server_fd) < 0) {
        perror("[IPv4] Error at close(server_fd)");
        return 5;
    }

    return 0;
}

