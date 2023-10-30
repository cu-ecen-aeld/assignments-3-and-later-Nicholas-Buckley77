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
#define PORT_NUM ("9000")
#define DELAY_TO_STAMP (10)
#define USE_AESD_CHAR_DEVICE (1)

#if(USE_AESD_CHAR_DEVICE)

    #define DATA_PATH ("/var/tmp/aesdchar")
#else
    #define DATA_PATH ("/var/tmp/aesdsocketdata")
#endif


pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t trasactionMutex = PTHREAD_MUTEX_INITIALIZER;

// copied from freebsd
#define	SLIST_FOREACH_SAFE(var, head, field, tvar)			\
	for ((var) = SLIST_FIRST((head));				\
	    (var) && ((tvar) = SLIST_NEXT((var), field), 1);		\
	    (var) = (tvar))

// Setup global file pointers and runAsDaemon
int sockfd;
int writefp;
int readfp;


bool runAsDaemon = false;
bool cleanUpTime = false;

char buffer[BUFFER_SIZE];
char sendBuffer[BUFFER_SIZE];



struct sockaddr_storage client_addr;
socklen_t addr_size = sizeof(client_addr);

pthread_t timerStampThread;


// The data type for the node
struct node
{   
    pthread_t id;
    int socketFd;
    bool transComplete;
    // This macro does the magic to point to other nodes
    SLIST_ENTRY(node) nodes;
};

struct node* thisNode;


// This macro creates the data type for the head of the queue
// for nodes of type 'struct node'
SLIST_HEAD(head_s, node) head;

/* simple clean routine called by signal handler for SIGINT and SIGTERM to close all 
*  file pointers and sockets, remove the file writen to and from and close the logging
*  this function was and sig handler skeletons were gotten from bing chat gpt
*  "Gracefully exits when SIGINT or SIGTERM is received, completing any open connection operations,
*  closing any open sockets, and deleting the file /var/tmp/aesdsocketdata."
*/
void cleanUp(int exitVal)
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

    addr_size = 0;

    struct node * e ;
    while(!SLIST_EMPTY(&head))
    {
        e = SLIST_FIRST(&head);
        SLIST_REMOVE(&head,e,node,nodes);
        pthread_join(e->id, NULL);
        shutdown(e->socketFd,SHUT_RDWR);
        close(e->socketFd);
        e->socketFd = -1;
        free(e);
        
    }

    pthread_mutex_destroy(&mutex);
    pthread_mutex_destroy(&trasactionMutex);
    

    free(thisNode);
    

    syslog(LOG_INFO,"Caught signal or failed, exiting");

    closelog();

#if(USE_AESD_CHAR_DEVICE)
    syslog(LOG_INFO,"Not removing datapath!");
#else
    remove(DATA_PATH);
#endif
    exit(exitVal);
}

void exitSigHandler(int signo) 
{
    if (signo == SIGINT || signo == SIGTERM) 
    {
        // Setup for main to cleanup!
        cleanUpTime = true;
        //cleanUp(EXIT_SUCCESS); // not totally safe but passes for now... The problem is cleaningup time stamper quickly for the test
    }
}

void *timeStamper () 
{
    // chat gpt "This program appends a timestamp in the form “timestamp:time” to the /var/tmp/aesdsocketdata 
    // file every 10 seconds using the RFC 2822 compliant strftime format. The string includes the year, month, 
    // day, hour (in 24 hour format), minute, and second representing the system wall clock time."
    while(!cleanUpTime)
    {
        char timestamp[128] ;
        sleep(DELAY_TO_STAMP);

        pthread_mutex_lock(&trasactionMutex);
        time_t t = time(NULL);
        struct tm *tm = localtime(&t);
        int timeStampfp;
        strftime(timestamp, sizeof(timestamp), "timestamp: %Y-%m-%d %H:%M:%S\n", tm);
        timeStampfp = open(DATA_PATH,   O_RDWR | O_CREAT |  O_APPEND, 0664);
        if(write(timeStampfp,timestamp,sizeof(timestamp)) == -1)
        {
            syslog(LOG_INFO,"Failed to write timeStamp");
            cleanUp(EXIT_FAILURE);
        }     
        close(timeStampfp);
        tm = NULL;
        pthread_mutex_unlock(&trasactionMutex);

    }
    cleanUp(EXIT_SUCCESS);
    return NULL;
}






