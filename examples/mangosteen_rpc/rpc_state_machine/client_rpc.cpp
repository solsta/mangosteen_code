#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 6379
#define SERVER_IP "127.0.0.1"

int main() {
	sleep(3);
    int sock = 0, valread;
    struct sockaddr_in serv_addr;
    const char *request = "0First request\r\n";
    char buffer[1024] = {0};

    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        exit(EXIT_FAILURE);
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // Convert IPv4 and IPv6 addresses from text to binary form
    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        exit(EXIT_FAILURE);
    }

    // Connect to server
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }

    // Send request
    send(sock, request, strlen(request), 0);
    printf("Request sent\n");

    // Read response
    valread = read(sock, buffer, 1024);
    printf("Response:\n%s\n", buffer);

    // Close socket
    close(sock);
    return 0;
}
