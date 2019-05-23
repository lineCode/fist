#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#include "indexer.h"
#include "hashmap.h"
#include "server.h"
#include "dstring.h"

// Commands

#define INDEX "INDEX"
#define SEARCH "SEARCH"

// Responses

#define INDEXED "Text has been indexed\n"
#define TOO_FEW_ARGUMENTS "Too few arguments\n"
#define NOT_FOUND "[]\n"
#define INVALID_COMMAND "Invalid Command\n"

hashmap * handle_connection(int new_socket, hashmap *hm) {  
    int run = 1;
    while(run) {
        dstring req = dempty();
        while(dindexof(req, '\n') == -1 || dindexof(req, '\r') == -1) {
            char buffer[1025]; // Need to preserve the last character being a null character
            memset(buffer, 0, 1025);
            recv(new_socket, buffer, 1024, 0); // Only fill 1024 of the 1025 bytes
            req = dappend(req, buffer);
        }
        
        dstring trimmed = dtrim(req);
        dstringa commands = dsplit(trimmed, ' ');
        printf("%d %s\n", req.length, trimmed.text); 
        if(dequals(req, dcreate("exit")))
            break;

        if(dequals(commands.values[0], dcreate(INDEX))) { 
            printf("INDEX\n");
            if(commands.length < 3) {
                send(new_socket, TOO_FEW_ARGUMENTS, strlen(TOO_FEW_ARGUMENTS), 0); 
            } else {
                dstring document = commands.values[1];
                dstring text = djoin(drange(commands, 2, commands.length), ' ');
                printf("TEXT: '%s'\n", text.text);
                dstringa index = indexer(text, 10);
                printf("INDEX SIZE: %d\n", index.length);
                for(int i = 0; i < index.length; i++) {
                    dstring on = index.values[i];
                    hm = hset(hm, on, document);
                }
                send(new_socket, INDEXED, strlen(INDEXED), 0);
            }
        } else if(dequals(commands.values[0], dcreate(SEARCH))) {
            printf("SEARCH\n");
            if(commands.length < 2) {
                send(new_socket, TOO_FEW_ARGUMENTS, strlen(TOO_FEW_ARGUMENTS), 0);
            } else {
                dstring text = djoin(drange(commands, 1, commands.length), ' ');
                printf("SEARCH STRING: '%s'\n", text.text);
                dstringa value = hget(hm, text);
                printf("%d\n", value.length);
                if(!value.length) {
                    send(new_socket, NOT_FOUND, strlen(NOT_FOUND), 0);
                } else {
                    dstring output = dcreate("[");
                    for(int i = 0; i < value.length; i++) { // This builds the JSON array output
                        dstring on = value.values[i];
                        output = dappendc(output, '"');
                        output = dappend(output, on.text);
                        output = dappendc(output, '"');
                        if(i != value.length - 1)
                            output = dappendc(output, ',');
                    }
                    output = dappendc(output, ']');
                    output = dappendc(output, '\n');
                    send(new_socket, output.text, output.length, 0);
                }
            }
        } else {
            send(new_socket, INVALID_COMMAND, strlen(INVALID_COMMAND), 0); 
        }
    }
    close(new_socket);
    return hm;
}

void start_server(char *host, int port) {
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(socket_fd == 0) {
        printf("Could not create socket\n");
        exit(1);
    }
    int opt = 1;

    if(setsockopt(socket_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt))) {
       perror("Problem setting sockopts");
       exit(EXIT_FAILURE);
    }
    
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if(bind(socket_fd, (struct sockaddr *) &address, sizeof(address)) < 0) {
        perror("Couldn't bind");
        exit(EXIT_FAILURE);
    }

    if(listen(socket_fd, 5) < 0) {
        perror("Couldn't listen");
        exit(EXIT_FAILURE);
    }

    printf("Fist started at localhost:%d\n", port); 

    hashmap *hm = hcreate();

    while(1) {
        int new_socket = accept(socket_fd, (struct sockaddr *) &address, (socklen_t*)&addrlen);
        if(new_socket < 0) {
            perror("Couldn't accept");
            exit(EXIT_FAILURE);
        }
        printf("New connection\n");
        hm = handle_connection(new_socket, hm);
    }
}
