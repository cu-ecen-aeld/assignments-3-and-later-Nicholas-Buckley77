// Nicholas Buckley AESDsocket socket base program!

// change found on stack overflow to allow netdb import to work! (It means to use POSIX 2004 standard defs)
#define _XOPEN_SOURCE 600
#define _XOPEN_SOURCE_EXTENDED 1

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

// assignment 6
#include <sys/queue.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>



#define BUFFER_SIZE 1024
#define DATA_PATH ("/var/tmp/aesdsocketdata")
#define PORT_NUM ("9000")


// addrinfo structure for reference gotten from lecture
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

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t trasactionMutex = PTHREAD_MUTEX_INITIALIZER;

// Setup global file pointers and runAsDaemon
int sockfd;
int openConSock;
int writefp;
int readfp;



bool runAsDaemon = false;
bool cleanUpTime = false;

char buffer[BUFFER_SIZE];
char sendBuffer[BUFFER_SIZE];

struct sockaddr_storage client_addr;
socklen_t addr_size = sizeof(client_addr);

// This macro creates the data type for the head of the queue
// for nodes of type 'struct node'
SLIST_HEAD(head_s, node) head;

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
    //free(&head);

    syslog(LOG_INFO,"Caught signal, exiting");

    closelog();
    remove(DATA_PATH);
}

void exitSigHandler(int signo) 
{
    if (signo == SIGINT || signo == SIGTERM) 
    {
        // Setup for main to cleanup!
        cleanUpTime = true;
    }
}

void timeStamp (int signo)
{
    // chat gpt "This program appends a timestamp in the form “timestamp:time” to the /var/tmp/aesdsocketdata 
    // file every 10 seconds using the RFC 2822 compliant strftime format. The string includes the year, month, 
    // day, hour (in 24 hour format), minute, and second representing the system wall clock time."

    if(signo == SIGALRM)
    {
        time_t t = time(NULL);
        struct tm *tm = localtime(&t);
        char timestamp[128] ;
        int timeStampfp;
        strftime(timestamp, sizeof(timestamp), "timestamp:%a, %d %b %Y %T %z\n", tm);
        printf("%s",timestamp);
        pthread_mutex_lock(&trasactionMutex);
        timeStampfp = open(DATA_PATH,   O_RDWR | O_CREAT |  O_APPEND, 0664);
        if(write(timeStampfp,timestamp,sizeof(timestamp)) == -1)
        {
            printf("FAILED TO WRITE TIME STAMP");
        }     
        close(timeStampfp);
        pthread_mutex_unlock(&trasactionMutex);

    }


}


// chat gpt "Can you show me creating a thread and adding it to a linked list queue to run"


// The data type for the node
struct node
{   
    pthread_t id;
    int socketFd;
    bool transComplete;
    // This macro does the magic to point to other nodes
    SLIST_ENTRY(node) nodes;
};

/**
 * This structure should be dynamically allocated and passed as
 * an argument to your thread using pthread_create.
 * It should be returned by your thread so it can be freed by
 * the joiner thread.
 */
struct thread_data{
    
    // This macro does the magic to point to other nodes
    struct node *thisNode;
};





// Initialize the head before use


//https://blog.taborkelly.net/programming/c/2016/01/09/sys-queue-example.html
void *add_to_queue_and_send(void *arg) {
    struct thread_data* addThread = (struct thread_data *)arg;
    pthread_mutex_lock(&mutex);
    struct node * e = addThread->thisNode;
    
    e->id = addThread->thisNode->id;
    e->socketFd = addThread->thisNode->socketFd;
    e->transComplete = false;
    
    SLIST_INSERT_HEAD(&head, e, nodes);
    
    
    pthread_mutex_unlock(&mutex);


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
        exit(EXIT_FAILURE);
    }

    int bytes_received = 1;
    char lastChar = ' ';

    // While buffer recieved end is not '\n'
    while (lastChar != '\n') 
    {
        // Recieve and then write recieved size to buffer size of data!
        bytes_received = recv(e->socketFd, buffer, BUFFER_SIZE - 1, 0);

        if(bytes_received == -1)
        {
            syslog(LOG_ERR,"Bytes not recieved");
            break;
        }

        pthread_mutex_lock(&trasactionMutex);

        if(write(writefp,buffer,bytes_received) == -1)
        {
            syslog(LOG_ERR,"Bytes not written");
            break;
        }
        lastChar = buffer[bytes_received - 1];
    }
    pthread_mutex_unlock(&trasactionMutex);

    


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
        exit(EXIT_FAILURE);
    }


    int bytes_send = 1;
    lastChar = ' ';
    // while sendBuffer's last char is not '\n'
    while (lastChar != '\n') 
    {
        //pthread_mutex_lock(&trasactionMutex);

        // Read and send the contents of the file in buffer size or less packets
        if((bytes_send = read(readfp, sendBuffer, BUFFER_SIZE)) == -1)
        {
            syslog(LOG_ERR,"read fail");
            break;
        }
        //pthread_mutex_unlock(&trasactionMutex);
        printf("Sending %s\r\n",sendBuffer);
        if (send(e->socketFd, sendBuffer, bytes_send, 0) == -1) 
        {
            syslog(LOG_ERR,"send fail");
            break;
        }

        lastChar = sendBuffer[bytes_send - 1];
    }
    


    // Close connection from the ip and the socket and readfile to reset back to accept
    syslog(LOG_INFO,"Closed connection from %s",ipAccepted);
    printf("Closed connection from %s\r\n",ipAccepted);
    close(readfp);
    close(e->socketFd);
    e->transComplete = true;
    e = NULL;
    addThread = NULL;

    printf("WOWOWOW EXIT THREAD\r\n");

    return NULL;
}




