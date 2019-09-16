#include "client.h"

int main(int argc, char *argv[]) {
    if (argc != 4) {
        cerr << argv[0] << " <ip address> <port> <search string>" << endl;
        exit(1);
    }

    string result = query_machine(argv[1], argv[2], argv[3]);
    cout << result << endl;
    return 0;
}

string query_machine(string host, string port, string query_string) {
    struct addrinfo info, *res;
    memset(&info, 0, sizeof(info));

    info.ai_family = AF_INET;
    info.ai_socktype = SOCK_STREAM;

    int s = getaddrinfo(host.c_str(), port.c_str(), &info, &res);
    if (s != 0) {
        // get the error using gai
        cerr << "gai failed " << gai_strerror(s) << endl;
        exit(1);
    }

    // get a socket for the client
    int client_socket = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (client_socket == -1) {
        perror("socket failed");
        exit(1);
    }

    // connect the client to the server
    int connected = connect(client_socket, res->ai_addr, res->ai_addrlen);
    if (connected == -1) {
        perror("connect failed");
        exit(1);
    }

    ssize_t nw = write_message_size(query_string.length(), client_socket);
    ssize_t message_nw = write_all_to_socket(client_socket, query_string.c_str(), query_string.length());

    ssize_t message_size = get_message_size(client_socket);

    char buf[message_size + 1]; buf[message_size] = '\0';
    read_all_from_socket(client_socket, buf, message_size);

    string ret(buf);

    // close client socket
    close(client_socket);
    return ret;
}

string read_response(int socket, size_t num_bytes) {
    string ret;
    // @TODO
    return ret;
}
