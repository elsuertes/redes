#include "servidor.h"

void salirCliente(int socket, fd_set *readfds, struct Cliente clientes[], int *numClientes, struct Partida partidas[]){
int i = -1;
    for (int j = 0; j < *numClientes; j++){
        if (clientes[j].socket == socket){
            i = j;
            break;
        }
    }
    if (i == -1) return;

    // 2. Si estaba jugando, avisar al rival y desactivar la partida
    int id_partida = clientes[i].id_partida;
    if (id_partida != -1 && partidas[id_partida].activa){
        int socket_rival = (partidas[id_partida].socket_j1 == socket) ? partidas[id_partida].socket_j2 : partidas[id_partida].socket_j1;
        char msg[] = "+Ok. Tu oponente ha terminado la partida\n";
        send(socket_rival, msg, strlen(msg), 0);
        partidas[id_partida].activa = false;
    }
    close(socket);
    FD_CLR(socket, readfds);
    for (int j = i; j < *numClientes - 1; j++){
        clientes[j] = clientes[j + 1];
    }
    
    (*numClientes)--;
}

void manejador (int signum, sig_atomic_t server_encendido){
    printf("\n-Ok. Se ha recibido la señal sigint (ctrl +c)\n");
    signal(SIGINT,manejador);
    server_encendido = 0;
}
void salirServer(char *buffer, fd_set *readfds, struct Cliente clientes[], int *numClientes){
    for (int j = 0; j < *numClientes; j++){
        memset(buffer, 0, sizeof(buffer));
        strcpy(buffer,"-Err. Desconexión servidor\n"); 
        send(clientes[j].socket, buffer , strlen(buffer), 0); 
        close(clientes[j].socket); 
        FD_CLR(clientes[j].socket, readfds); 
    }
    *numClientes = 0;
}



bool traducir_coordenada(char *coord, int *fila, int *col){
    if (strlen(coord) != 2) return false;
    *fila = coord[0] - 'A'; // 'A' -> 0, 'B' -> 1, etc.
    *col = coord[1] - '1';  // '1' -> 0, '2' -> 1, etc.
    if (*fila < 0 || *fila >= 5 || *col < 0 || *col >= 5) return false;
    return true;
}

void iniciar_partida(){
    int j1 = 0,j2 = 0, cont =0;
    for(cliente){
        if(cliente[].estado == BUSCANDO_PARTIDA){
            if(cont>2)  break;
            cont++;
            j1 += i;
        }
    }

}

void estado(int socket, int id_partida, struct Partida partidas[]){
    struct Partida *p = &partidas[id_partida];
    int mi_id = (p->socket_j1 == socket) ? 1 : 2;
    char buffer[2048];
    char temp[100];
    memset(buffer, 0, sizeof(buffer));
    strcpy(buffer, "+Ok. Estado del tablero:\n    1     2     3     4     5\n +-----+-----+-----+-----+-----+\n");
    for (int i = 0; i < 5; i++) {
        char letra_fila = 'A' + i;
        sprintf(temp, " %c |", letra_fila); //* añade en temp una cadena tipo: " A | "
        strcat(buffer, temp); 
        for (int j = 0; j < 5; j++) {
            int prop = p->tablero[i][j].propietario;
            int tropas = p->tablero[i][j].tropas;
            if (prop == 0)  strcat(buffer, "     |"); 
            else if (prop == mi_id) {
                sprintf(temp, " J%d(%-2d|", prop, tropas);  //* j%d(%-2d) -> J1(20) para que siempre tenga el mismo formato
                strcat(buffer, temp);
            }else {
                sprintf(temp, "  J%d  |", prop); 
                strcat(buffer, temp);
            }
        }
        strcat(buffer, "\n +-----+-----+-----+-----+-----+\n");
    }
    send(socket, buffer, strlen(buffer), 0);
}

