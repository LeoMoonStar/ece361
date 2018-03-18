/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   client.c
 * Author: xuzhuoju
 *
 * Created on March 12, 2018, 1:17 PM
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#define AUTHENTICATE 2
#define BROADCAST 3
#define PRIVATE 4
#define LIST 5

//All the functions that I used are the following: 
int setup_connection(struct sockaddr_in * server_address, int server_socket, int client_socket, char *username) ;
void get_command(char *command);
int parse_command(char *command);

void handle_broadcast();
int send_to_server(int type, char *body);
void handle_pmessage();
void handle_list();
void print_response(int type, char *server_response);
int get_message_command(char *message, char *body);

void format_message(int type, char *message, char *body);
int authenticate(char *username);

void *listen_to_server();

//Global Variable
int client_socket;
int auth = -1;
int ready = 0;


int main(int argc, char** argv) {
   int server_addr = atoi(argv[1]);
   int server_port = atoi(argv[2]);
   char *name = argv[3];
    printf("--------------------PROGRAM INFO------------------------\n");
    printf("Private message: plesae type private name message\n");
    printf("Broadcast message: plesae type broadcast message\n");
    printf("For List and exit, please type list and exit respectively.\n");
    pthread_t listen;
    struct sockaddr_in server_address;

    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    pthread_create(&listen, NULL, listen_to_server, NULL);
    int s; 

    if ( s = setup_connection(&server_address, server_port, client_socket, name) == -1) {
        printf("Error connection to socket, leave the program. \n\n");
        pthread_cancel(listen);
    } else {
        printf("Welcome this client %s\n", name);
        while (1) {
            char command[256];
            printf("Enter the message, 256 chars max. : ");
           fgets(command, 256, stdin);
           printf("\n");
           //this line removes that \n
           command[strcspn(command, "\n")] = '\0';
            if (!parse_command(command)) break;

            //Let thread to print out message.
            int i = 0;
            //Have to use ++i, otherwise when I broadcast first time, it will show error message.
            //Due to errors in thread.
            while (++i < 40000000);
        }
    }
    //After finishing, need to cancel the thread and shutdown the program.
    pthread_cancel(listen);
    shutdown(client_socket, 2);
    return 0;
}

void *listen_to_server(void *ptr) {
  while (1) {
    char server_response[256] = "";
    char response_body[256] = "";
    int status = recv(client_socket, &server_response, sizeof (server_response), 0);
    if (status < 0) {
     return NULL;
     }
     char auth_msg[256] = "send auth";
    if (strstr(server_response, "send")) {
      if (strcmp(server_response, auth_msg) != 0) {
          printf("Authentication Failure: %s\n", (char *)server_response);
          ready = 1;
          auth = 0;
      } else {
        printf("Connection successful. Server Response: %s\n\n", server_response);
        ready = 1;
        auth = 1;
      }
    } else if (strstr(server_response, "PRIVATE ERROR")) {
      printf("Error: The user you are trying to message is not registered.\n");
    }
    else {
      printf("Server Raw Response: %s\n", server_response);
      int type = get_message_command(server_response, response_body);
      print_response(type, response_body);
    }
  }
  return NULL;
}


int authenticate(char *username) {
    printf("Connection success, next is to authenticate user: %s\n", username);

    char server_response[256];
    char auth_msg[256] = "";
    format_message(AUTHENTICATE, auth_msg, username);
    printf("Send authenticate message: %s\n", auth_msg);

    if (send(client_socket, auth_msg, sizeof(auth_msg), 0) < 0)
      printf("Authentication request failed to send, exiting...\n");

    // Parse Server response to see if authenticated
    while (ready == 0 || auth < 0);
    if (auth) return 1;
    else return 0;
}

int setup_connection(struct sockaddr_in * server_address, int server_socket, int client_socket, char *username) {
     // connect to server
    server_address->sin_family = AF_INET;
    server_address->sin_port = htons(server_socket);
    server_address->sin_addr.s_addr = INADDR_ANY;

    int connection_status = connect(client_socket, (struct sockaddr *) server_address, sizeof (*server_address));

    if (connection_status >= 0) {
        if (authenticate(username))
          return 1;
        else
          return -1;
    } else {
        printf("Failed to connect to server\n");
        return -1;
    };
}



