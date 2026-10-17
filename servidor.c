#include "servidor.h"

volatile sig_atomic_t server_encendido = 1;


int main(){
    //* comportamiento de ctrl +c
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = manejador;  //* asigna la funcion
    sigemptyset(&sa.sa_mask);   //* no bloquear otras señales
    sa.sa_flags = 0;            //* llamadas bloqueantes (ej:select) devuelven error
    sigaction(SIGINT, &sa, NULL);

	//*	Descriptor del socket y buffer de datos                
	int on = 1;
    srand(time(NULL));
	struct sockaddr_in sockname, from;
    struct Cliente clientes[MAX_CLIENTS];
    struct Partida partidas[MAX_PARTIDAS];
    memset(clientes,0,sizeof(clientes));
    memset(partidas,0,sizeof(partidas));
	socklen_t from_len;
    fd_set readfds, auxfds; //* Descriptor de lectura y auxiliar
    int socket_maestro, socket_new_user, i, bytes_recibidos, salida;
    int numClientes = 0;
    char buffer[MSG_SIZE];

    //* abrimos el socket en modo IPv4 y TCP
  	socket_maestro = socket (AF_INET, SOCK_STREAM, 0);
	if (socket_maestro == -1) { perror("-Err. No se puede abrir el socket cliente\n"); exit (EXIT_FAILURE);}

    //* evitamos el bloqueo del puerto
    if(setsockopt( socket_maestro, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) == -1){ perror("-Err. Error en la operación setsockopt"); exit(EXIT_FAILURE); }  
	sockname.sin_family = AF_INET;
	sockname.sin_port = htons(2026);
	sockname.sin_addr.s_addr = htonl(INADDR_ANY);

    //* Asociamos IP y puerto 2026 al socket_maestro
	if (bind (socket_maestro, (struct sockaddr *) &sockname, sizeof (sockname)) == -1){ perror("-Err. Error en la operación bind"); exit(EXIT_FAILURE); }

	//* De las peticiones que vamos a aceptar sólo necesitamos el tamaño de su estructura, el resto de información nos la proporcionará el método que recibe las peticiones.
	from_len = sizeof (from);
	if(listen(socket_maestro,5) == -1) { perror("-Err. Error en la operación de listen");exit(EXIT_FAILURE); } //* 5 numero de jugadores que puede haber en cola
    printf("-Ok. El servidor está esperando conexiones...\n");	//Inicializar los conjuntos fd_set	

    //* limpiamos descriptores y agregamos el socketmaestro (para nuevas conexiones) y 0 (para comandos de admin) 
	FD_ZERO(&readfds);
    FD_ZERO(&auxfds);
    FD_SET(socket_maestro,&readfds);
    FD_SET(0,&readfds);
	while(server_encendido){    
        //* Esperamos recibir mensajes de los clientes
        auxfds = readfds;
        salida = select(FD_SETSIZE,&auxfds,NULL,NULL,NULL);   //! Bloquea proceso hasta que algun descriptor esté para lectura (y modifica auxfds)       
        if (!server_encendido) {
            break;}
        if (salida < 0 && errno == EINTR) { //! evitamos que al cerrar abruptamente tras ctrl+c se lo tome como un error
            continue;}
        if(salida > 0){
            //* Buscamos el socket que ha establecido la comunicación
            for(i=0; i<FD_SETSIZE; i++){
                if(FD_ISSET(i, &auxfds)) {
                    if(i == socket_maestro){
                        if((socket_new_user = accept(socket_maestro, (struct sockaddr *)&from, &from_len)) == -1){ 
                            perror("-Err. Error aceptando peticiones");
                        } else {
                            if(numClientes < MAX_CLIENTS){ 
                                clientes[numClientes].socket = socket_new_user;
                                clientes[numClientes].estado = NO_VALIDADO; 
                                clientes[numClientes].id_partida = -1; 
                                clientes[numClientes].turno = 0;
                                numClientes++;
                                FD_SET(socket_new_user, &readfds); //* añadimos el socket del cliente para poder monitorizar sus mensajes
                                
                                //* mandamos mensaje de confirmacion
                                memset(buffer, 0, sizeof(buffer));
                                strcpy(buffer, "+Ok. Usuario conectado\n");
                                send(socket_new_user, buffer, strlen(buffer), 0);
                            }else{
                                memset(buffer, 0, sizeof(buffer));
                                strcpy(buffer,"-Err. Demasiados clientes conectados\n");
                                send(socket_new_user,buffer,strlen(buffer),0);
                                close(socket_new_user);
                            }   
                        }
                    }else if(i == 0){       //* Se ha introducido info por la terminal del server
                        memset(buffer, 0, sizeof(buffer));
                        fgets(buffer, sizeof(buffer),stdin);
                        //! Controlar si se ha introducido "SALIR", cerrando todos los sockets y finalmente saliendo del servidor
                        if(strcmp(buffer,"SALIR\n") == 0 || server_encendido == 0){
                            server_encendido = 0;
                            break; //* salimos del for para llamar a cerrar cliente
                        }
                    }else{  //! si no se ha introducido info por la terminal del server
                        memset(buffer, 0, sizeof(buffer));
                        bytes_recibidos = recv(i,buffer,sizeof(buffer)-1,0);
                        if(bytes_recibidos > 0){
                            buffer[bytes_recibidos] = '\0';
                            if(strcmp(buffer, "SALIR") == 0 || strcmp(buffer, "SALIR\n") == 0) salirCliente(i,&readfds, clientes, &numClientes, partidas); 
                            else    procesarMensaje(i, buffer, &readfds, clientes, &numClientes, partidas);
                        }else if(bytes_recibidos == 0){
                            printf("-Ok. El socket %d ha cerrado la conexión (Ctrl+C o desconexión)\n", i);
                            salirCliente(i, &readfds, clientes, &numClientes, partidas);
                        }else{
                            perror("-Err. Error en recv");
                            salirCliente(i, &readfds, clientes, &numClientes, partidas);
                        }
                    }
                }
            }
	    }
    }
    salirServer(buffer, &readfds, clientes, &numClientes);
    close(socket_maestro);
	return 0;
}