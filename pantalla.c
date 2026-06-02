// Ejemplo de threads
// Dispara dos threads y queda leyendo texto del usuario
// El thread 1 mueve un banner en la esquina superior izquierda
// El thread 2 actualiza la hora en la esquina superior derecha
// El thread principal realiza lectura de teclado dentro de una ventana
//
// Autor: CB

#include <stdio.h>  
#include <pthread.h>  
#include <ncurses.h>
#include <time.h>
#include <string.h>


//Esta función mostrará el banner en un thread
void *banner(void *param);

//Esta función mostrará el reloj en un thread
void *reloj(void *param);


/* Programa principal */
int main(int argc, char *argv[]) {  
    pthread_t t_reloj, t_banner;
    WINDOW *win;
    char texto[200]="";

    //Prepara pantalla
    initscr(); // Inicializar Ncurses

    clear(); // Limpiar la pantalla
    refresh();

    //Crea una ventana donde ingresar el texto
    win=newwin(15,70,1,1);      
    box(win,0,0);
    mvwprintw(win, 1, 2, "Ingrese texto:"); 
    wrefresh(win); 

    //Dispara los threads
    pthread_create(&t_banner,NULL,(void *)banner,NULL); //dispara banner
    pthread_create(&t_reloj,NULL,(void *)reloj,NULL); //dispara reloj

    //lee texto en la ventana, hasta el FIN
    while (strcmp(texto,"FIN")!=0) {
       wgetstr(win, (char*)texto);
    }

    //Cancela (termina) los threads
    pthread_cancel(t_banner);
    pthread_cancel(t_reloj);

    werase(win);
    endwin(); // Finalizar Ncurses y termina
    return 0;  
}  

// Función que muestra el banner en la esquina superior izquierda
void *banner(void *param) {  
int i = 0;
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

    time_t rawtime;
 
    // cada un segundo actualiza el reloj
    while (1){
      time(&rawtime);
      mvaddstr(0, 50,ctime(&rawtime) ); //muestra la hora
      sleep(1); // espera un segundo
      refresh(); // refresca pantalla      
    }  
    pthread_exit(0);   
}  