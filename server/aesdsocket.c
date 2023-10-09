// Nicholas Buckley AESDsocket socket base program!

// change found on stack overflow to allow netdb import to work!
#define _XOPEN_SOURCE 600


#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <stddef.h>
#include <syslog.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/stat.h>


#define BUFFER_SIZE 1024
#define DATA_PATH ("/var/tmp/aesdsocketdata")
#define PORT_NUM ("9000")


/*
struct addrinfo {
    int ai_flags;
    int ai_family;
    int ai_socktype;
    int ai_protocol;
    size_t ai_addrlen;
    struct sockaddr *ai_addr;
    char *ai_canonname;

    struct addrinfo *ai_next;
};*/


int sockfd;
int openConSock;
int writefp;
int readfp;
// setup system log with PID and CONS and at priorty USER


bool runAsDaemon = false;

/* simple clean routine called by signal handler for SIGINT and SIGTERM to close all 
*  file pointers and sockets, remove the file writen to and from and close the logging
*  this function was and sig handler skeletons were gotten from bing chat gpt
*  "Gracefully exits when SIGINT or SIGTERM is received, completing any open connection operations,
*  closing any open sockets, and deleting the file /var/tmp/aesdsocketdata."
*/
void cleanUp(void)
{
    if (writefp != -1) 
    {
        close(writefp);
        writefp = -1;
    }

    if (readfp != -1) 
    {
        close(readfp);
        readfp = -1;
    }

    if (sockfd != -1) 
    {
        shutdown(sockfd,SHUT_RDWR);
        close(sockfd);
        sockfd = -1;
    }

    if (openConSock != -1) 
    {
        shutdown(openConSock,SHUT_RDWR);
        close(openConSock);
        openConSock = -1;
    }

    syslog(LOG_INFO,"Caught signal, exiting");

    closelog();
    remove(DATA_PATH);
}

void exitSigHandler(int signo) 
{
    if (signo == SIGINT || signo == SIGTERM) 
    {
        cleanUp();
        exit(0);
    }
}

