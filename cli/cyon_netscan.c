#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#define CYAN "\033[0;36m"
#define RED  "\033[0;31m"
#define GREEN "\033[0;32m"
#define NC   "\033[0m"

int host_reachable(const char *ip) {
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "ping -c 1 -W 1 %s > /dev/null 2>&1", ip);
    return system(cmd) == 0;
}

void scan_port(const char *ip, int port) {
    int sock;
    struct sockaddr_in target;
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return;
    target.sin_family = AF_INET;
    target.sin_port = htons(port);
    inet_pton(AF_INET, ip, &target.sin_addr);
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    if (connect(sock, (struct sockaddr *)&target, sizeof(target)) == 0) {
        printf("%s  [OPEN]%s  Port %d\n", GREEN, NC, port);
    } else {
        printf("%s  [CLOSED]%s Port %d\n", RED, NC, port);
    }
    close(sock);
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        printf("%s Usage: cyon_netscan <ip> <start_port> <end_port>%s\n", CYAN, NC);
        printf(" Example: cyon_netscan 192.168.1.1 1 1024\n");
        return 1;
    }
    const char *ip = argv[1];
    int start = atoi(argv[2]);
    int end   = atoi(argv[3]);

    printf("%s\n 🌀 Cyon NetScan%s\n", CYAN, NC);
    printf(" Checking host %s...\n", ip);

    if (!host_reachable(ip)) {
        printf("%s Host %s is unreachable. Aborting.%s\n", RED, ip, NC);
        return 1;
    }

    printf("%s Host is up.%s Scanning ports %d-%d\n\n", GREEN, NC, start, end);

    for (int port = start; port <= end; port++) {
        scan_port(ip, port);
    }

    printf("\n%s Scan complete.%s\n", CYAN, NC);
    return 0;
}