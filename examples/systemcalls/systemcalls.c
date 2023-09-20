#include "systemcalls.h"

/**
 * @param cmd the command to execute with system()
 * @return true if the command in @param cmd was executed
 *   successfully using the system() call, false if an error occurred,
 *   either in invocation of the system() call, or if a non-zero return
 *   value was returned by the command issued in @param cmd.
*/
bool do_system(const char *cmd)
{

    int retCode;
    bool retVal = true;
    retCode = system(cmd);
    
    if (retCode)
    {
    	retVal = false;
    }

    return retVal;
}

/**
* @param count -The numbers of variables passed to the function. The variables are command to execute.
*   followed by arguments to pass to the command
*   Since exec() does not perform path expansion, the command to execute needs
*   to be an absolute path.
* @param ... - A list of 1 or more arguments after the @param count argument.
*   The first is always the full path to the command to execute with execv()
*   The remaining arguments are a list of arguments to pass to the command in execv()
* @return true if the command @param ... with arguments @param arguments were executed successfully
*   using the execv() call, false if an error occurred, either in invocation of the
*   fork, waitpid, or execv() command, or if a non-zero return value was returned
*   by the command issued in @param arguments with the specified arguments.
*/

bool do_exec(int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
    // this line is to avoid a compile warning before your implementation is complete
    // and may be removed
    command[count] = command[count];

    // This solution is rewritten and expanded version of the code from page 161 of the linux system programming book

    int status;
    pid_t processID;
    
    openlog("assignment3-part1",LOG_PID, LOG_USER); 
    processID = fork();
    
    if(processID==-1) // process could not fork
    {
    	syslog(LOG_ERR,"Could not fork! Returning false\r\n");
    	
    	closelog();
        return false;
    }
    else if (processID == 0) // process child fork
    {
        // save path at command[0] and try execv
        const char* path = command[0];
        execv(path, command);
        
        // and if it returns it failed so log...
        syslog(LOG_ERR,"Could execv command with path: %s! Exiting with Failure\r\n",path);
        exit(EXIT_FAILURE);
        
        
    }
    
    // check if child process failed or stopped and save the status
    if(waitpid(processID, &status, 0) == -1) // if the wait fails return false...
    {
    	syslog(LOG_ERR,"waitpid returned -1 Returning false\r\n");
    	closelog();
    	return false;
    }
    else if(WIFEXITED(status)) // else check if the the status indicated a child process exited
    {
    	syslog(LOG_WARNING,"WIFEXITED was true with a status of %d",status);
        // if it did...
        if(WEXITSTATUS(status)!= 0) // check if that status is non-zero...
        {
            syslog(LOG_ERR,"WEXITSTATUS was non-zero with a status of %d so returning false\r\n",status);
            closelog();
            // return false because it was a non zero status
            return false;
        }

    }
    
    // return true because the process did not exit so it must have succeeded
    va_end(args);
    closelog();
    return true;
}

/**
* @param outputfile - The full path to the file to write with command output.
*   This file will be closed at completion of the function call.
* All other parameters, see do_exec above
*/
bool do_exec_redirect(const char *outputfile, int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
    // this line is to avoid a compile warning before your implementation is complete
    // and may be removed
    command[count] = command[count];



    // This solution is rewritten and expanded version of the code suggested to us https://stackoverflow.com/a/13784315/1446624

    int childPid;
     openlog("assignment3-part1",LOG_PID, LOG_USER); 
    
    int fileDesc = open(outputfile, O_WRONLY|O_TRUNC|O_CREAT, 0644);
    
    if (fileDesc < 0) 
    { 
        syslog(LOG_ERR,"Could not open file! Returning false\r\n"); 
        closelog();
        return false; 
    }
    
    switch (childPid = fork()) 
    {
        case -1: 
            syslog(LOG_ERR,"Could not fork! Returning false\r\n"); 
            closelog(); 
            return false;
        case 0:
            if (dup2(fileDesc, 1) < 0) //if dup2 does not set the filedesc to be the new stdoutput fd fails...
            { 
                syslog(LOG_ERR,"Could not set filedesc to stdout! Returning false\r\n"); 
                closelog();
                close(fileDesc); 
                return false; 
            }
            
            close(fileDesc); // cleanup and close fileDesc
            
            const char* path = command[0];
            
            execv(path, command);
            
             
            syslog(LOG_ERR,"Could execv command with path: %s! Exiting with Failure\r\n",path);
            exit(EXIT_FAILURE);
        default:
	    close(fileDesc);
	    int status;
	    
	    // check if child process failed or stopped and save the status
            if(waitpid(childPid, &status, 0) == -1) // if the wait fails return false...
            {
    	        syslog(LOG_ERR,"waitpid returned -1 Returning false\r\n");
    	        closelog();
    	        return false;
            }
            else if(WIFEXITED(status)) // else check if the the status indicated a child process exited
           {
    	        syslog(LOG_WARNING,"WIFEXITED was true with a status of %d",status);
                // if it did...
                if(WEXITSTATUS(status)!= 0) // check if that status is non-zero...
                {
                    syslog(LOG_ERR,"WEXITSTATUS was non-zero with a status of %d so returning false\r\n",status);
                    closelog();
                    // return false because it was a non zero status
                    return false;
                }

            }
            
    }// end of switch
    
    va_end(args);
    closelog();
    return true;
}