// main program that get's command line arguments
int main( int argc, char* argv[]) {

    // fixed the implementation of option    
    int option;
    while((option = getopt(argc, argv, "d")) != -1)
    {
        //printf("arg =%d", option);
        if(option == 'd')
        {
            runAsDaemon = true;
        }
    }


    


    openlog("NB aesdSocket", LOG_PID | LOG_CONS, LOG_USER);

    // register signals to be handled by exitSigHandler!
    signal(SIGINT, exitSigHandler);
    signal(SIGTERM, exitSigHandler);
    // assignment 6
    signal(SIGALRM,timeStamp);
    alarm(10);
    
    /*timer_t timeStamper;
    struct sigevent timer;
    memset(&timer, 0, sizeof(struct sigevent));
    timer.sigev_notify = SIGEV_SIGNAL;
    timer.sigev_signo = SIGALRM;
    timer_create(CLOCK_REALTIME,&timer,&timeStamper);

    struct itimerval itimer;
    itimer.it_value.tv_sec = 10;
    itimer.it_value.tv_usec = 0;
    itimer.it_interval.tv_sec = 10;
    itimer.it_interval.tv_usec = 0;

    if (setitimer(ITIMER_REAL, &itimer, NULL) == -1) {
        printf("Error setting timer.\n");
        return 1;
    }*/
    
    
    SLIST_INIT(&head);


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
    
    // From hint from lecture and added to fix issue of sockets stepping on eachother
    // SOL_SOCKET is at the socket level, allow reuse of binding local addresses, option enabled, sizeof int
    int op = 1;
    if(setsockopt(sockfd, SOL_SOCKET,SO_REUSEADDR,&op,sizeof(int)) == -1)
    {
        syslog(LOG_ERR,"option set failed");
        return -1;
    }

    // socket file desc, assign address based on the filedec of the given socket, size of address
    if (bind(sockfd, res->ai_addr, res->ai_addrlen) == -1) 
    {
        syslog(LOG_ERR,"bind failed");
        return -1;
    }

    // turn socket into a passive socket to accept connections, 10 connections in queue
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
    


    
    while (!cleanUpTime) 
    {
        
        openConSock = accept(sockfd, (struct sockaddr*)&client_addr, &addr_size);

        
        if(openConSock == -1)
        {
            syslog(LOG_ERR,"connection accept failed");
        }

        /*
        //https://stackoverflow.com/questions/5392813/accept-multiple-subsequent-connections-to-socket
        pid_t accepter = fork();

        if(!accepter) // if I am the child
        {
            close(sockfd); // close the listener

            exit(0);
        }*/
        struct thread_data *newThread = (struct thread_data*)malloc(sizeof(struct thread_data));
        newThread->thisNode = (struct node*)malloc(sizeof(struct node));
    
        if(newThread == NULL || newThread->thisNode == NULL)
        {
            printf("wow");
        }
        
        newThread->thisNode->socketFd = openConSock;
        
        newThread->thisNode->id = 0;

        pthread_create(&(newThread->thisNode->id), NULL, add_to_queue_and_send, (void*)newThread);
        pthread_mutex_lock(&mutex);
        printf("after main lock...");

        struct node * e  = NULL;
        SLIST_FOREACH(e, &head, nodes)
        {

            printf("ID = %ld",(e->id));
            if(e->transComplete)
            {
                SLIST_REMOVE(&head,e,node,nodes);
                pthread_join(e->id, NULL);
                free(e);
            }
        }
        pthread_mutex_unlock(&mutex);
    }
    
    cleanUp();
    exit(0);
    return 0;

}
