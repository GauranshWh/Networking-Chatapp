#include <iostream>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <thread>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <algorithm>

std::unordered_map<int, std::string> client_ids;  // Maps client socket to username
std::unordered_map<std::string, std::vector<int>> chat_rooms;  // Maps room name to connected clients
std::mutex clients_mutex;

void send_message(int socket, const std::string& message) {
    if (send(socket, message.c_str(), message.size(), 0) == -1) {
        std::cerr << "Failed to send message to client." << std::endl;
    }
}

void broadcast_message(const std::string& message, int sender_socket) {
    std::lock_guard<std::mutex> lock(clients_mutex);
    for (const auto& client : client_ids) {
        if (client.first != sender_socket) {
            send_message(client.first, message);
        }
    }
}

void send_private_message(const std::string& message, int sender_socket, const std::string& recipient_username) {
    std::lock_guard<std::mutex> lock(clients_mutex);
    bool user_found = false;
    for (const auto& client : client_ids) {
        if (client.second == recipient_username) {
            send_message(client.first, "Private from " + client_ids[sender_socket] + ": " + message);
            user_found = true;
            break;
        }
    }

    if (!user_found) {
        send_message(sender_socket, "User not found: " + recipient_username);
    }
}

void join_chat_room(const std::string& room_name, int client_socket) {
    std::lock_guard<std::mutex> lock(clients_mutex);
    chat_rooms[room_name].push_back(client_socket);
    send_message(client_socket, "You have joined the room: " + room_name);
}

void broadcast_to_room(const std::string& room_name, const std::string& message, int sender_socket) {
    std::lock_guard<std::mutex> lock(clients_mutex);
    for (int socket : chat_rooms[room_name]) {
        if (socket != sender_socket) {
            send_message(socket, message);
        }
    }
}

void handle_client(int client_socket) {
    char buffer[1024];
    std::string username;

    // Receive username
    if (recv(client_socket, buffer, sizeof(buffer), 0) <= 0) {
        std::cerr << "Failed to receive username." << std::endl;
        close(client_socket);
        return;
    }
    username = std::string(buffer);
    client_ids[client_socket] = username;

    // Send welcome message
    send_message(client_socket, "Welcome " + username + "!");

    // Listen for messages from the client
    while (true) {
        memset(buffer, 0, sizeof(buffer));
        int bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
        if (bytes_received <= 0) {
            std::cout << "Client disconnected." << std::endl;
            break;
        }

        std::string message(buffer);

        // Handle private messages
        if (message.substr(0, 8) == "/private") {
            size_t space_pos = message.find(' ', 9);
            std::string recipient_username = message.substr(9, space_pos - 9);
            std::string private_msg = message.substr(space_pos + 1);
            send_private_message(private_msg, client_socket, recipient_username);
        }
        // Handle room messages
        else if (message.substr(0, 5) == "/join") {
            std::string room_name = message.substr(6);
            join_chat_room(room_name, client_socket);
        }
        else if (message.substr(0, 5) == "/room") {
            size_t space_pos = message.find(' ', 6);
            std::string room_name = message.substr(6, space_pos - 6);
            std::string room_msg = message.substr(space_pos + 1);
            broadcast_to_room(room_name, username + ": " + room_msg, client_socket);
        }
        else if (message == "/quit") {
            break;
        } else {
            broadcast_message(username + ": " + message, client_socket);
        }
    }

    // Remove client and notify others
    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        client_ids.erase(client_socket);
    }

    close(client_socket);
}

int main() {
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        std::cerr << "Failed to create socket." << std::endl;
        return -1;
    }

    sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(8080);

    if (bind(server_socket, (sockaddr*)&server_address, sizeof(server_address)) < 0) {
        std::cerr << "Bind failed." << std::endl;
        close(server_socket);
        return -1;
    }

    if (listen(server_socket, 5) < 0) {
        std::cerr << "Listen failed." << std::endl;
        close(server_socket);
        return -1;
    }

    std::cout << "Server is listening on port 8080." << std::endl;

    while (true) {
        int client_socket = accept(server_socket, nullptr, nullptr);
        if (client_socket < 0) {
            std::cerr << "Failed to accept connection." << std::endl;
            continue;
        }

        std::cout << "Client connected!" << std::endl;

        std::thread client_thread(handle_client, client_socket);
        client_thread.detach();
    }

    close(server_socket);
    return 0;
}

