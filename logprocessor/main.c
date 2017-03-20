    //
//  main.c
//  logprocessor
//
//  Created by Peter Söderlind on 2017-01-24.
//
//

#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>






int process_files(char *name) {
    char logfilename[256], proclogfilename[256], finishedlogfilename[256];
    int res;
    
    strcpy(logfilename, "logs/");
    strcat(logfilename, name);
    
    strcpy(proclogfilename, "processing/");
    strcat(proclogfilename, name);
    
    strcpy(finishedlogfilename, "finished/");
    strcat(finishedlogfilename, name);
    
    printf("Moving file %s to %s\n", logfilename, proclogfilename);
    res = rename(logfilename, proclogfilename);

    printf("Doing processing on file %s\n", proclogfilename);
    printf("Moving file %s to %s\n", proclogfilename, finishedlogfilename);
    res = rename(proclogfilename, finishedlogfilename);
    return(0);
}


int main(int argc, const char * argv[]) {

    DIR *dirp;
    struct dirent *dirent;
    struct stat sdata;
    char buf[256];
    
    dirp = opendir("logs");
    if(dirp == NULL) {
        perror("Can't open logdirectory!");
        return(-1);
    }
    
    while(1) {
        
        printf("Checking directory: logs!\n");
        
        while( (dirent = readdir(dirp))) {
            if(dirent->d_name[0] == '.') continue;
            if(strncmp("csplog", dirent->d_name, 6)) continue;
            strcpy(buf, "logs/");
            strcat(buf, dirent->d_name);
            stat(buf, &sdata);
            printf("mtime: %ld %ld\n", sdata.st_mtimespec.tv_sec, sdata.st_mtimespec.tv_nsec);
            process_files(dirent->d_name);
        }
        
        rewinddir(dirp);
        sleep(3);
    }

    //printf("Hello, World!\n");
    return 0;
}