//https://blog.taborkelly.net/programming/c/2016/01/09/sys-queue-example.html
void *add_to_queue_and_send(void *arg) {
    struct node* addedNode = (struct node *)arg;
    pthread_mutex_lock(&mutex);
    
    
    SLIST_INSERT_HEAD(&head, addedNode, nodes);
    
    
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

    

    
    int bytes_received ;

    bool recieving = true;
    pthread_mutex_lock(&trasactionMutex);
    // open the writing file and create it if it doesn't exist!
    writefp = open(DATA_PATH, O_RDWR | O_CREAT |  O_APPEND, 0664);

    if (writefp == -1 ) 
    {
        syslog(LOG_ERR,"Open write file failed");
        cleanUp(EXIT_FAILURE);
    }

    // While Recieving bytes and then write recieved size to buffer size of data!
    while (recieving) 
    {
        bytes_received = recv(addedNode->socketFd, buffer, BUFFER_SIZE - 1, 0);
        if(bytes_received == -1)
        {
            syslog(LOG_ERR,"Failed to recv bytes");
            cleanUp(EXIT_FAILURE);
        }
        else if(bytes_received == 0)
        {
            recieving = false;
        }
        else
        {
        
        
            if(write(writefp,buffer,bytes_received) == -1)
            {
                syslog(LOG_ERR,"Bytes not written");
                cleanUp(EXIT_FAILURE);
            }

            // if there was a \n written to the file...
            if(strchr(buffer,'\n') != NULL)
            {
                recieving = false;
            }
        }
    }
    // Reset file pointer
    close(writefp);
    //pthread_mutex_unlock(&trasactionMutex);

        



    //pthread_mutex_lock(&trasactionMutex);

    // Open file to be read only
    readfp = open(DATA_PATH,  O_RDONLY );
    if (readfp == -1 ) 
    {
        syslog(LOG_INFO,"open fail on read");
        cleanUp(EXIT_FAILURE);
    }

    int bytes_send;
    // Send file data until there's nothing left
    while ((bytes_send = read(readfp, sendBuffer, BUFFER_SIZE )) > 0) 
    {
        
        if (send(addedNode->socketFd, sendBuffer, bytes_send, 0) == -1) 
        {
            syslog(LOG_ERR,"send fail");
            cleanUp(EXIT_FAILURE);

        }

    }
    close(readfp);
    pthread_mutex_unlock(&trasactionMutex);

    // Close connection from the ip and the socket and readfile to reset back to accept
    syslog(LOG_INFO,"Closed connection from %s",ipAccepted);


    close(addedNode->socketFd);
    addedNode->transComplete = true;

    return NULL;
}




// main program that get's command line arguments
int main( int argc, char* argv[]) {

    // fixed the implementation of option  
    if(argc > 0)
    {
        int option;
        while((option = getopt(argc, argv, "d")) != -1)
        {
            //printf("arg =%d", option);
            if(option == 'd')
            {
                runAsDaemon = true;
            }
        }
    }  
    

    openlog("NB aesdSocket", LOG_PID | LOG_CONS, LOG_USER);

    // register signals to be handled by exitSigHandler!
    signal(SIGINT, exitSigHandler);
    signal(SIGTERM, exitSigHandler);
    // assignment 6
    
    
    SLIST_INIT(&head);


    // setup from https://man7.org/linux/man-pages/man3/getaddrinfo.3.html and tips from lecture
    struct addrinfo hints = {0};
    struct addrinfo *res = NULL;

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;


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


    // Skeleton gotten and adapted from Bing Chat gpt 4 "how to create a daemon process!"
    if(runAsDaemon)
    {
        pid_t pid = fork();

        if(pid < 0)
        {
            syslog(LOG_ERR,"Fork fail");
            cleanUp(EXIT_FAILURE);

        }

        if(pid > 0)
        {
            syslog(LOG_USER,"Exit parent");
            exit(EXIT_SUCCESS);
        }

        if(setsid() == -1)
        {
            syslog(LOG_ERR,"failed to set sid for child remaining");
            cleanUp(EXIT_FAILURE);

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

#if(USE_AESD_CHAR_DEVICE)
#else
    pthread_create(&timerStampThread,NULL, timeStamper, NULL);
#endif

    
    while (!cleanUpTime) 
    {
        thisNode = (struct node*)malloc(sizeof(struct node));
    
        if( thisNode == NULL)
        {
            syslog(LOG_ERR,"Node failed to malloc");
            cleanUp(EXIT_FAILURE);
        }
        
        thisNode->transComplete = false;
        
        thisNode->socketFd = accept(sockfd, (struct sockaddr*)&client_addr, &addr_size);

        
        if(thisNode->socketFd == -1)
        {
            syslog(LOG_ERR,"connection accept failed");
            cleanUp(EXIT_FAILURE);
        }


        pthread_create(&(thisNode->id), NULL, add_to_queue_and_send, (void*)thisNode);
        pthread_mutex_lock(&mutex);
        thisNode = NULL;

        struct node * e ;
        struct node * safe ;
        if(!SLIST_EMPTY(&head))
        {
            SLIST_FOREACH_SAFE(e, &head, nodes,safe)
            {

                if(e->transComplete)
                {
                    SLIST_REMOVE(&head,e,node,nodes);
                    pthread_join(e->id, NULL);
                    shutdown(e->socketFd,SHUT_RDWR);
                    close(e->socketFd);
                    free(e);
                }
            }
        }
        
        pthread_mutex_unlock(&mutex);
    }
    
    cleanUp(EXIT_SUCCESS);
    return 0;

}
