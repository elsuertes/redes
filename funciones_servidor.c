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

void registro(int socket, char *user, char *password) {
    if (strlen(user) == 0 || strlen(password) == 0) {
        char err[] = "-Err. Formato: REGISTRO -u <usuario> -p <password>\n";
        send(socket, err, strlen(err), 0);
        return;
    }

    FILE *f = fopen("usuarios.txt", "a+");
    if (f == NULL) {
        char err[] = "-Err. Error interno al acceder a la base de datos\n";
        send(socket, err, strlen(err), 0);
        return;}

    char u[50], p[50];
    rewind(f);
    //* leemos el fichero linea a linea y comprobamos que no exista ya el usuario
    while (fscanf(f, "%49s %49s", u, p) == 2) {
        if (strcmp(u, user) == 0) {
            char err[] = "-Err. El usuario ya existe\n";
            send(socket, err, strlen(err), 0);
            fclose(f);
            return;
        }
    }

    fprintf(f, "%s %s\n", user, password);
    fclose(f);

    char ok[] = "+Ok. Usuario registrado con éxito\n";
    send(socket, ok, strlen(ok), 0);
}

void usuario(int socket, char *nombre_usuario, struct Cliente *clientes) {
    FILE *f = fopen("usuarios.txt", "r");
    if (f == NULL) {
        char err[] = "-Err. No hay usuarios registrados en el sistema\n";
        send(socket, err, strlen(err), 0);
        return;}

    char u[50], p[50];
    bool existe = false;

    while (fscanf(f, "%49s %49s", u, p) == 2) {
        if (strcmp(u, nombre_usuario) == 0) {
            existe = true;
            break;}
    }
    fclose(f);

    if (existe) {
        strncpy(clientes->username, nombre_usuario, sizeof(clientes->username) - 1);
        clientes->username[sizeof(clientes->username) - 1] = '\0';
        clientes->estado = ESPERANDO_PASS;
        char ok[] = "+Ok. Usuario correcto. Introduzca contraseña\n";
        send(socket, ok, strlen(ok), 0);
    }else{
        char err[] = "-Err. Usuario no encontrado\n";
        send(socket, err, strlen(err), 0);
    }
}

void passwd(int socket, char *password, struct Cliente *clientes) {
    if (clientes->estado != ESPERANDO_PASS) {
        char err[] = "-Err. Debe enviar primero el comando USUARIO\n";
        send(socket, err, strlen(err), 0);
        return;}
    FILE *f = fopen("usuarios.txt", "r");
    if (f == NULL) {
        char err[] = "-Err. Error interno al verificar contraseña\n";
        send(socket, err, strlen(err), 0);
        return;}

    char u[50], p[50];
    bool validado = false;

    while (fscanf(f, "%49s %49s", u, p) == 2) {
        if (strcmp(u, clientes->username) == 0 && strcmp(p, password) == 0) {
            validado = true;
            break;
        }
    }
    fclose(f);

    if (validado) {
        clientes->estado = VALIDADO;
        char ok[] = "+Ok. Usuario validado\n";
        send(socket, ok, strlen(ok), 0);
    } else {
        // Vuelve a NO_VALIDADO para obligar a reiniciar el flujo si falla
        clientes->estado = NO_VALIDADO;
        char err[] = "-Err. Contraseña incorrecta\n";
        send(socket, err, strlen(err), 0);
    }
}

