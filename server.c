/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>
#include <time.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#define AUTHENTICATE 2
#define BROADCAST 3
#define PRIVATE 4
#define LIST 5


/*
 *
 */

 typedef struct {
     char user_name[50];
     int socket_id;

 } User;

void removeUser(User ** users, int socket, int *user_id);
int findSocketID(User ** users, int socket, int user_id);
int checkCommand(char * recv_msg, char * msg);
void handleRequest(int function, char * msg_body, fd_set * master, int fdmax, int server_socket, User **users, int client_socket, int * user_id);

void handlePrivate(User ** users, int sender, char * msg_body, int user_id);
int findName(User ** users, int user_id, char * uname);
void parseName(char * user_name, char * msg_to_send, char * msg_body, char * send_uname);
void handleList(User ** users, int send_to, int id);
int findUser(User ** users, char* username, int num_users);
void handleAuth(char* msg_body, fd_set * master, User ** users, int *id, int socket);
void handleBroadcast(fd_set * master, char* msg_body, int fdmax, int server_socket, int sender, User ** users, int id);
void setupSocket(int *server_socket, int port_num);


int main(int argc, char** argv) {
    //Need to have argv[1] to have successful connection between client and server
    //Otherwise, fail.
    int port_num=atoi(argv[1]);
    //Maximum of user is 10
    User *users[10];
    int user_id = 0;    // Keep track of how many users are connected

    // Setup fd_sets for concurrency
    fd_set master;
    fd_set read_fds;
    int fdmax;  // Max file descriptor for select()

    FD_ZERO(&master);
    FD_ZERO(&read_fds);


    // Server Bootstrap
    int server_socket;
    setupSocket(&server_socket, port_num);
    //5 is used for different commands. 
    listen(server_socket, 5);

    // Add server_socket to master set
    FD_SET(server_socket, &master);

    // keep track of the biggest file
    fdmax = server_socket;
    listen(server_socket, 5);

    // Add server_socket to master set
    FD_SET(server_socket, &master);

    // Keep track the biggest file
    fdmax = server_socket;

    while (1) {
        read_fds = master;
        //Needs to be biggest file, so fdmax has to plus 1.
        if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
          printf("select has a problem\n");
          break;
        }

        // Client socket index
        int socket = 0;
        for (socket = 0; socket <= fdmax; socket++) {
           //If we got a send and recv request
          if (FD_ISSET(socket, &read_fds)){ 
            if (socket == server_socket) {  // The server got a connection request
              // Handle new connections
              int newfd;
               newfd = accept(server_socket, NULL, NULL);
              // Check if connection to newfd was success
              if (newfd == -1) {
                printf("Error connecting to new client!\n\n");
              } else {
                FD_SET(newfd, &master); // Add new socket to master
                if (newfd > fdmax) {  // Change value of fdmax
                  fdmax = newfd;
                }
              }
            } else {  // Handle data from client
              char recv_message[256];
              char server_message[256] = "auth";
              char basic_message[256]; //

              // Receive data from client and send AUTH upon success
              int recv_size = recv(socket, &recv_message, sizeof (recv_message), 0);
              if (recv_size == -1 ) {
                  printf("Error receieving message\n\n");
                  FD_CLR(socket, &master);
              } else if (recv_size == 0) {
                  printf("Leaving the program, it is closed\n\n");
                  FD_CLR(socket, &master);
                  removeUser(users, socket, &user_id);
              }  else {

                //function: takes in broadcast, private
                //message: messages i want to type
                int function = checkCommand(recv_message, basic_message);
                handleRequest(function, basic_message, &master, fdmax, server_socket, users, socket, &user_id);
                strcpy(basic_message, "");

              }
            }
          }
        }
    }
    shutdown(server_socket, 2);
    return 0;
}

// Remove user socket from array
void removeUser(User ** users, int socket, int *user_id) {
    int id = findSocketID(users, socket, *user_id);
    int i = 0;
    //avoid seg fault.
    free(users[id]);
    for (i = id; i < *user_id; i++) {
        if (i+1 != *user_id) {
            users[i] = users[i+1];
        }
    }
    users[*user_id - 1] = NULL;
    *user_id = *user_id - 1;
}

//Return the index of user in array
int findSocketID(User ** users, int socket, int user_id) {
    int i = 0;
    int found = -1;
    for (i = 0; i < user_id; i++) {
      if (socket == users[i]->socket_id){
          found = i;
          break;
      }
    }
    return found;
}

int checkCommand(char * recv_msg, char * msg) {
    char * command = strtok(recv_msg, " ");
    int type;

    if (strcmp(command, "list") == 0) 
        type = LIST;
    else if (strcmp(command, "broadcast") == 0) 
        type = BROADCAST;
    else if (strcmp(command, "authenticate") == 0) 
        type = AUTHENTICATE;
    else if (strcmp(command, "private") == 0) 
        type = PRIVATE;

    command = strtok(NULL, " ");

    // Parse out message
    while (command != NULL) {
      strcat(msg, command);
      strcat(msg, " ");
      command = strtok(NULL, " ");
    }

    return type;
}

