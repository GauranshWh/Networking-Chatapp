#include <iostream>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <string>
#include <thread>

void receive_messages(int client_socket) {
    char buffer[1024];
    while (true) {
        memset(buffer, 0, sizeof(buffer));
        int bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
        if (bytes_received <= 0) {
            std::cerr << "Server disconnected." << std::endl;
            break;
        }
        std::cout << buffer << std::endl;
    }
}

void send_message(int client_socket, const std::string& message) {
    if (send(client_socket, message.c_str(), message.size(), 0) == -1) {
        std::cerr << "Failed to send message." << std::endl;
    }
}

int main() {
    int client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        std::cerr << "Failed to create socket." << std::endl;
        return -1;
    }

    sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &server_address.sin_addr);

    if (connect(client_socket, (sockaddr*)&server_address, sizeof(server_address)) < 0) {
        std::cerr << "Connection failed." << std::endl;
        close(client_socket);
        return -1;
    }

    std::cout << "Connected to server!" << std::endl;

    // Get username
    std::string username;
    std::cout << "Enter your username: ";
    std::getline(std::cin, username);
    send_message(client_socket, username);

    // Create a separate thread to handle receiving messages
    std::thread receive_thread(receive_messages, client_socket);
    receive_thread.detach();

    // Main loop for sending messages
    while (true) {
        std::string message;
        std::cout << "> ";
        std::getline(std::cin, message);

        if (message == "/quit") {
            send_message(client_socket, message);
            break;
        }
        send_message(client_socket, message);
    }

    close(client_socket);
    return 0;
}