// main program that get's command line arguments
int main( int argc, char* argv[]) {

     
    for(int i = 1; i < argc; i++)
    {
        int option;
        if((option = getopt(argc, argv, "d")) != 1)
        {
            //printf("arg =%d", option);
            if(option == 'd')
            {
                runAsDaemon = true;
            }
        }

    }


    openlog("NB aesdSocket", LOG_PID | LOG_CONS, LOG_USER);
    signal(SIGINT, exitSigHandler);
    signal(SIGTERM, exitSigHandler);


    // setup from https://man7.org/linux/man-pages/man3/getaddrinfo.3.html and tips from lecture
    struct addrinfo hints = {0};
    struct addrinfo *res = NULL;

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    hints.ai_protocol = 0;

    if(getaddrinfo(NULL, PORT_NUM, &hints, &res) != 0)
    {
        syslog(LOG_ERR,"failed to use getaddrinfo");
        return -1;
    }

    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd == -1) 
    {
        syslog(LOG_ERR,"socket fd failed to get opened");
        return -1;
    }
    
    int op = 1;
    if(setsockopt(sockfd, SOL_SOCKET,SO_REUSEADDR,&op,sizeof(int)) == -1)
    {
        syslog(LOG_ERR,"option set failed");
        return -1;
    }

    if (bind(sockfd, res->ai_addr, res->ai_addrlen) == -1) 
    {
        syslog(LOG_ERR,"bind failed");
        return -1;
    }

    if (listen(sockfd, 10) == -1) 
    {
        syslog(LOG_ERR,"Not listening"); 
        return -1;
    }

    freeaddrinfo(res);
    res = NULL;


    //printf("Socket bound to port %s\n", PORT_NUM);


    // Skeleton gotten and adapted from Bing Chat gpt 4 "how to create a daemon process!"
    if(runAsDaemon)
    {
        pid_t pid = fork();

        if(pid < 0)
        {
            syslog(LOG_ERR,"Fork fail");
        }

        if(pid > 0)
        {
            syslog(LOG_USER,"Exit parent");
            exit(0);
        }

        if(setsid() == -1)
        {
            syslog(LOG_ERR,"failed to set sid for child remaining");
        }

        // close all paths to run in background as a daemon
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);

        
        if(chdir("/") == -1)
        {
            syslog(LOG_ERR,"failed to chdir to /");
        }
        
        umask(0);

    }
    

    struct sockaddr_storage client_addr;
    socklen_t addr_size = sizeof(client_addr);
    char buffer[BUFFER_SIZE];
    char sendBuffer[BUFFER_SIZE];

    while (1) 
    {
        
        openConSock = accept(sockfd, (struct sockaddr*)&client_addr, &addr_size);

        if(openConSock == -1)
        {
            syslog(LOG_ERR,"connection accept failed");
        }
        
        // Implentation is a combination of both cases of either 4 or 6!
        //https://stackoverflow.com/questions/3060950/how-to-get-ip-address-from-sock-structure-in-c

        char ipAccepted[INET6_ADDRSTRLEN]; // handle both Inet6 and 4
        void *addrPtr;

        // if INET 4 
        if(client_addr.ss_family == AF_INET)
        {
            // then save the address
            struct sockaddr_in *tempSock = (struct sockaddr_in *)&client_addr;
            addrPtr = &(tempSock->sin_addr);
        }
        else
        {
            // save the INET6 address
            struct sockaddr_in6 *tempSock = (struct sockaddr_in6 *)&client_addr;
            addrPtr = &(tempSock->sin6_addr);
        }

        // translate the pointer to a ip using the INET_NTOP function
        inet_ntop(client_addr.ss_family, addrPtr, ipAccepted, sizeof(ipAccepted));

        syslog(LOG_INFO,"Accepted connection from %s",ipAccepted);


        // open the writing file and create it if it doesn't exist!
        writefp = open(DATA_PATH,   O_RDWR | O_CREAT |  O_APPEND, 0664);

        if (writefp == -1 ) 
        {
             syslog(LOG_ERR,"Open write file failed");
            return -1;
        }

        int bytes_received = 1;
        char lastChar = ' ';

        // While buffer recieved end is not '\n'
        while (lastChar != '\n') 
        {
            // Recieve and then write recieved size to buffer size of data!
            bytes_received = recv(openConSock, buffer, BUFFER_SIZE - 1, 0);

            if(bytes_received == -1)
            {
                syslog(LOG_ERR,"Bytes not recieved");
                break;
            }

            if(write(writefp,buffer,bytes_received) == -1)
            {
                syslog(LOG_ERR,"Bytes not written");

            }
            lastChar = buffer[bytes_received - 1];
        }

        if (bytes_received == -1) 
        {
            syslog(LOG_INFO,"recv fail");
        }

        // Reset file pointer
        close(writefp);

        // Open file to be read only
        readfp = open(DATA_PATH,  O_RDONLY );
        if (readfp == -1 ) 
        {
            syslog(LOG_INFO,"open fail on read");
            return -1;
        }


        int bytes_send = 1;
        lastChar = ' ';
        // while sendBuffer's last char is not '\n'
        while (lastChar != '\n') 
        {
            // Read and send the contents of the file in buffer size or less packets
            if((bytes_send = read(readfp, sendBuffer, BUFFER_SIZE)) == -1)
            {
                syslog(LOG_ERR,"read fail");
                break;
            }

            if (send(openConSock, sendBuffer, bytes_send, 0) == -1) 
            {
                syslog(LOG_ERR,"send fail");
                break;
            }

            lastChar = sendBuffer[bytes_send - 1];
        }

        // Close connection from the ip and the socket and readfile to reset back to accept
        syslog(LOG_INFO,"Closed connection from %s",ipAccepted);
        close(readfp);
        close(openConSock);
        
    }
    

    return 0;

}
