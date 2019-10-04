#include "utils.h"

extern int errno;

udp_client_info udp_client(string host, string port) {
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
        exit(1);
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

int udp_server(int port) {
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