void handle_broadcast() {
    char message[256] = "";
    char *token = strtok(NULL, " ");

    while (token != NULL) {
        strcat(message, token);
        strcat(message, " ");

        token = strtok(NULL, " ");
    }
    send_to_server(BROADCAST, message);
}

void handle_pmessage() {
    char message[256] = "";
    char *token = strtok(NULL, " ");

    while (token != NULL) {
        strcat(message, token);
        strcat(message, " ");

        token = strtok(NULL, " ");
    }
    send_to_server(PRIVATE, message);
}

void handle_list() {
    send_to_server(LIST, NULL);
}

int parse_command(char *command) {
    char *cmd = strtok(command, " ");
    char exit_str[256] = "exit";
    char broadcast_str[256]="broadcast";
    char pmessage_str[256]="private";
    char list_str[256]="list";
    if (strcmp(command, exit_str) == 0) {
        return 0;
    } else if (strcmp(command, broadcast_str) == 0) {
        handle_broadcast();
        return 1;
    } else if (strcmp(command, pmessage_str) == 0){
        handle_pmessage();
        return 1;
    } else if (strcmp(command, list_str) == 0){
        handle_list();
        return 1;
    }
}

int send_to_server(int type, char *body) {
    char server_response[256];
    char send_message[256] = "";
    format_message(type, send_message, body);

    printf("Client Request: %s\n", send_message);
    int send_status = send(client_socket, send_message, 256, 0);

    if (type == BROADCAST)
        if (send_status < 0)
          printf("broadcast in error :(");
    else if (type == PRIVATE)
        if (send_status < 0)
          printf("user_name in error :(");
    else if (type == LIST)
        if (send_status < 0)
          printf("list in error :(");
}

void print_response(int type, char *server_response) {
    if (type == BROADCAST)
    {
      char *username = strtok(server_response, " ");
      char *token = strtok(NULL, " ");
      char message[256] = "";

      while (token != NULL) {
        strcat(message, token);
        strcat(message, " ");
        token = strtok(NULL, " ");
      }
      printf("Broadcast message from: %s\n", username);
      printf("Message is: %s\n", message);
    }
    else if (type == PRIVATE)
    {
      char *username = strtok(server_response, " ");
      char *token = strtok(NULL, " ");
      char message[256] = "";

      while (token != NULL) {
        strcat(message, token);
        strcat(message, " ");
        token = strtok(NULL, " ");
      }
      printf("Private message from %s: %s\n", username, message);
    }
    else if (type == LIST)
    {
      printf("Active users are the following: \n\n");
      char *token = strtok(server_response, " ");
      while (token != NULL) {
        printf("   * %s\n", token);
        token = strtok(NULL, " ");
      }
    }

}

int get_message_command(char *message, char *body) {
    char *command = strtok(message, " ");
    char *token = strtok(NULL, " ");

    while (token != NULL) {
        strcat(body, token);
        strcat(body, " ");
        token = strtok(NULL, " ");
    }

      char exit_str[256] = "exit";
    char broadcast_str[256]="broadcast";
    char pmessage_str[256]="private";
    char list_str[256]="list";
     if (strcmp(command, broadcast_str) == 0) {
        return BROADCAST;
    } else if (strcmp(command, pmessage_str) == 0){
        return PRIVATE;
    } else if (strcmp(command, list_str) == 0){
        return LIST;
    } else {
        return 0;
    }
}

void format_message(int type, char *message, char *body) {
    if (type == AUTHENTICATE) {
      strcat(message, "authenticate ");
      strcat(message, body);
    }
    else if (type == BROADCAST)
    //if (type==BROADCAST)
    {
      strcat(message, "broadcast ");
      strcat(message, body);
    }
    else if (type == PRIVATE)
    {
      strcat(message, "private ");
      strcat(message, body);
    }
    else if (type == LIST)
    {
      strcat(message, "list");
    }
}
