#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <ncurses.h>
#include <time.h>
#include <string.h>
#include <fcntl.h>
#include <mqueue.h>
#include <unistd.h>

#define BUFSIZE 1024
#define INPUTSIZE 1024

//Esta función mostrará el banner en un thread
void *banner(void *param);

//Esta función mostrará el reloj en un thread
void *reloj(void *param);

void *receive_mq(void *param);

typedef struct {
    WINDOW *top_win;
    mqd_t receiver;
    char *buff;
} ReceiverData;

/* Programa principal */
int main(int argc, char *argv[]) {  
    pthread_t t_reloj, t_banner;
    pthread_t t_receiver;
    WINDOW *top_win;
    WINDOW *bottom_win;
    char input[INPUTSIZE]="";
    char buff[BUFSIZE];

    if(argc != 3) {
        printf("Uso: <Receive> <Send>");
        return 1;
    }

    mqd_t receiver;
    if ((receiver = mq_open (argv[1],  O_RDWR)) == -1) { 
        perror("No se puede acceder a la cola de mensajes"); exit(1);
    }

    mqd_t sender;
    /* Abre la cola de mensajes */
    if ((sender = mq_open (argv[2],  O_RDWR )) == -1) { 
        perror("No se puede acceder a la cola de mensajes"); exit(1); 
    }

    //Prepara pantalla
    initscr(); // Inicializar Ncurses
    nodelay(stdscr, TRUE);
    keypad(stdscr, TRUE);

    clear(); // Limpiar la pantalla
    refresh();

    //Crea una ventana donde ingresar el texto
    top_win=newwin(15,66,1,1);
    bottom_win=newwin(15,66,1,68);

    mvwprintw(top_win, 1, 2, "Mensajes de Hilo %s: \n", argv[1]); 

    box(top_win,0,0);
    box(bottom_win,0,0);
    wrefresh(top_win);
    wrefresh(bottom_win);

    ReceiverData receiver_data = {
        .top_win = top_win,
        .receiver = receiver,
        .buff = buff
    };

    //Dispara los threads
    pthread_create(&t_banner,NULL,(void *)banner,NULL); //dispara banner
    pthread_create(&t_reloj,NULL,(void *)reloj,NULL); //dispara reloj
    pthread_create(&t_receiver, NULL, receive_mq, (void *)&receiver_data);

    static int row = 1;
    while (1) {
        wmove(bottom_win, row, 2);
        wrefresh(bottom_win);

        wgetstr(bottom_win, input);

        if (strcmp(input, "FIN") == 0) break;

        if (mq_send(sender, input, INPUTSIZE, 1) == -1) { 
            perror("Error al enviar el mensaje");
        }

        row++;
        if (row > 13) row = 1;
    }

    //Cancela (termina) los threads
    pthread_cancel(t_banner);
    pthread_cancel(t_reloj);
    pthread_cancel(t_receiver);

    werase(top_win);
    werase(bottom_win);
    endwin(); // Finalizar Ncurses y termina

    mq_close(receiver);

    return 0;  
}  

// Función que muestra el banner en la esquina superior izquierda
void *banner(void *param) {  
    (void)param;

    unsigned long i = 0;
    char cartel[] = "Sistemas Operativos ";
    char s1[100];

    // cada un segundo actualiza el banner
    while (1){
        strncpy(s1, &cartel[i], sizeof(cartel)); //desplaza texto 1 caracter

        mvaddstr(0, 0,s1 ); //muestra el banner
        refresh(); // refresca pantalla      

        i=(i+1)%(sizeof(cartel)-1); //incrementa posición
        usleep(500000); // espera medio segundo
    }  
    pthread_exit(0);   
}

// Función que muestra el reloj en la esquina superior derecha
void *reloj(void *param) {  
    (void)param;

    time_t rawtime;
 
    // cada un segundo actualiza el reloj
    while (1){
        time(&rawtime);

        mvaddstr(0, 50,ctime(&rawtime) ); //muestra la hora
        refresh(); // refresca pantalla

        sleep(1); // espera un segundo
    }  
    pthread_exit(0);   
}  

void *receive_mq(void *param) {
    ReceiverData *data = (ReceiverData *)param;
    unsigned int prio = 0;
    static int row = 2;

    while(1) {
        memset(data->buff, 0, BUFSIZE);

        if (mq_receive(data->receiver, data->buff, BUFSIZE, &prio) == -1) {
            perror("Error al recibir el mensaje"); exit(1);
        }

        wmove(data->top_win, row, 2);
        wprintw(data->top_win, "%s", data->buff);
        wrefresh(data->top_win);

        row++;
        if (row > 13) row = 2;
    }
    
    return NULL;
}