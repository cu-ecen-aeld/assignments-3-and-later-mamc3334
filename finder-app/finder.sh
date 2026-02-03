#!/bin/sh

# Accepts the following runtime arguments: 
    # the first argument is a path to a directory on the filesystem, referred to below as filesdir; 
    # the second argument is a text string which will be searched within these files, referred to below as searchstr
# Exits with return value 1 error and print statements if any of the parameters above were not specified
# Exits with return value 1 error and print statements if filesdir does not represent a directory on the filesystem
# Prints a message "The number of files are X and the number of matching lines are Y" 
    # where X is the number of files in the directory and all subdirectories and Y is the number of matching lines found in respective files, 
    # where a matching line refers to a line which contains searchstr (and may also contain additional content).

set -e
set -u

FILESDIR=""
SEARCHSTR=""

usage() {
    echo "
    The script \"$0\" allows users to determine the number of files in the specified 
    directory and all subdirectories and the number of matching lines in those files.
    
    Usage: $0 <path to directory> <search string>
    where:
        <path to directory> is the path to the directory to search
        <search string> is the string to search for within the files
    "

    exit 1
}

finder() {
    num_files=$(find "$FILESDIR" -type f | wc -l)
    num_matching_lines=$(grep -r "$SEARCHSTR" "$FILESDIR" | wc -l)

    echo "The number of files are $num_files and the number of matching lines are $num_matching_lines"
}

main() {
    if [ $# -ne 2 ]; then
        echo "Error: Two parameters required - path to directory and search string"
        usage
        exit 1
    fi

    FILESDIR=$1
    SEARCHSTR=$2

    if [ ! -d "$FILESDIR" ]; then
        echo "Error: $FILESDIR is not a valid directory"
        usage
        exit 1
    fi

    finder
}

main "$@"

