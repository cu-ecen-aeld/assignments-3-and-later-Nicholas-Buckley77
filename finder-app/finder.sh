#!/bin/sh
#Assignment 1 AESD Nicholas Buckley finder script 
filesdir=$1

searchstr=$2

#Check argument count...
if [ $# -lt 2 ]
then #if it's less than required exit error 1 and print

    echo "$# is not enough arguments."
    echo "Please specify a path and a search string to find!"
    exit 1
    
else # Number of arguments are valid! (or greater)... 

    # so check validity of path
    $(find "$filesdir" &> "/dev/null" ) 
    
    #if path was not valid...
    if [ $? -gt 0 ]
    then
        echo "Path is invalid please try again!"
        exit 1
    
    else # count files and strings in directory and files respectively 
    	fileCount=$(find "$filesdir" -type f | wc -l)
    	stringCount=$(grep -r "$searchstr" "$filesdir" | wc -l )
    fi
    
    echo "The number of files are $fileCount and the number of matching lines are $stringCount"
fi

exit 0

