#include "utils.h"

#include <unistd.h>

extern int errno;

// Sends a UDP packet to the specified destination
void udp_client_svc::send(string host, string port, char *msg, unsigned length) {
    udp_client_info conn = udp_client(host, port);
    if (conn.client_socket == -1) return;

    // Send the message and close the connection
    sendto(conn.client_socket, msg, length, 0, &conn.addr, sizeof(conn.addr));
    close(conn.client_socket);
}

/*
 * Creates a UDP connection to a given host and port.
 * Returns a socket fd to the host, otherwise crashes on failure.
 */
udp_client_info udp_client_svc::udp_client(string host, string port) {
    int client_socket;
    udp_client_info ret;

    struct addrinfo info, *res;

    memset(&info, 0, sizeof(info));

    info.ai_family = AF_INET;
    info.ai_socktype = SOCK_DGRAM;
    info.ai_protocol = IPPROTO_UDP;

    int s = getaddrinfo(host.c_str(), port.c_str(), &info, &res);
    if (s != 0) {
        // get the error using gai
        std::cerr << "gai failed " << gai_strerror(s) << std::endl;
        ret.client_socket = -1;
        return ret;
    }

    // get a socket for the client
    client_socket = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (client_socket == -1) {
        perror("socket failed");
        exit(1);
    }

    ret.client_socket = client_socket;
    ret.addr_len = res->ai_addrlen;
    ret.addr = *(res->ai_addr);
    return ret;
}

// Starts the server on the given port
void udp_server_svc::start_server(int port) {
    server_fd = udp_server(port);
}

// Stops the server
void udp_server_svc::stop_server() {
    close(server_fd);
}

// Wrapper function around recvfrom that handles errors
int udp_server_svc::recv(char *buf, unsigned length) {
    struct sockaddr_in client_sa;
    socklen_t client_len = sizeof(client_sa);

    int msg_size = recvfrom(server_fd, buf, length, 0, (struct sockaddr*)&client_sa, &client_len);
    if (msg_size < 0) {
        lg->log_v("Unexpected error in receiving UDP packet, errno " + errno);
    }

    return msg_size;
}

int udp_server_svc::udp_server(int port) {
    int server_fd;
    struct sockaddr_in server_sa;

    memset(&server_sa, 0, sizeof(server_sa));

    if ((server_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket failed");
        exit(1);
    }

    server_sa.sin_family = AF_INET;
    server_sa.sin_addr.s_addr = htonl(INADDR_ANY);
    server_sa.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *) &server_sa, sizeof(server_sa)) < 0) {
        perror("bind failed");
    }

    return server_fd;
}

/*

    struct addrinfo info, *res;
    memset(&info, 0, sizeof(info));

    info.ai_family = AF_INET;
    info.ai_socktype = SOCK_STREAM;

    int s = getaddrinfo(host.c_str(), port.c_str(), &info, &res);
*/