void conquistar(int socket, int id_partida, char *origen_str, char *destino_str, int unidades_enviadas, struct Partida partidas[]){
    struct Partida *p = &partidas[id_partida];
    int mi_id = (p->socket_j1 == socket) ? 1 : 2;
    int f_orig, c_orig, f_dest, c_dest;
    char msg[150];


    //* Variables de control de errores
    int id_rival = (mi_id == 1) ? 2 : 1;
    int socket_rival = (mi_id == 1) ? p->socket_j2 : p->socket_j1;
    int diff_f = abs(f_orig - f_dest);
    int diff_c = abs(c_orig - c_dest);

    //* 1. Validar turno
    if (p->turno != mi_id) {
        send(socket, "-Err. No es tu turno\n", 22, 0);
        return;}

    //* 2. Validar coordenadas, posesion y tropas y territorio 
    if (!traducir_coordenada(origen_str, &f_orig, &c_orig) || !traducir_coordenada(destino_str, &f_dest, &c_dest)) {
        send(socket, "-Err. Coordenadas inválidas\n", 28, 0);
        return;}
    if (p->tablero[f_orig][c_orig].propietario != mi_id || p->tablero[f_orig][c_orig].tropas <= unidades_enviadas) {
        send(socket, "-Err. Tropas insuficientes o territorio no propio\n", 51, 0);
        return;}
    if (p->tablero[f_dest][c_dest].propietario != 0) {
        send(socket, "-Err. El territorio destino no es neutral\n", 43, 0);
        return;}
    if (!((diff_f == 1 && diff_c == 0) || (diff_f == 0 && diff_c == 1))) {
        send(socket, "-Err. Los territorios no son adyacentes\n", 41, 0);
        return;
    }

    //* Ejecutar conquista
    p->tablero[f_orig][c_orig].tropas -= unidades_enviadas;
    p->tablero[f_dest][c_dest].propietario = mi_id;
    p->tablero[f_dest][c_dest].tropas = unidades_enviadas;

    //* Alternar turno
    p->turno = (p->turno == 1) ? 2 : 1;

    sprintf(msg, "+Ok. Territorio %s conquistado.\n", destino_str);
    send(socket, msg, strlen(msg), 0);

    //* Notificar al rival
    char msg_rival[] = "+Ok. El oponente ha movido. Es tu turno.\n";
    send(socket_rival, msg_rival, strlen(msg_rival), 0);
}

void atacar(int socket, int id_partida, char *origen_str, char *destino_str, int unidades_enviadas, struct Partida partidas[]){
    struct Partida *p = &partidas[id_partida];
    int mi_id = (p->socket_j1 == socket) ? 1 : 2;
    int f_orig, c_orig, f_dest, c_dest;
    int tropas_defensoras = p->tablero[f_dest][c_dest].tropas;
    char msg[150];


    //* Variables de control de errores
    int id_rival = (mi_id == 1) ? 2 : 1;
    int socket_rival = (mi_id == 1) ? p->socket_j2 : p->socket_j1;
    int diff_f = abs(f_orig - f_dest);
    int diff_c = abs(c_orig - c_dest);

    //* 1. Validar turno
    if (p->turno != mi_id) {
        send(socket, "-Err. No es tu turno\n", 22, 0);
        return;}

    //* 2. Validar coordenadas, posesion y tropas y territorio 
    if (!traducir_coordenada(origen_str, &f_orig, &c_orig) || !traducir_coordenada(destino_str, &f_dest, &c_dest)) {
        send(socket, "-Err. Coordenadas inválidas\n", 28, 0);
        return;}
    if (p->tablero[f_orig][c_orig].propietario != mi_id || p->tablero[f_orig][c_orig].tropas <= unidades_enviadas) {
        send(socket, "-Err. Tropas insuficientes o territorio no propio\n", 51, 0);
        return;}
    if (p->tablero[f_dest][c_dest].propietario != id_rival) {
        send(socket, "-Err. El territorio destino no es de tu rival (¿quieres atacar?))\n", 45, 0);
        return;}
    if (!((diff_f == 1 && diff_c == 0) || (diff_f == 0 && diff_c == 1))) {
        send(socket, "-Err. Los territorios no son adyacentes\n", 41, 0);
        return;
    }
    
    p->tablero[f_orig][c_orig].tropas -= unidades_enviadas;

    if (unidades_enviadas > tropas_defensoras) {
        // ATAQUE EXITOSO: Conquistas el territorio
        p->tablero[f_dest][c_dest].propietario = mi_id;
        p->tablero[f_dest][c_dest].tropas = unidades_enviadas; 
        sprintf(msg, "+Ok. Has conquistado el territorio %s.\n", destino_str);
    } 
    else {
        // ATAQUE FRACASADO: Pierdes las tropas enviadas, el defensor se queda como estaba
        sprintf(msg, "+Ok. Has perdido el ataque contra %s.\n", destino_str);
    }

    p->turno = (p->turno == 1) ? 2 : 1;
    send(socket, msg, strlen(msg), 0);
}

