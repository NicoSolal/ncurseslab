#include <stdio.h>
#include <pthread.h>
#include <ncurses.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <mqueue.h>
#include <unistd.h>
#include <errno.h>

#define QUEUE_PERMISSIONS 0666
#define MAX_MESSAGES 100
#define MAX_MESSAGE_LEN 256

typedef struct {
    char messages[MAX_MESSAGES][MAX_MESSAGE_LEN];
    int top;
    pthread_mutex_t lock;
} MessageStack;

typedef struct {
    mqd_t queue;
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

void* receiver_thread(void *param) {
    ThreadData *data = (ThreadData *)param;
    char buff[MAX_MESSAGE_LEN];
    unsigned int prio = 0;

    while (1) {
        memset(buff, 0, sizeof(buff));
        if (mq_receive(data->queue, buff, MAX_MESSAGE_LEN, &prio) != -1) {
            stack_push(data->recv_stack, buff);
            display_stack(data->recv_win, data->recv_stack);
        }
    }
    pthread_exit(0);
}

void* banner_thread(void *param) {
    (void)param;
    int i = 0;
    char cartel[] = "Sistemas Operativos ";
    char s1[100];
    size_t len = strlen(cartel);

    while (1) {
        memset(s1, 0, sizeof(s1));
        int idx = 0;
        for (size_t j = i; j < len; j++) s1[idx++] = cartel[j];
        for (size_t j = 0; j < i; j++) s1[idx++] = cartel[j];
        
        mvaddstr(0, 0, "                                        ");
        mvaddstr(0, 0, s1);
        
        wnoutrefresh(stdscr);
        doupdate();
        
        i = (i + 1) % (int)len;
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
    if (argc < 4) {
        fprintf(stderr, "uso: %s <nombre-cola> <cant-msgs> <tamaño-msgs>\n", argv[0]);
        exit(1);
    }

    mqd_t queue;
    struct mq_attr attr;
    int mqsize = atoi(argv[2]);
    int msgsize = atoi(argv[3]);

    fprintf(stderr, "DEBUG: Input parameters\n");
    fprintf(stderr, "  Queue name: '%s'\n", argv[1]);
    fprintf(stderr, "  mqsize (arg): %d\n", mqsize);
    fprintf(stderr, "  msgsize (arg): %d\n", msgsize);

    if (msgsize < MAX_MESSAGE_LEN) {
        fprintf(stderr, "DEBUG: Adjusting msgsize from %d to %d\n", msgsize, MAX_MESSAGE_LEN);
        msgsize = MAX_MESSAGE_LEN;
    }

    fprintf(stderr, "DEBUG: System limits\n");
    FILE *fp = fopen("/proc/sys/fs/mqueue/msgsize_max", "r");
    if (fp) {
        int max_msgsize;
        fscanf(fp, "%d", &max_msgsize);
        fclose(fp);
        fprintf(stderr, "  msgsize_max: %d\n", max_msgsize);
        if (msgsize > max_msgsize) {
            fprintf(stderr, "  ERROR: msgsize (%d) exceeds system limit (%d)\n", msgsize, max_msgsize);
            exit(1);
        }
    }

    fp = fopen("/proc/sys/fs/mqueue/msg_max", "r");
    if (fp) {
        int max_msgs;
        fscanf(fp, "%d", &max_msgs);
        fclose(fp);
        fprintf(stderr, "  msg_max: %d\n", max_msgs);
        if (mqsize > max_msgs) {
            fprintf(stderr, "  ERROR: mqsize (%d) exceeds system limit (%d)\n", mqsize, max_msgs);
            fprintf(stderr, "  Using system limit instead\n");
            mqsize = max_msgs;
        }
    }

    memset(&attr, 0, sizeof(struct mq_attr));
    attr.mq_flags = 0;
    attr.mq_maxmsg = mqsize;
    attr.mq_msgsize = msgsize;
    attr.mq_curmsgs = 0;

    fprintf(stderr, "DEBUG: Final queue attributes\n");
    fprintf(stderr, "  mq_maxmsg: %ld\n", attr.mq_maxmsg);
    fprintf(stderr, "  mq_msgsize: %ld\n", attr.mq_msgsize);
    fprintf(stderr, "  mq_flags: %ld\n", attr.mq_flags);

    fprintf(stderr, "DEBUG: Unlinking old queue\n");
    mq_unlink(argv[1]);
    usleep(100000);

    fprintf(stderr, "DEBUG: Creating queue\n");
    printf("DEBUG: Queue name: %s\n", argv[1]);
    printf("DEBUG: mq_maxmsg: %d\n", attr.mq_maxmsg);
    printf("DEBUG: mq_msgsize: %d\n", attr.mq_msgsize);
    printf("DEBUG: mq_flags: %ld\n", attr.mq_flags);

    queue = mq_open(argv[1], O_CREAT | O_RDWR, QUEUE_PERMISSIONS, &attr);
    if (queue == -1) {
        perror("Error creating message queue");
        fprintf(stderr, "Error: %s\n", strerror(errno));
        printf("DEBUG: errno = %d\n", errno);
        exit(1);
    }

    fprintf(stderr, "DEBUG: Queue created successfully (fd: %d)\n", (int)queue);

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

    ThreadData thread_data = {
        .queue = queue,
        .recv_win = recv_win,
        .send_win = send_win,
        .recv_stack = recv_stack,
        .send_stack = send_stack
    };

    pthread_t t_banner, t_clock, t_receiver;
    pthread_create(&t_banner, NULL, banner_thread, NULL);
    pthread_create(&t_clock, NULL, clock_thread, NULL);
    pthread_create(&t_receiver, NULL, receiver_thread, (void *)&thread_data);

    char input[MAX_MESSAGE_LEN] = "";
    
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
            
            if (mq_send(queue, input, strlen(input) + 1, 0) == -1) {
                perror("Error sending message to queue");
            }
        }
    }

    pthread_cancel(t_banner);
    pthread_cancel(t_clock);
    pthread_cancel(t_receiver);

    werase(recv_win);
    werase(send_win);
    delwin(recv_win);
    delwin(send_win);
    endwin();

    mq_close(queue);
    mq_unlink(argv[1]);

    free(recv_stack);
    free(send_stack);

    return 0;
}
