#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <netdb.h>
#include <string.h>

#define MAX_HOST 1024
#define MAX_PATH 1024
#define MAX_REQUEST 1024
#define MAX_ANSWER 2048
#define MAX_PORT 8

typedef struct args_t {
    char *url;
    int debug;
    char *file;
    int default_name;
    int progress;
    int ressources;
} args_t;

/// @brief Initialise la structure args_t 
/// @param args structure contenant les options du programme
void init_args(args_t *args) {
    args->url = NULL;
    args->debug = 0;
    args->file = NULL;
    args->default_name = 0;
    args->progress = 0;
    args->ressources = 0;
}

/// @brief Affiche les instructions d'utilisation du programme
void print_help() {
    fprintf(stderr, "Usage: gethttp <URI>\n"
           "-d  : mode debug\n"
           "-w <fichier> : écriture sur le disque\n"
           "-p : progression et vitesse\n"
           "-r : ressources HTML\n");
}

/// @brief Parse les arguments de la ligne de commande et définit les options du programme
/// @param argc argument standard du main
/// @param argv argument standard du main 
/// @param args structure contenant les options du programme 
void parse_args(int argc, char **argv, args_t *args)
{
    init_args(args);

    int opt;
    while((opt = getopt(argc, argv, ":dw:prh")) != -1) {
        switch (opt)
        {
        case 'd':
            args->debug = 1;
            break;
        case 'w':
            args->file = optarg;
            break;
        case 'p':
            args->progress = 1;
            break;
        case 'r':
            args->ressources = 1;
            break;
        case ':':
            if(optopt == 'w') {
                args->default_name = 1;
            }
            break;
        case 'h':
            print_help();
            exit(EXIT_FAILURE);
        }
    }
    args->url = argv[optind];
}

/// @brief écrit le contenu de la ressource obtenue sur le disque
/// @param filename nom de fichier 
/// @param data données à écrire
void write_ressource(char *filename, char *data) {
    FILE * file;
    file = fopen(filename, "w");
    if(file == NULL) {
        fprintf(stderr, "Erreur lors de l'ouverture du fichier\n");
        exit(EXIT_FAILURE);
    }

    if((fwrite(data, 1, strlen(data), file)) < strlen(data)) {
        fprintf(stderr, "On n'a pas pu écrire toutes les données");
        exit(EXIT_FAILURE);
    }

    fclose(file);
}

/// @brief parse l'url et récupère le hostname et le path
/// @param url url passé en argument 
/// @param hostname on stockera ici le nom domaine récuperé dans l'url
/// @param path on stockera ici le path récuperé dans l'url
/// @return 0 si succès, -1 si échec
int parse_addr(char *url, char *hostname, char *path) {
    size_t i = 0;
    size_t url_len = strlen(url);

    // On recherche un premier / dans l'url
    while(i < url_len && url[i] != '/') {
        i++;
    }

    size_t j = 0;

    // Une fois le premier / trouvé on regarde s'il y en a un 2e
    if(i + 1 < url_len && url[i + 1] == '/') {
        i += 2;
        // On copie le nom de domaine
        while(url[i] != '/' && j < MAX_HOST) {
            hostname[j] = url[i];
            i++;
            j++;
        }
    }
    else {
        return -1;
    }

    // On copie le path, tout ce que l'on trouve après le nom domaine (on ignore le \)
    if(strncpy(path, url + i + 1, MAX_PATH) == NULL) {
        return -1;
    }

    hostname[j] = '\0';
    return 0;
}

/// @brief obtient le port 
/// @param hostname nom de domaine à chercher le port
/// @param port on stockera ici le port récuperé
void get_port(char *hostname, char *port) {
    char *index = strchr(hostname, ':');

    // on vérifie s'il y a un port dans le nom de domaine
    if(index != NULL) {
        snprintf(port, MAX_PORT, "%s", index + 1);
        *index = '\0';
    }
    // s'il n'y a pas de port dans le nom de domaine, on affecte un port par défaut
    else {
        sprintf(port, "80");
    }
}

/// @brief envoie la requête au serveur HTTP
/// @param sock socket du serveur
/// @param hostname nom de domaine pour la requête 
/// @param path chemin pour la requête
void http_get(int sock, char *hostname, char *path) {
    char request[MAX_REQUEST];
    snprintf(request, MAX_REQUEST, "GET /%s HTTP/1.1\nHost: %s\n\n", path, hostname);

    if(send(sock, request, strlen(request), 0) < 0) {
        perror("send");
        exit(1);
    }
}

/// @brief reçoit la réponse à la requête envoyée
/// @param sock socket du serveur 
/// @param debug mode debug 
void receive(int sock, int debug, char *file) {
    char data[MAX_ANSWER];
    ssize_t recieved;
    if((recieved = recv(sock, data, MAX_ANSWER, 0)) < 0) {
        perror("recv");
        exit(1);
    }
    data[recieved] = '\0';

    char *index = strstr(data, "\r\n\r\n");
    index += 4;
    if(!debug || file != NULL) {
        sprintf(data, "%s", index);
    }

    if(file != NULL) {
        write_ressource(file, data);
    }
    else {
        printf("%s", data);
    }
}

/// @brief Permet d'obtenir le nom final de la ressource
/// @param path le chemin d'accès de la ressource
/// @param ressource_name le nom final de la ressource
void get_ressource_name(char *path, char *ressource_name) {
    char *index = strrchr(path, '/');
    if(index == NULL) {
        strcpy(ressource_name, path);
    }
    else {
        strcpy(ressource_name, index + 1);
    }
}

int main(int argc, char **argv) {
    if(argc < 2) {
        print_help();
        return 1;
    }

    args_t args;
    parse_args(argc, argv, &args);

    if(args.url == NULL) {
        fprintf(stderr, "Il faut préciser l'URL\n");
        print_help();
        return 1;
    }

    char hostname[MAX_HOST];
    char path[MAX_PATH];
    char server_port[MAX_PORT];
    char ressource_name[MAX_PATH];
    int sock;

    if(parse_addr(args.url, hostname, path) == -1) {
        fprintf(stderr, "Erreur lors du parsage\n");
        return 1;
    }
    get_port(hostname, server_port);

    struct addrinfo filter;
    struct addrinfo *server;

    memset(&filter, 0, sizeof(filter));

    filter.ai_family = AF_INET;
    filter.ai_socktype = SOCK_STREAM;

    int status = getaddrinfo(hostname, server_port, &filter, &server);
    if(status != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return 1;
    }

    sock = socket(server->ai_family, server->ai_socktype, server->ai_protocol);
    if(sock < 0) {
        perror("socket");
        return 1;
    }

    if(connect(sock, server->ai_addr, sizeof(*(server->ai_addr))) != 0) {
        perror("connect");
        return 1;
    }

    http_get(sock, hostname, path);

    if(args.file == NULL && args.default_name == 1) {
        get_ressource_name(path, ressource_name);
        args.file = strdup(ressource_name);
    }
    receive(sock, args.debug, args.file);

    freeaddrinfo(server);
    close(sock);
}