#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <signal.h>
#define MSG_SIZE 250
#define PUERTO 2026

int main(int argc, char *argv[]) {
    int sd;
    struct sockaddr_in sockname;
    char buffer[MSG_SIZE];
    socklen_t len_sockname;
    fd_set readfds, auxfds;
    int salida;
    int fin = 0;

    // 1. Abrir el socket TCP
        
    sd = socket(AF_INET, SOCK_STREAM, 0);
    if (sd == -1) { perror("-Err. Error al abrir el socket cliente\n"); exit(1);}

    // 2. Configurar la direccion del servidor (IP y Puerto 2026)
  
    sockname.sin_family = AF_INET;
    sockname.sin_port = htons(PUERTO);
    char *ip = "127.0.0.1";
    if (argc > 1) {
        ip = argv[1];
    }
    sockname.sin_addr.s_addr = inet_addr(ip);

    // 3. Conectar al servidor
    
    len_sockname = sizeof(sockname);
    if (connect(sd, (struct sockaddr *)&sockname, len_sockname) == -1) { perror("-Err. Error de conexión con el servidor");   exit(1);}

    printf("+Ok. Conectado con éxito al servidor del juego Dominio Territorial.\n");

    printf("<<--------------------->>\n");
    printf(" CONECTADO AL JUEGO!!\n");
    printf("<<--------------------->>\n");


    // 4. Inicializar descriptores para multiplexacion (select)

    FD_ZERO(&readfds);
    FD_ZERO(&auxfds);

    // Entrada estándar (teclado)
    FD_SET(0, &readfds);  
    // Socket del servidor
    FD_SET(sd, &readfds); 

    // 5. Bucle de transmisión de mensajes
   
    do {
        auxfds = readfds;
        
        // Espera bloqueante a que haya actividad en el teclado o en el socket
        // sd +1 para que compruebe descriptores desde 0 al sd que corresponde 
        salida = select(sd + 1, &auxfds, NULL, NULL, NULL);

        if (salida > 0) {

            // Caso A: Mensaje recibido desde el servidor
            if (FD_ISSET(sd, &auxfds)) {
                bzero(buffer, sizeof(buffer));
                int bytes = recv(sd, buffer, sizeof(buffer) - 1, 0);

                if (bytes <= 0) {
                    printf("\nEl servidor ha cerrado la conexión.\n");
                    fin = 1;
                } else {
                    printf("%s", buffer);

                    // Mensajes especiales de desconexión por parte del servidor
                    if (strstr(buffer, "-Err. Desconexión servidor") != NULL || 
                        strstr(buffer, "-Err. Demasiados clientes") != NULL) {
                        fin = 1;

                    }
                }
            }

            // Caso B: Entrada por teclado enviada por el usuario
            if (FD_ISSET(0, &auxfds)) {
                bzero(buffer, sizeof(buffer));
                fgets(buffer, sizeof(buffer), stdin);

                // Eliminamos el salto de linea \n y \r al final de la cadena.
                // \r\n lo usa Windows al pulsar Enter
                buffer[strcspn(buffer, "\r\n")] = '\0';

                //Nos aseguramos que el usuario no ha pulsado el Enter en blanco y enviamos mensaje a servidor
                if(strlen(buffer) > 0){
                    send(sd, buffer, sizeof(buffer), 0);
                }
                
                // Si el comando introducido fue SALIR, cerramos tras enviar
                if (strncmp(buffer, "SALIR", 5) == 0) {
                    fin = 1;
                }
            }
        }

    } while (fin == 0);

    close(sd);
    printf("+Ok. Conexion finalizada.\n");
    return 0;
}