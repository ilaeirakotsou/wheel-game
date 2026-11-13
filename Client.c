#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 5555
#define BUF_SIZE 128

int main(int argc, char *argv[]) {
    const char *ip = (argc > 1) ? argv[1] : "127.0.0.1";
    struct sockaddr_in server;
    char line[BUF_SIZE], input[BUF_SIZE], out[BUF_SIZE + 16];

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }

    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    if (inet_pton(AF_INET, ip, &server.sin_addr) != 1) {
        fprintf(stderr, "Invalid IP address: %s\n", ip);
        return 1;
    }

    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("connect");
        return 1;
    }

    /* Διαβάζουμε από το socket γραμμή-γραμμή */
    FILE *in = fdopen(sock, "r");

    while (fgets(line, sizeof(line), in) != NULL) {
        printf("%s", line);

        if (strncmp(line, "YOUR_TURN", 9) == 0) {
            printf("Your turn! Press Enter to spin the wheel... ");
            fflush(stdout);
            if (fgets(input, sizeof(input), stdin) == NULL) break;
            send(sock, "SPIN\n", 5, 0);

        } else if (strncmp(line, "SPIN_RESULT POINTS", 18) == 0) {
            printf("Type a letter or the whole word: ");
            fflush(stdout);
            if (fgets(input, sizeof(input), stdin) == NULL) break;
            input[strcspn(input, "\r\n")] = '\0';

            if (strlen(input) == 1)
                snprintf(out, sizeof(out), "GUESS_LETTER %c\n", input[0]);
            else
                snprintf(out, sizeof(out), "GUESS_WORD %.63s\n", input);
            send(sock, out, strlen(out), 0);

        } else if (strncmp(line, "END", 3) == 0) {
            break;
        }
    }

    fclose(in); /* κλείνει και το socket */
    return 0;
}
