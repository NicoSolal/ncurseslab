#include <stdio.h>
#include <pthread.h>
#include <ncurses.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#define MAX_MESSAGES 100
#define MAX_MESSAGE_LEN 256

typedef struct {
    char messages[MAX_MESSAGES][MAX_MESSAGE_LEN];
    int top;
    pthread_mutex_t lock;
} MessageStack;

typedef struct {
    WINDOW *recv_win;
    WINDOW *send_win;
    MessageStack *recv_stack;
    MessageStack *send_stack;
} ThreadData;

MessageStack* stack_create(void) {
    MessageStack *stack = malloc(sizeof(MessageStack));
    stack->top = -1;
    pthread_mutex_init(&stack->lock, NULL);
    return stack;
}

void stack_push(MessageStack *stack, const char *msg) {
    pthread_mutex_lock(&stack->lock);
    if (stack->top < MAX_MESSAGES - 1) {
        stack->top++;
        strncpy(stack->messages[stack->top], msg, MAX_MESSAGE_LEN - 1);
        stack->messages[stack->top][MAX_MESSAGE_LEN - 1] = '\0';
    }
    pthread_mutex_unlock(&stack->lock);
}

int stack_pop(MessageStack *stack, char *msg) {
    pthread_mutex_lock(&stack->lock);
    if (stack->top >= 0) {
        strncpy(msg, stack->messages[stack->top], MAX_MESSAGE_LEN - 1);
        msg[MAX_MESSAGE_LEN - 1] = '\0';
        stack->top--;
        pthread_mutex_unlock(&stack->lock);
        return 1;
    }
    pthread_mutex_unlock(&stack->lock);
    return 0;
}

void display_stack(WINDOW *win, MessageStack *stack) {
    pthread_mutex_lock(&stack->lock);
    wclear(win);
    box(win, 0, 0);
    mvwprintw(win, 1, 2, "Messages:");
    
    int display_row = 3;
    for (int i = stack->top; i >= 0 && display_row < 10; i--) {
        mvwprintw(win, display_row, 2, "%.50s", stack->messages[i]);
        display_row++;
    }
    
    wrefresh(win);
    pthread_mutex_unlock(&stack->lock);
}

void* banner_thread(void *param) {
    (void)param;
    int i = 0;
    char cartel[] = "Sistemas Operativos ";
    char s1[100];
    int len = strlen(cartel);

    while (1) {
        memset(s1, 0, sizeof(s1));
        int idx = 0;
        for (int j = i; j < len; j++) s1[idx++] = cartel[j];
        for (int j = 0; j < i; j++) s1[idx++] = cartel[j];
        
        mvaddstr(0, 0, "                                        ");
        mvaddstr(0, 0, s1);
        
        wnoutrefresh(stdscr);
        doupdate();
        
        i = (i + 1) % len;
        usleep(300000);
    }
    pthread_exit(0);
}

void* clock_thread(void *param) {
    (void)param;
    time_t rawtime;
    struct tm *timeinfo;
    char timestr[50];

    while (1) {
        time(&rawtime);
        timeinfo = localtime(&rawtime);
        strftime(timestr, sizeof(timestr), "%H:%M:%S", timeinfo);

        mvaddstr(0, 60, "        ");
        mvaddstr(0, 60, timestr);
        
        wnoutrefresh(stdscr);
        doupdate();
        
        sleep(1);
    }
    pthread_exit(0);
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    initscr();
    cbreak();
    noecho();
    clear();
    refresh();

    WINDOW *recv_win = newwin(12, 80, 2, 0);
    WINDOW *send_win = newwin(12, 80, 15, 0);

    box(recv_win, 0, 0);
    mvwprintw(recv_win, 1, 2, "Received Messages Stack:");
    wrefresh(recv_win);

    box(send_win, 0, 0);
    mvwprintw(send_win, 1, 2, "Enter message (type 'FIN' to exit):");
    mvwprintw(send_win, 2, 2, "Input: ");
    wrefresh(send_win);

    MessageStack *recv_stack = stack_create();
    MessageStack *send_stack = stack_create();

    pthread_t t_banner, t_clock;
    pthread_create(&t_banner, NULL, banner_thread, NULL);
    pthread_create(&t_clock, NULL, clock_thread, NULL);

    /* char input[MAX_MESSAGE_LEN] = "";
    
    while (strcmp(input, "FIN") != 0) {
        wmove(send_win, 4, 2);
        wclrtoeol(send_win);
        mvwprintw(send_win, 4, 2, "Input: ");
        wrefresh(send_win);
        
        wmove(send_win, 4, 10);
        wgetstr(send_win, input);

        if (input[0] != '\0' && strcmp(input, "FIN") != 0) {
            stack_push(send_stack, input);
            display_stack(send_win, send_stack);
            
            stack_push(recv_stack, input);
            display_stack(recv_win, recv_stack);
        }
    } */

    pthread_cancel(t_banner);
    pthread_cancel(t_clock);

    werase(recv_win);
    werase(send_win);
    delwin(recv_win);
    delwin(send_win);
    endwin();

    free(recv_stack);
    free(send_stack);

    return 0;
}