void reforzar(int socket, int id_partida, char *origen_str, char *destino_str, int unidades_enviadas, struct Partida partidas[]){
    struct Partida *p = &partidas[id_partida];
    int mi_id = (p->socket_j1 == socket) ? 1 : 2;
    int f_orig, c_orig, f_dest, c_dest;
    int tropas_defensoras = p->tablero[f_dest][c_dest].tropas;
    char msg[150];

    //* Variables de control de errores
    int socket_rival = (mi_id == 1) ? p->socket_j2 : p->socket_j1;
    int diff_f = abs(f_orig - f_dest);
    int diff_c = abs(c_orig - c_dest);

    //* 1. Validar turno
    if (p->turno != mi_id) {
        send(socket, "-Err. No es tu turno\n", 22, 0);
        return;}

    //* 2. Validar coordenadas, posesion y tropas y territorio 
    if (!traducir_coordenada(origen_str, &f_orig, &c_orig) || !traducir_coordenada(destino_str, &f_dest, &c_dest)) {
        send(socket, "-Err. Coordenadas inválidas\n", 28, 0);
        return;}
    if (p->tablero[f_orig][c_orig].propietario != mi_id || p->tablero[f_orig][c_orig].tropas <= unidades_enviadas) {
        send(socket, "-Err. Tropas insuficientes o territorio no propio\n", 51, 0);
        return;}
    if (p->tablero[f_dest][c_dest].propietario != mi_id) {
        send(socket, "-Err. El territorio destino no es de tu rival (¿quieres atacar?))\n", 45, 0);
        return;}
    if (!((diff_f == 1 && diff_c == 0) || (diff_f == 0 && diff_c == 1))) {
        send(socket, "-Err. Los territorios no son adyacentes\n", 41, 0);
        return;
    }
    
    p->tablero[f_orig][c_orig].tropas -= unidades_enviadas;
    p->tablero[f_dest][c_dest].tropas += unidades_enviadas;
    sprintf(msg, "+Ok. Has enviado %d tropas al territorio %s.\n",unidades_enviadas, destino_str);
    p->turno = (p->turno == 1) ? 2 : 1;
    send(socket, msg, strlen(msg), 0);
}

void pasar(int socket, int id_partida, struct Partida partidas[]){
    struct Partida *p = &partidas[id_partida];
    int mi_id = (p->socket_j1 == socket) ? 1 : 2;
    char msg[150];

    //* Variables de control de errores
    int socket_rival = (mi_id == 1) ? p->socket_j2 : p->socket_j1;

    //* 1. Validar turno
    if (p->turno != mi_id) {
        send(socket, "-Err. No es tu turno\n", 22, 0);
        return;}
    p->turno = (p->turno == 1) ? 2 : 1;
    sprintf(msg, "+Ok. turno pasado\n");
    send(socket, msg, strlen(msg), 0);
}



void procesarMensaje(int socket_cliente, char *buffer, fd_set *readfds, struct Cliente clientes[], int *numClientes, struct Partida partidas[]){
    char comando[50];
    char respuesta[MSG_SIZE];
    char origen_str[100];
    char destino_str[100];
    int unidades_enviadas;
    memset(comando, 0, sizeof(comando));
    memset(respuesta, 0, sizeof(respuesta));
    int id_user = 0;
    if(strlen(buffer) <= 2) return; //por si tiene /n o /r/n
    size_t final = strcspn(buffer, "\n");
    buffer[final] = '\0';
    for(int i=0;i<*numClientes; i++){
        if(clientes[i].socket == socket_cliente){
            id_user =i;
            break;
        }

    }

    if(clientes[id_user].estado == NO_VALIDADO || clientes[id_user].estado == ESPERANDO_PASS){
        if(strcmp(comando,"REGISTRO"))      registro();
        else if(strcmp(comando,"USUARIO"))  usuario();
        else if(strcmp(comando,"PASSWORD")) passwd();
    }else if(clientes[id_user].estado == VALIDADO && strcmp(comando,"INICIAR-PARTIDA")){  iniciar_partida();
    }else if(clientes[id_user].estado == JUGANDO){
        if(strcmp(comando,"ESTADO")) estado(socket, clientes[*numClientes].id_partida, partidas);
        else if(strcmp(comando,"CONQUISTAR")){
           conquistar(socket_cliente, clientes[*numClientes].id_partida, origen_str, destino_str, unidades_enviadas, partidas);
        }else if(strcmp(comando,"ATACAR")){
            atacar(socket_cliente, clientes[*numClientes].id_partida, origen_str, destino_str, unidades_enviadas, partidas);
        }else if(strcmp(comando,"REFORZAR")){
             reforzar(socket_cliente, clientes[*numClientes].id_partida, origen_str, destino_str, unidades_enviadas, partidas);
        }else if(strcmp(comando,"PASAR"))        pasar(socket_cliente, clientes[*numClientes].id_partida, partidas);
        else if(strcmp(comando,"SALIR"))        salirCliente(socket_cliente, &readfds, clientes, numClientes, partidas);
    }
}




