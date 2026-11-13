# wheel-

# Wheel Game

Multiplayer word game for 3 players, written in C using TCP sockets.

The server picks a secret word. Players take turns spinning a wheel
(bankrupt, lose turn, or points) and then guess a letter or the whole word.
The player with the most points wins.

## How to run

    gcc -Wall -o server server.c
    gcc -Wall -o client client.c

Start the server in one terminal:

    ./server

Then start three clients, each in its own terminal:

    ./client

## What I used

- C, TCP sockets (`socket`, `bind`, `listen`, `accept`, `connect`)
- A simple text protocol, one message per line

