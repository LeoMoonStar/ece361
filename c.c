/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   main.c
 * Author: jiaweiyang
 *
 * Created on March 19, 2018, 1:13 AM
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>



#define MAXBUFLEN 100
#define backlog 10
#define AUTHENTICATE 2
#define BROADCAST 3
#define PRIVATE 4
#define LIST 5



typedef struct{
    char user_name[50];
    int socket_id;
} User;  // Every user has name and socket_id

int findSocketId(User** users, int socket, int user_id);
void broadcastHandler(fd_set *master, char *msg_body,int fdmax,int server_socket,int sender,User **users,int id);
void handlePrivate(User **users, int send_to, char *basic_message, int * userNum);
void setup(int *server_socket,int port_num);
void removeUser(User** users,int socket,int *user_id);
int checkcommand(char * recv_message,char * msg);
void handleRequest(int function,char * basic_message,fd_set *master,int fd_max,int server_socket,User **users,int client_socket,
        int *user_id);
void handleAuth(char* msg_body, fd_set * master, User ** users, int *id, int socket);
void handleBroadcast(fd_set * master, char* msg_body, int fdmax, int server_socket, int sender, User ** users, int id);
void findName(User** users, int user_id, char* uname);
void handleList(User **users, int client_socket, int* user_id);




void sigchld_handler(int s){
    int saved_errno=errno;
    while(waitpid(-1,NULL,WNOHANG)>0);
    errno=saved_errno;
}

void *get_in_addr(struct sockaddr *sa){
    if(sa->sa_family== AF_INET){
        return &((struct sockaddr_in*)sa)->sin_addr;
    }
    return &((struct sockaddr_in6*)sa)->sin6_addr;
}

int findSocketId(User **users,int socket,int userNum){
    int i=0;
    int found = -1;
    for (i=0;i<userNum;i++){
        if(socket==users[i]->socket_id){
            found=i;
            break;
        }
    }
    return found;
}

int main(int argc, char** argv) {
    int port_num=atoi(argv[1]);//assign the port number 
    int userNum=0;
    
    User *users[backlog];
    fd_set master;
    fd_set read_fds;
    int fdmax;// max file descriptor for select
    
    FD_ZERO(&master);
    FD_ZERO(&read_fds);
    
    struct sockaddr_storage their_add;
    socklen_t address_size;
    
    struct addrinfo hints,*servinfo,*p;
    struct sigaction sa;
    int yes=1;
    char s[MAXBUFLEN];
    int status;
    
    
    int server_socket;
    setup(&server_socket,port_num);
    listen(server_socket,backlog);
    // now accept an incoming connection
    FD_SET(server_socket,&master);
    fdmax=server_socket;
    
    // main loop
    
    while(1){
        read_fds=master;
        if(select(fdmax+1,&read_fds,NULL,NULL,NULL)==-1){
            printf("select has a problem\n");
            break;
        }
        //client socket index
        int socket=0;
        for(socket=0;socket<fdmax;socket++){
            if(FD_ISSET(socket,&read_fds)){
                if(socket==server_socket){
                    int newfd=accept(server_socket,NULL,NULL);
                    if(newfd==-1){
                    printf("ERROR connecting to new client!\n");
                    } 
                    else 
                    {
                        FD_SET(newfd,&master);
                        if(newfd>fdmax)
                            fdmax=newfd;
                    }
                }
                }
            else {
                char recv_message[256];
                char server_message[256]="auth";
                char basic_message[256];
                
                //receive data from client and send auth upon success
                int recv_size=recv(socket,&recv_message,sizeof(recv_message),0);
                if(recv_size==-1){
                    printf("error receiving message\n\n");
                    FD_CLR(socket,&master);
                    removeUser(users,socket,&userNum);
                }   else {
                    int function= checkcommand(recv_message,basic_message);
                    handleRequest(function,basic_message,&master,fdmax,server_socket,user.socket,&userNum);
                    strcpy(basic_message,"");
                    
                }
            }
        }
    }
    
    
    shutdown(server_socket,2);
    
  
    return 0;
}

void setup(int *server_socket,int port_num){
    *server_socket=socket(AF_INET,SOCK_STREAM,0);
    struct sockaddr_in server_address;
    unsigned long s_addr=1000;
    
    server_address.sin_family=AF_INET;
    server_address.sin_port=htons(port_num);
    server_address.sin_addr.s_addr=INADDR_ANY;
    
    bind(*server_socket,(struct sockaddr*)&server_address,sizeof(server_address));
}

void removeUser(User ** users,int socket,int *user_id){
    int id=findSocketId(users,socket,*user_id);
    int i=0;
    
    free(users[id]);
    for(i=id;i<*user_id;i++){
        if(i+1!=*user_id){
            users[i]=users[i+1];
        }
    }
    users[*user_id-1]=NULL;
    *user_id=*user_id-1;
}

void handleRequest(int function,char * basic_message,fd_set *master,int fdmax,int server_socket,User **users,int client_socket,
        int *user_id){
    if (function == AUTHENTICATE){
      handleAuth(basic_message, master, users, user_id, client_socket);
  }
  else if (function == BROADCAST) {
      printf("\ncommand is broadcast receivied\n");
      handleBroadcast(master, basic_message, fdmax, server_socket, client_socket, users, *user_id);
      printf("Broadcast Message is: %s\n\n", basic_message); 
  }
 
  else if (function == PRIVATE) {
      printf("\ncommand is user_name receieved\n");
      handlePrivate(users, client_socket, basic_message, *user_id);
  }
  else if (function == LIST) {
      printf("\ncommand is list. Get all the names in the client.\n");
      handleList(users, client_socket, *user_id);
  }
}
void handleBroadcast(fd_set * master, char* msg_body, int fdmax, int server_socket, int sender, User ** users, int id){
    int i = 0;
    char right_msg[256];

    int sendid = findSocketId(users, sender, id);
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

void handleList(User **users, int client_socket, int *user_id){
  int i = 0;
  char user_list[256];
  strcpy(user_list, "list");

  // Create user_list string
  for (i = 0; i < user_id; i++) {
      strcat(user_list, " ");
      strcat(user_list, users[i]->user_name);
  }

  // Send user list to send_to socket
  int s;
  if (s = send(client_socket, user_list, 256, 0) == -1) {
      printf("error sending list to: %d!!\n", client_socket);
  } else {
      printf("User is the following: \n%s\n", user_list);
  }
}
void handlePrivate(User **users, int send_to, char *basic_message, int * userNum){
    char sender_name[256];
    char dest_name[256];
    char mes[256];
    strcpy(mes,basic_message);
    
    char err_msg[256]="Error on sending private message";
    
    int rcv_socket,rcv_id,destid;
    
    destid=findSocketId(users,send_to,userNum);
    dest_name=users[destid]->user_name;
    parseName(sender_name,mes,basic_message,dest_name);
    printf("sending message to this user:%s",dest_name);

    //msg_body includes names, message information and types.
    printf("Private message is, including private type, username and message: %s\n\n", mes);
    //Get receiver sock id
    rcv_id = findName(users, userNum, sender_name);
    if (rcv_id == -1) {
        printf("error: User_name not found!!\n");
        send(send_to, err_msg, 256, 0);
        return;
    }
    else {
      rcv_socket = users[rcv_id]->socket_id;
    }

    //send message to receiever. If wrong, show sending private message error.
    int send_status = send(rcv_socket, mes, 256, 0);
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
