#include "server.h"

#define MAX_CLIENTS 4

int main(int argc, char *argv[]) {
    if (argc != 2) {
        cerr << argv[0] << " <port>" << endl;
        exit(1);
    }

    int server_fd = setup_server(argv[1]);
    run_server(server_fd);
    return 0;
}

string run_grep_command(string search_text) {
    array<char, 128> buffer;
    string result = "";
    string cmd = "grep -c " + search_text + " " + log_file;

    unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);

    if (!pipe) {
        throw std::runtime_error("popen failed");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get())) {
        result += buffer.data();
    }

    if (!result.empty() && result.back()  == '\n') {
        result.pop_back();
    }

    return result;
}

int setup_server(string port) {
    struct addrinfo info, *res;
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    memset(&info, 0, sizeof(info));

    info.ai_family = AF_INET;
    info.ai_socktype = SOCK_STREAM;
    info.ai_flags = AI_PASSIVE;

    // getaddrinfo for the server port
    int s = getaddrinfo(NULL, port.c_str(), &info, &res);
    if (s != 0) {
        // get the error using gai
        cerr << "gai failed " << gai_strerror(s) << endl;
        free(res);
        exit(1);
    }

    // bind server socket to port and address
    if (bind(server_fd, res->ai_addr, res->ai_addrlen) != 0) {
        perror("bind failed");
        free(res);
        exit(1);
    }
    free(res);

    // set up listening for clients
    if (listen(server_fd, MAX_CLIENTS) != 0) {
        perror("listen failed");
        exit(1);
    }

    return server_fd;
}

void run_server(int server_fd) {
    int client_fd;
    while ((client_fd = accept(server_fd, NULL, NULL))) {
        if (client_fd < 0) {
            perror("accept failed");
            exit(1);
        }

        ssize_t query_length = get_message_size(client_fd);

        char buf[query_length + 1]; buf[query_length] = '\0';
        read_all_from_socket(client_fd, buf, query_length);

        string query_string(buf);
        string query_result = run_grep_command(query_string);

        write_message_size(query_result.length(), client_fd);
        write_all_to_socket(client_fd, query_result.c_str(), query_result.length());

        // close the client fd
        close(client_fd);
    }
}
