#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 5555
#define MAX_CLIENTS 3
#define BUF_SIZE 128

static const char *words[] = {"hello", "read", "write", "export", "file", "document"};
#define WORD_COUNT (sizeof(words) / sizeof(words[0]))

/* Στέλνει ένα μήνυμα που τελειώνει πάντα με \n */
static void send_line(int fd, const char *fmt, ...) {
    char buf[BUF_SIZE];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf) - 1, fmt, ap);
    va_end(ap);
    strcat(buf, "\n");
    send(fd, buf, strlen(buf), 0);
}

/* Διαβάζει μία ολόκληρη γραμμή. Επιστρέφει -1 αν έκλεισε η σύνδεση. */
static int read_line(FILE *in, char *buf, size_t size) {
    if (fgets(buf, size, in) == NULL)
        return -1;
    buf[strcspn(buf, "\r\n")] = '\0';
    return 0;
}

int main(void) {
    int server_fd, client_fd[MAX_CLIENTS];
    FILE *client_in[MAX_CLIENTS];
    struct sockaddr_in addr;
    char buffer[BUF_SIZE];

    srand(time(NULL));
    signal(SIGPIPE, SIG_IGN); /* να μην πεθαίνει ο server αν φύγει ένας client */

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        return 1;
    }
    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("listen");
        return 1;
    }

    printf("Server listening on port %d...\n", PORT);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        client_fd[i] = accept(server_fd, NULL, NULL);
        if (client_fd[i] < 0) {
            perror("accept");
            return 1;
        }
        client_in[i] = fdopen(client_fd[i], "r"); /* για ανάγνωση γραμμή-γραμμή */
        send_line(client_fd[i], "WELCOME P%d", i);
        printf("Client P%d connected\n", i);
    }

    /* Επιλογή λέξης */
    const char *secret = words[rand() % WORD_COUNT];
    int len = strlen(secret);
    char hidden[len + 1];
    memset(hidden, '_', len);
    hidden[len] = '\0';

    int scores[MAX_CLIENTS] = {0};
    int turn = 0;
    int game_over = 0;
    int aborted = 0;

    while (!game_over) {
        for (int i = 0; i < MAX_CLIENTS; i++) {
            send_line(client_fd[i], "STATE %s", hidden);
            send_line(client_fd[i], "SCORES %d %d %d", scores[0], scores[1], scores[2]);
        }
        send_line(client_fd[turn], "YOUR_TURN");

        if (read_line(client_in[turn], buffer, sizeof(buffer)) < 0) {
            aborted = 1;
            break;
        }

        if (strcmp(buffer, "SPIN") != 0) {
            send_line(client_fd[turn], "ERROR expected SPIN");
            continue; /* ξαναρωτάμε τον ίδιο παίκτη */
        }

        int r = rand() % 5;

        if (r == 0) {
            scores[turn] = 0;
            send_line(client_fd[turn], "SPIN_RESULT BANKRUPT");
            turn = (turn + 1) % MAX_CLIENTS;
            continue;
        }
        if (r == 1) {
            send_line(client_fd[turn], "SPIN_RESULT LOSE_TURN");
            turn = (turn + 1) % MAX_CLIENTS;
            continue;
        }

        int points = (r + 1) * 100;
        send_line(client_fd[turn], "SPIN_RESULT POINTS %d", points);

        if (read_line(client_in[turn], buffer, sizeof(buffer)) < 0) {
            aborted = 1;
            break;
        }

        char c;
        char guess[BUF_SIZE];

        if (sscanf(buffer, "GUESS_LETTER %c", &c) == 1) {
            c = tolower((unsigned char)c);
            int count = 0;

            if (isalpha((unsigned char)c)) {
                for (int i = 0; i < len; i++) {
                    if (secret[i] == c && hidden[i] == '_') {
                        hidden[i] = c;
                        count++;
                    }
                }
            }

            if (count > 0) {
                scores[turn] += points * count;
                send_line(client_fd[turn], "LETTER_RESULT OK");
                if (strcmp(secret, hidden) == 0)
                    game_over = 1;
            } else {
                send_line(client_fd[turn], "LETTER_RESULT FAIL");
                turn = (turn + 1) % MAX_CLIENTS;
            }

        } else if (sscanf(buffer, "GUESS_WORD %63s", guess) == 1) {
            if (strcmp(guess, secret) == 0) {
                /* πόντοι για κάθε γράμμα που ήταν ακόμα κρυμμένο */
                int hidden_left = 0;
                for (int i = 0; i < len; i++)
                    if (hidden[i] == '_') hidden_left++;
                scores[turn] += points * hidden_left;

                strcpy(hidden, secret);
                send_line(client_fd[turn], "WORD_RESULT OK");
                game_over = 1;
            } else {
                send_line(client_fd[turn], "WORD_RESULT FAIL");
                turn = (turn + 1) % MAX_CLIENTS;
            }

        } else {
            send_line(client_fd[turn], "ERROR invalid guess");
            turn = (turn + 1) % MAX_CLIENTS;
        }
    }

    /* Νικητής = ο παίκτης με τους περισσότερους πόντους */
    int winner = 0;
    for (int i = 1; i < MAX_CLIENTS; i++)
        if (scores[i] > scores[winner]) winner = i;

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (aborted)
            send_line(client_fd[i], "END ABORTED WORD %s", secret);
        else
            send_line(client_fd[i], "END WINNER P%d SCORE %d WORD %s",
                      winner, scores[winner], secret);
        fclose(client_in[i]); /* κλείνει και το socket */
    }

    close(server_fd);
    return 0;  

  
}