void handleRequest(int function, char * msg_body, fd_set * master, int fdmax, int server_socket, User **users, int client_socket, int * user_id) {
  //handle if user_name is repeat or not.
  if (function == AUTHENTICATE){
      handleAuth(msg_body, master, users, user_id, client_socket);
  }
  else if (function == BROADCAST) {
      printf("\ncommand is broadcast receivied\n");
      handleBroadcast(master, msg_body, fdmax, server_socket, client_socket, users, *user_id);
      printf("Broadcast Message is: %s\n\n", msg_body); 
  }
  else if (function == PRIVATE) {
      printf("\ncommand is user_name receieved\n");
      handlePrivate(users, client_socket, msg_body, *user_id);
  }
  else if (function == LIST) {
      printf("\ncommand is list. Get all the names in the client.\n");
      handleList(users, client_socket, *user_id);
  }
}

void handlePrivate(User ** users, int sender, char * msg_body, int user_id) {
    char user_name[50];
    char send_uname[50];
    char msg[256];
    char err_msg[256] = "Private Error";
    int rcv_socket, rcv_id, send_id;

    //Get message and user name in the server
    send_id = findSocketID(users, sender, user_id);
   
    parseName(user_name, msg, msg_body, users[send_id] -> user_name);

    printf("Sending the message to this user_name: %s\n", user_name);

    //msg_body includes names, message information and types. 
    printf("Private message is, including private type, username and message: %s\n\n", msg);
    //Get receiver sock id
    rcv_id = findName(users, user_id, user_name);
    if (rcv_id == -1) {
        printf("error: User_name not found!!\n");
        send(sender, err_msg, 256, 0);
        return;
    }
    else {
      rcv_socket = users[rcv_id]->socket_id;
    }

    //send message to receiever. If wrong, show sending private message error.
    int send_status = send(rcv_socket, msg, 256, 0);
    if (send_status == -1 ) {
        printf("Error sending private message\n\n");
    } 
}

int findName(User ** users, int user_id, char * uname) {
    strcat(uname, " ");
    int rcv_id = -1;
    int i = 0;
    for (i = 0; i < user_id; i++) {
        if (strcmp(users[i]->user_name, uname) == 0) {
            rcv_id = i;
            break;
        }
    }
    return rcv_id;
}

void parseName(char * user_name, char * msg_to_send, char * msg_body, char * send_uname) {
    char * uname = strtok(msg_body, " ");
    strcpy(user_name, uname);

    strcpy(msg_to_send, "private ");
    strcat(msg_to_send, send_uname);
    strcat(msg_to_send, " ");

    char * token = strtok(NULL, " ");
    while (token != NULL) {
        strcat(msg_to_send, token);
        strcat(msg_to_send, " ");
        token = strtok(NULL, " ");
    }

}

void handleList(User ** users, int send_to, int id) {
  int i = 0;
  char user_list[256];
  strcpy(user_list, "list");

  // Create user_list string
  for (i = 0; i < id; i++) {
      strcat(user_list, " ");
      strcat(user_list, users[i]->user_name);
  }

  // Send user list to send_to socket
  int s;
  if (s = send(send_to, user_list, 256, 0) == -1) {
      printf("error sending list to: %d!!\n", send_to);
  } else {
      printf("User is the following: \n%s\n", user_list);
  }
}


int findUser(User ** users, char* username, int num_users) {
    int i = 0;
    for (i = 0; i < num_users; i++) {
        if (strcmp(users[i]->user_name, username) == 0){
          return 1;
        }
    }
    return 0;
}

//Checking for information is authenticate or not.
void handleAuth(char* msg_body, fd_set * master, User ** users, int *id, int socket){
    char server_message[256] = "send auth";
    char denied[256] = "send denied";

    //If name not found, add user name to the array
    if (*id == 0 || !findUser(users, msg_body, *id )){

        User * new_user = malloc(sizeof(User));
        users[*id] = new_user;
        strcpy(users[*id]->user_name, msg_body);
        users[*id] -> socket_id = socket;

        send(socket, server_message, sizeof(server_message), 0);

        *id = *id + 1;


    } else {
        printf("error: User_name %s is already taken :( \n\n", msg_body);
        send(socket, denied, sizeof(denied), 0);
        FD_CLR(socket, master);
    }
}


//sends message
void handleBroadcast(fd_set * master, char* msg_body, int fdmax, int server_socket, int sender, User ** users, int id) {
    int i = 0;
    char right_msg[256];

    int sendid = findSocketID(users, sender, id);
    strcpy(right_msg, "broadcast ");
    strcat(right_msg, users[sendid]->user_name);
    strcat(right_msg, msg_body);

    for (i = 0; i <= fdmax; i++) {
      if (FD_ISSET(i, master)) {
          if (i != server_socket) {
            if (send(i, right_msg, 256, 0) == -1){
                printf("error sending broadcast to: %d\n\n", i);
            }
          }
      }
    }
}

void setupSocket(int *server_socket, int port_num) {
    *server_socket = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in server_address;
    unsigned long s_addr = 1000;

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port_num);
    server_address.sin_addr.s_addr = INADDR_ANY;

    bind(*server_socket, (struct sockaddr*) &server_address, sizeof (server_address));

}

