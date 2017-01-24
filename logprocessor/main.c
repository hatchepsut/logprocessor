    //
//  main.c
//  logprocessor
//
//  Created by Peter Söderlind on 2017-01-24.
//
//

#include <stdio.h>
#include <string.h>
#include <dirent.h>

int process_file(char *name) {
    char logfilename[256], newlogfilename[256];
    int res;
    
    strcpy(logfilename, "logs/");
    strcat(logfilename, name);
    
    strcpy(newlogfilename, "processed/");
    strcat(newlogfilename, name);
    
    
    
    
    
    //printf("Move %s to %s\n", logfilename, newlogfilename);
    res = rename(logfilename, newlogfilename);
    //perror("rename");
    return(0);
}


int main(int argc, const char * argv[]) {

    DIR *dirp;
    struct dirent *dirent;
    
    dirp = opendir("logs");
    if(dirp == NULL) {
        perror("Can't open logdirectory!");
        return(-1);
    }
    
    while( (dirent = readdir(dirp))) {
        if(dirent->d_name[0] == '.') continue;
        process_file(dirent->d_name);
    }

    // insert code here...
    //printf("Hello, World!\n");
    return 0;
}