void iniciar_partida(int socket_cliente, struct Cliente clientes[], int numClientes, struct Partida partidas[], int maxPartidas){
    int idx_solicitante = -1;
    int idx_rival = -1;
    int id_p = -1;
    for (int i = 0; i < numClientes; i++){
        if (clientes[i].socket == socket_cliente){
            idx_solicitante = i;
            break;
        }
    }
    if (idx_solicitante == -1) return;
    //* Marcamos al jugador en cola de espera
    clientes[idx_solicitante].estado = BUSCANDO_PARTIDA;
    //* Buscamos si hay otro jugador esperando
    for (int i = 0; i < numClientes; i++){
        if (i != idx_solicitante && clientes[i].estado == BUSCANDO_PARTIDA){
            idx_rival = i;
            break;
        }
    }
    //* Si nadie más está buscando, se le informa 
    if (idx_rival == -1){
        char espera[] = "+Ok. Esperando a otro jugador...\n";
        send(socket_cliente, espera, strlen(espera), 0);
        return;
    }
    //* Si hay rival, buscamos una ranura de partida libre
    for (int i = 0; i < maxPartidas; i++){
        if (!partidas[i].activa){
            id_p = i;
            break;
        }
    }
    if (id_p == -1){
        char lleno[] = "-Err. No hay partidas disponibles en el servidor\n";
        send(socket_cliente, lleno, strlen(lleno), 0);
        send(clientes[idx_rival].socket, lleno, strlen(lleno), 0);
        clientes[idx_solicitante].estado = VALIDADO;
        clientes[idx_rival].estado = VALIDADO;
        return;
    }

    //* Inicializar datos de la partida
    partidas[id_p].activa = true;
    partidas[id_p].socket_j1 = clientes[idx_rival].socket;
    partidas[id_p].socket_j2 = clientes[idx_solicitante].socket;
    partidas[id_p].turno = 1; // Comienza J1
    for (int f = 0; f < 5; f++){
        for (int c = 0; c < 5; c++){
            partidas[id_p].tablero[f][c].propietario = 0;
            partidas[id_p].tablero[f][c].tropas = 0;
        }
    }

    int f1 = rand() % 5;
    int c1 = rand() % 5;
    int f2, c2;
    do {
        f2 = rand() % 5;
        c2 = rand() % 5;
    } while (f1 == f2 && c1 == c2);
    //* Casillas base iniciales (por ejemplo: J1 en A1 [0][0] y J2 en E5 [4][4])
    partidas[id_p].tablero[f1][c1].propietario = 1;
    partidas[id_p].tablero[f1][c1].tropas = 30;
    partidas[id_p].tablero[f2][c2].propietario = 2;
    partidas[id_p].tablero[f2][c2].tropas = 30;
    //* Actualizar estado de ambos jugadores
    clientes[idx_rival].estado = JUGANDO;
    clientes[idx_rival].id_partida = id_p;
    clientes[idx_solicitante].estado = JUGANDO;
    clientes[idx_solicitante].id_partida = id_p;

    //* Notificar el inicio a ambos jugadores
    char msg_j1[] = "+Ok. Partida encontrada. Eres el Jugador 1 (J1). Tu turno.\n";
    char msg_j2[] = "+Ok. Partida encontrada. Eres el Jugador 2 (J2). Turno de tu rival.\n";
    send(partidas[id_p].socket_j1, msg_j1, strlen(msg_j1), 0);
    send(partidas[id_p].socket_j2, msg_j2, strlen(msg_j2), 0);
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
    char user[100];
    char contraseña[100];
    char origen_str[100];
    char destino_str[100];
    char aux[100];
    int unidades_enviadas = 0;
    memset(comando, 0, sizeof(comando));
    memset(respuesta, 0, sizeof(respuesta));
    memset(user, 0, sizeof(user));
    memset(contraseña, 0, sizeof(contraseña));
    memset(origen_str, 0, sizeof(origen_str));
    memset(destino_str, 0, sizeof(destino_str));
    memset(aux, 0, sizeof(aux));
    if (strlen(buffer) <= 2) return;
    buffer[strcspn(buffer, "\n")] = '\0';
    //* Identificar el índice del cliente en el array
    int id_user = -1;
    for (int i = 0; i < *numClientes; i++){
        if (clientes[i].socket == socket_cliente){
            id_user = i;
            break;
        }
    }
    if (id_user == -1) return;

  // Extraer COMANDO
    char *token = strtok(buffer, " ");
    if (token == NULL) return;
    strncpy(comando, token, sizeof(comando) - 1);
    if (strcmp(comando, "REGISTRO") == 0) {
        token = strtok(NULL, " ");
        if (token) strncpy(user, token, sizeof(user) - 1);
        token = strtok(NULL, " ");
        if (token) strncpy(contraseña, token, sizeof(contraseña) - 1);
    } else if (strcmp(comando, "USUARIO") == 0) {
        token = strtok(NULL, " ");
        if (token) strncpy(user, token, sizeof(user) - 1);
    } else if (strcmp(comando, "PASSWORD") == 0) {
        token = strtok(NULL, " ");
        if (token) strncpy(contraseña, token, sizeof(contraseña) - 1);
    } else if (strcmp(comando, "CONQUISTAR") == 0 || strcmp(comando, "ATACAR") == 0 || strcmp(comando, "REFORZAR") == 0) {
        token = strtok(NULL, " ");
        if (token) strncpy(origen_str, token, sizeof(origen_str) - 1);
        token = strtok(NULL, " ");
        if (token) strncpy(destino_str, token, sizeof(destino_str) - 1);
        token = strtok(NULL, " ");
        if (token) strncpy(aux, token, sizeof(aux) - 1); unidades_enviadas = atoi(aux);
    }

    // 3. Ejecutar la acción según el estado del cliente
    int id_partida_actual = clientes[id_user].id_partida;

    if (clientes[id_user].estado == NO_VALIDADO || clientes[id_user].estado == ESPERANDO_PASS) {
        if (strcmp(comando, "REGISTRO") == 0) {
            registro(socket_cliente, user, contraseña);
        } else if (strcmp(comando, "USUARIO") == 0) {
            usuario(socket_cliente, user, &clientes[id_user]);
        } else if (strcmp(comando, "PASSWORD") == 0) {
            passwd(socket_cliente, contraseña, &clientes[id_user]);
        } else {
            char err[] = "-Err. Debe autenticarse o registrarse primero\n";
            send(socket_cliente, err, strlen(err), 0);
        }
    } else if (clientes[id_user].estado == VALIDADO) {
        if (strcmp(comando, "INICIAR-PARTIDA") == 0) {
            iniciar_partida(socket_cliente, clientes, *numClientes, partidas, MAX_PARTIDAS);
        } else if (strcmp(comando, "SALIR") == 0) {
            salirCliente(socket_cliente, readfds, clientes, numClientes, partidas);
        }
    } else if (clientes[id_user].estado == JUGANDO) {
        if (strcmp(comando, "ESTADO") == 0) {
            estado(socket_cliente, id_partida_actual, partidas);
        } else if (strcmp(comando, "CONQUISTAR") == 0) {
            conquistar(socket_cliente, id_partida_actual, origen_str, destino_str, unidades_enviadas, partidas);
        } else if (strcmp(comando, "ATACAR") == 0) {
            atacar(socket_cliente, id_partida_actual, origen_str, destino_str, unidades_enviadas, partidas);
        } else if (strcmp(comando, "REFORZAR") == 0) {
            reforzar(socket_cliente, id_partida_actual, origen_str, destino_str, unidades_enviadas, partidas);
        } else if (strcmp(comando, "PASAR") == 0) {
            pasar(socket_cliente, id_partida_actual, partidas);
        } else if (strcmp(comando, "SALIR") == 0) {
            salirCliente(socket_cliente, readfds, clientes, numClientes, partidas);
        }
    }
}




