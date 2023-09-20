// Nicholas Buckley AESF writer.c file for creating files and writing text to them!

#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <string.h>

// Code skeleton made from chat GPT-4 "a c program that takes a filename and a text string and makes the file with the string as it's contents also setting up and using syslog with LOG_DEBUG"

// Function to create file and write a string to it
int createFileWithString(char *filename, char *text) {

    // make file pointer to write to
    FILE *fp;
    fp = fopen(filename, "w");
    
    
    if (fp == NULL) {
        syslog(LOG_ERR, "Error opening file %s", filename);
        return 1;
    }
    
    // print text to file
    fputs(text, fp);
    fclose(fp);
    return 0;
}

// main program that get's command line arguments
int main(int argc, char* argv[]) {
    
    if(argc != 3)
    {
    	syslog(LOG_ERR, "%d is the incorrect number of arguments, needs a filename and string to be written to the file", argc);
    	return 1;
    
    }
    
    // setup system log with PID and CONS and at priorty USER
    openlog("NB Writer", LOG_PID | LOG_CONS, LOG_USER);
    
    // log the described debug message
    syslog(LOG_DEBUG, "Writing %s to %s where %s is a text string written to the file (second argument) and %s is the file created by the script.",argv[2], argv[1], argv[2], argv[1] );
    
    int returnVal = createFileWithString(argv[1], argv[2]);
    
    // close and cleanup logging
    closelog();
    return returnVal;
}
