#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const int wins[8][3] = {
    {0,1,2},{3,4,5},{6,7,8},{0,3,6},{1,4,7},{2,5,8},{0,4,8},{2,4,6}
};

static char winner(const char board[9]) {
    size_t i;
    for (i = 0; i < 8; ++i) {
        int a = wins[i][0], b = wins[i][1], c = wins[i][2];
        if (board[a] && board[a] == board[b] && board[a] == board[c]) return board[a];
    }
    return 0;
}

static void draw(const char board[9]) {
    int i;
    for (i = 0; i < 9; ++i) {
        printf(" %c %s", board[i] ? board[i] : (char)('1' + i), i % 3 == 2 ? "\n" : "|");
        if (i == 2 || i == 5) puts("-----------");
    }
}

int main(void) {
    char board[9] = {0}, line[64], player = 'X';
    int turns = 0;
    while (!winner(board) && turns < 9) {
        long position; char *end;
        draw(board); printf("Player %c, choose 1-9: ", player); fflush(stdout);
        if (!fgets(line, sizeof line, stdin)) { fputs("input ended\n", stderr); return 1; }
        position = strtol(line, &end, 10);
        while (*end == ' ' || *end == '\t') end++;
        if (position < 1 || position > 9 || (*end != '\n' && *end != '\0')) { puts("Invalid position."); continue; }
        if (board[position - 1]) { puts("That position is occupied."); continue; }
        board[position - 1] = player; turns++; player = player == 'X' ? 'O' : 'X';
    }
    draw(board);
    if (winner(board)) printf("Player %c wins!\n", winner(board)); else puts("It's a draw!");
    return 0;
}
