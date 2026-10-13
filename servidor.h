#ifndef SERVIDOR_H
#define SERVIDOR_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <stdlib.h>
#include <time.h>

#define PUERTO 2026
#define MSG_SIZE 250
#define MAX_CLIENTS 10
#define MAX_PARTIDAS 4
#define FILAS_TABLERO 5
#define COLUMS_TABLERO 5

enum EstadoCliente { 
    NO_VALIDADO,        
    ESPERANDO_PASS,     
    VALIDADO,           
    BUSCANDO_PARTIDA,   
    JUGANDO,            
    DESCONECTADO};

struct Cliente {
    int socket;                 // El descriptor del socket para enviarle/recibir mensajes
    char username[50];          
    enum EstadoCliente estado;  // El estado actual para controlar qué comandos puede usar
    int id_partida;  
    int turno;           // El índice de la partida en la que está  (-1 si no está jugando)
    };

struct Casilla {
    int propietario;            // 0 = Neutral, 1 = Jugador 1, 2 = Jugador 2
    int tropas;};

struct Partida {
    bool activa;                 
    int socket_j1;                    
    int socket_j2;                    
    int turno;                  // 1 = Turno del J1, 2 = Turno del J2
    struct Casilla tablero[FILAS_TABLERO][COLUMS_TABLERO];};

// Prototipos de funciones implementadas en funciones_servidor.c
void procesarMensaje(int socket_cliente, char *buffer, fd_set *readfds, struct Cliente clientes[], int *numClientes, struct Partida partidas[]);
void salirCliente(int socket, fd_set *readfds, struct Cliente clientes[], int *numClientes, struct Partida partidas[]);
bool traducir_coordenada(char *coord, int *fila, int *col);
void manejador(int signum, sig_atomic_t server_encendido);
void salirServer(char *buffer, fd_set *readfds, struct Cliente clientes[], int *numClientes);

#endif



