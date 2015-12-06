//
//  main.c
//  finalClient
//
//  Created by Or Ben David on 11/22/15.
//  Copyright © 2015 Or Ben David. All rights reserved.
//

//
//  Client.c
//  ClientSide
//
//  Created by Or Ben David on 11/15/15.
//  Copyright © 2015 Or Ben David. All rights reserved.
//

//
//  Client.c
//  SelectServer
//
//  Created by Or Ben David on 11/15/15.
//  Copyright © 2015 Or Ben David. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <dirent.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>




#define PORT "9034" // the port client will be connecting to

#define N 256 // max number of bytes we can get at once
#define M 1024 //max path & command size
#define LS 1
#define DOWNLOAD 2
#define UPLOAD 3
#define UPLOADCMP 4
#define GET_OUT 5
#define CHAT 6



void printManual()
{
    printf(" ===================================================================================\n");
    printf("||                      you have connected to the server                           ||\n");
    printf("||                                                                                 ||\n");
    printf("||                           client/server practice                                ||\n");
    printf("||                           ======================                                ||\n");
    printf("||                                                                                 ||\n");
    printf("||                    your may use the following commands:                         ||\n");
    printf("||                                                                                 ||\n");
    printf("||                        ls, download/upload + 'file'                             ||\n");
    printf("||                                                                                 ||\n");
    printf("||                                                                                 ||\n");
    printf("||                                                                                 ||\n");
    printf("||  *                  ls: display the files in the server                         ||\n");
    printf("||                                                                                 ||\n");
    printf("||  *      download + filename: download the specific file from the server         ||\n");
    printf("||                                                                                 ||\n");
    printf("||  *      upload + filename: upload the file from the client to the server        ||\n");
    printf("||                                                                                 ||\n");
    printf("||  * uploadcmp + filename: compress the specific file and send it to the serve    ||\n");
    printf("||                                                                                 ||\n");
    printf("||                                                                                 ||\n");
    printf("||                                                                                 ||\n");
    printf(" ===================================================================================\n");

}

void recveLs(int sockfd)
{
    int filesNum = 0;
    int cmd = LS;
    char fileName[M] = {0};
    if (send(sockfd, &cmd, sizeof(int), 0) < 0)
    {
        printf("failed to send command ls\nerrno says: %s\n", strerror(errno));
        exit(1);
    }
    if (recv(sockfd, &filesNum, sizeof(int), 0) == -1)//server returns the number of files
    {
        printf("faild to receive from server\n errno: %s\n", strerror(errno));
        exit(1);
    }
    printf("Server's content(%d files):\n", filesNum);
    for (int i = 0; i < filesNum; i++)
    {
        bzero(fileName, M);
        int size = 0;
        if ((recv(sockfd, &size, sizeof(int), 0)) < 0)
        {
            printf("fail to get lsFileName size\nerrno says: %s\n", strerror(errno));
            exit(1);
        }
        if ((recv(sockfd, fileName, size, 0)) < 0)
        {
            printf("faild to get list from server\n errno: %s\n", strerror(errno));
            exit(1) ;
        }//server returns file name
        printf(" - %s\n", fileName);
    }
}

int compress(char *filename)
{
    int pipes[2];
    
    if (pipe(pipes) < 0)
    {
        return -1;
    }
    
    // fcntl(pipes[1], F_SETFD, O_NDELAY);
    int pid;
    
    if ((pid = fork()) < 0)
    {
        return -1;
    }
    
    if (pid == 0) {
        
        close(pipes[0]);
        dup2(pipes[1], 1);
        close(pipes[1]);
        
        char *args[4];
        args[0] = "/usr/bin/compress";
        args[1] = "-c";
        args[2] = strdup(filename);
        args[3] = NULL;
        
        execvp(args[0], args);
        perror("error");
        _exit(1);
    }
    else
    {
        return pipes[0];
    }
}

void uploadCompress(int sockfd, char *fileName)
{
    struct stat status;
    int toDo = UPLOADCMP;
    int fileD;
    int flag = 0;
    if ((fileD = compress(fileName)) < 0)
    {
        printf("failed open file stream to upload\nerrno says: %s\n", strerror(errno));
        return;
    }
    // fcntl(pipes[0], F_SETFD, O_NDELAY);
    if (fstat(fileD, &status) < 0)
    {
        printf("fstat failed\n");
        close(fileD);
        return;
    }
    // flag = fcntl(fileD, F_GETFL, 0);
    // fcntl(fileD, F_SETFL, flag | O_NONBLOCK);
    // flag = 0;
    if (send(sockfd, &toDo, sizeof(int), 0) < 0)
    {
        printf("failed sending upload command\nerrno says: %s", strerror(errno));
        close(fileD);
        exit(1);
    }
    int stringSize = sizeof(fileName);
    if (fileName[stringSize] != 'Z')
    {
        strcat(fileName, ".Z");
        stringSize = sizeof(fileName);
    }
    if (send(sockfd, &stringSize, sizeof(int), 0) < 0)
    {
        printf("failed to send string size\nerrno says: %s\n", strerror(errno));
        close(fileD);
        exit(1);
    }
    if (send(sockfd, fileName, sizeof(fileName), 0) < 0)
    {
        printf("failed sending file name with upload command\nerrno says: %s", strerror(errno));
        close(fileD);
        exit(1);
    }
    if (recv(sockfd, &flag, sizeof(int), 0) < 0)
    {
        printf("failed to get flag from server\nerrno says: %s\n", strerror(errno));
        close(fileD);
        exit(1);
    }
    if (flag)
    {
        printf("server failed preparing\n");
        close(fileD);
        return;
    }
    char buf[M] = {0};
    int bytesRead = M-1;
    while (bytesRead == M-1)//read and send bytes blocks
    {
        bytesRead = read(fileD, buf, M-1);
        if (send(sockfd, &bytesRead, sizeof(int), 0) < 0)
        {
            printf("failed to send reading size\nerrno says: %s\n", strerror(errno));
            flag = 1;
            break;
        }
//        if (errno == EAGAIN || errno == EWOULDBLOCK) printf("eagain....\n");
        if (bytesRead <= 0)
        {
            if (bytesRead < 0)
            {
                flag = 0;
                if (errno == EWOULDBLOCK)
                {
                    printf("finish reading\n");
                    flag = 1;
                    if (send(sockfd, &flag, sizeof(int), 0) < 0)
                    {
                        printf("failed sending 'finish' flag\nerrno says: %s\n", strerror(errno));
                        close(fileD);
                        exit(1);
                    }
                    close(fileD);
                    return;
                }
                printf("failed to read file\nerrno says: %s\n", strerror(errno));
                if (send(sockfd, &flag, sizeof(int), 0) < 0)
                {
                    printf("failed to send 'failed' flag\nerrno says: %s\n", strerror(errno));
                    close(fileD);
                    exit(1);
                }
            }
            close(fileD);
            return;
        }
        int counter = 0;
        int bytesSent = 0;
        while (bytesSent < bytesRead)
        {
            counter = send(sockfd, buf + bytesSent, bytesRead - bytesSent, 0);
            if (counter < 0)
            {
                printf("failed to upload file\nerrno says: %s\n", strerror(errno));
                exit(1);
            }
            bytesSent += counter;
        }
        bzero(buf, M);
    }
    close(fileD);
}

int checkFileExist(DIR* dir, char *path, char *fileName)
{
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (!strcmp(entry->d_name, fileName))
        {
            closedir(dir);
            return 0;//success
        }
    }
    closedir(dir);
    return 1;//failure
}

void uploadFile(int sockfd, char *fileName)
{
    
    int toDo = UPLOAD;
    int fileD = 0;
    int flag = 0;
    struct stat stBuf;
    if ((fileD = open(fileName, O_RDWR, S_IRWXU)) < 0)
    {
        printf("failed open file stream to upload\nerrno says: %s\n", strerror(errno));
        return;
    }
    if (stat(fileName, &stBuf) < 0)
    {
        printf("failed with file /'stat/'\nerrno says: %s\n", strerror(errno));
        close(fileD);
        return;
    }
    int fileSize = (int)stBuf.st_size;
    if (send(sockfd, &toDo, sizeof(int), 0) < 0)
    {
        printf("failed sending upload command\nerrno says: %s", strerror(errno));
        close(fileD);
        exit(1);
    }
    int stringSize = sizeof(fileName);//getting the size of the file
    if (send(sockfd, &stringSize, sizeof(int), 0) < 0)
    {
        printf("failed to send string size\nerrno says: %s\n", strerror(errno));
        close(fileD);
        exit(1);
    }
    if (send(sockfd, fileName, stringSize, 0) < 0)
    {
        printf("failed sending file name with upload command\nerrno says: %s", strerror(errno));
        close(fileD);
        exit(1);
    }
    if (send(sockfd, &fileSize, sizeof(int), 0) < 0)
    {
        printf("failed to send the file length\nerrno says: %s\n", strerror(errno));
        close(fileD);
        exit(1);
    }
    if (recv(sockfd, &flag, sizeof(int), 0) < 0)
    {
        printf("failed to receive the flag\nerrno says: %s\n", strerror(errno));
        close(fileD);
        exit(1);
    }
    if (flag)
    {
        close(fileD);
        return;
    }
    char buf[M] = {0};
    int bytesRead = 0;
    int counter = 0;
    int bytesSent = 0;
    while (1)
    {
        bytesRead = read(fileD, buf, M-1);
        if (send(sockfd, &bytesRead, sizeof(int), 0) < 0)
        {
            printf("failed to send block size\nerrno says: %s\n", strerror(errno));
            exit(1);
        }
        if (bytesRead == 0)//finish reading
        {
            break;
        }
        if (bytesRead < 0)
        {
            printf("failed read from file\nerrno says: %s\n", strerror(errno));
            close(fileD);
            exit(1);
        }
        while (bytesSent < bytesRead)
        {
            if ((counter = send(sockfd, buf + bytesSent, bytesRead - bytesSent, 0)) < 0)
            {
                printf("failed to upload file\nerrno says: %s\n", strerror(errno));
                close(fileD);
                exit(1);
            }
            bytesSent += counter;
        }
        bytesSent = 0;
        bzero(buf, M);
    }
    close(fileD);
}

void downldFile(int sockfd, char *fileName)
{
    int fileD = 0;
    int fileSize = 0;
    int flag = 0;
    int stringSize = sizeof(fileName);
    int toDo =  DOWNLOAD;
    if ((fileD = open(fileName, O_CREAT | O_RDWR, S_IRWXU))< 0)
    {
        printf("unable to create new file\nerrno saya: %s\n", strerror(errno));
        exit(1);
    }
    if ((send(sockfd, &toDo, sizeof(int), 0)) < 0)
    {
        printf("failed send download command\nerrno says: %s\n", strerror(errno));
        close(fileD);
        exit(1);
    }
    if ((send(sockfd, &stringSize, sizeof(int), 0)) < 0)
    {
        printf("failed to send string size\nerrno says: %s\n", strerror(errno));
        close(fileD);
        exit(1);
    }
    if ((send(sockfd, fileName, stringSize, 0)) < 0)
    {
        printf("failed send file name to download\nernno says: %s\n", strerror(errno));
        close(fileD);
        exit(1);
    }
    //receiving flag to know if the server could prepare hthe file
    if (recv(sockfd, &flag, sizeof(int), 0) < 0)
    {
        printf("failed to receive the flag\nerrno says: %s\n", strerror(errno));
        close(fileD);
        exit(1);
    }
    if (flag)
    {
        printf("server failed preparing the file to send\n");
        close(fileD);
        exit(1);
    }
    
    //getting the file
    if (recv(sockfd, &fileSize, sizeof(int), 0) < 0)
    {
        printf("failed getting file size\n");
        close(fileD);
        exit(1);
    }
    char buf[M] = {0};
    int totalRecv = 0;
    int blockSize = 0;
    int bytesRecv = 0;
    int counter = 0;
    int bytesWrote = 0;
    while (totalRecv < fileSize)//in any case of failure the client gonna be removed!!
    {
        if (recv(sockfd, &blockSize, sizeof(int), 0) < 0)
        {
            flag = 1;
            printf("failed to receive block size\nerrno says: %s\n", strerror(errno));
            break;
        }
        if (blockSize == 0)
        {
            break;
        }
        while (bytesRecv < blockSize)
        {
            if ((counter = recv(sockfd, buf + bytesRecv, blockSize - bytesRecv, 0)) < 0)
            {
                flag = 1;
                printf("failed to receive bytes from server\nerrno says: %s\n", strerror(errno));
                break;
            }
            bytesRecv += counter;
        }
        if (flag)
        {
            break;
        }
        totalRecv += bytesRecv;
        while (bytesWrote < bytesRecv)
        {
            if ((counter = write(fileD, buf + bytesWrote, bytesRecv - bytesWrote)) < 0)
            {
                flag = 1;
                printf("failed to write on the file\nernno says: %s\n", strerror(errno));
                break;
            }
            bytesWrote += counter;
        }
        if (flag)
        {
            break;
        }
        bytesRecv = 0;
        bytesWrote = 0;
        bzero(buf, M);
    }
    close(fileD);
    if (flag)
    {
        exit(1);
    }
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}


int main(int argc, char *argv[])
{
    int sockfd = 0;
    long numbytes;
    char buf[N];
    char command[M];
    char path[M];
    char *temp;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];
    DIR *dir;
    struct dirent *entry;
    
    if (argc != 2)
    {
        fprintf(stderr,"usage: client hostname\n");
        exit(1);
    }
    
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    
    if ((rv = getaddrinfo(argv[1], PORT, &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }
    
    // loop through all the results and connect to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next)
    {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1)
        {
            perror("client: socket");
            continue;
        }
        
        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1)
        {
            close(sockfd);
            perror("client: connect");
            continue;
        }
        
        break;
    }
    
    if (p == NULL)
    {
        fprintf(stderr, "client: failed to connect\n");
        return 2;
    }
    
    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);
    printf("client: connecting to %s\n", s);
    
    freeaddrinfo(servinfo); // all done with this structure
    
    if ((numbytes = recv(sockfd, buf, N-1, 0)) == -1)
    {
        perror("recv");
        
        exit(1);
    }
    
    buf[numbytes] = '\0';
    
    printf("client: received '%s'\n",buf);
    
    getcwd(path, N);
    dir = opendir(path);
    if (!dir)
    {
        printf("failed to reach the root folder\n");
        exit(1);
    }
    
    printManual();
    
    for(;;)
    {
        printf(">>: ");
        bzero(command, M);
        fgets(command, M, stdin);
        temp = strtok(command, " \n");
        if (!strcmp(temp, "ls"))
        {
            temp = strtok(NULL, " \n");         {
                if (temp)
                {
                    printf("illegal parameter - 'ls' can't reach to specific folder");
                    continue;
                }
                recveLs(sockfd);//receive ls from server
                continue;
            }
        }
        else if (!strcmp(temp, "download"))
        {
            temp = strtok(NULL, " \n");
            if (!temp)
            {
                printf("missing /'file name/' to download");
                continue;
            }
            downldFile(sockfd, temp);//download file from server
            continue;
        }
        else if (!strcmp(temp, "upload"))
        {
            
            temp = strtok(NULL, " \n");
            if (!temp)
            {
                printf("missing file to upload\n");
                continue;
            }
            if (checkFileExist(dir, path, temp))
            {
                printf("no such file in directory\n");
                dir = opendir(path);
                continue;
            }
            dir = opendir(path);
            uploadFile(sockfd, temp);
            continue;
        }
        else if (!strcmp(temp, "uploadcmp"))
        {
            temp = strtok(NULL, " \n");
            if (!temp)
            {
                printf("missing file to upload\n");
                continue;
            }
            if (checkFileExist(dir, path, temp))
            {
                printf("no such file in directory\n");
                dir = opendir(path);
                continue;
            }
            dir = opendir(path);
            uploadCompress(sockfd, temp);
            continue;
        }
        else if (!strcmp(temp, "exit"))
        {
            int toDo = GET_OUT;
            if (send(sockfd, &toDo, sizeof(int), 0) < 0)
            {
                printf("failed to sebd 'exit' command\nerrno says: %s\n", strerror(errno));
            }
            break;
        }
        else
        {
            int toDo = CHAT;
            if (send(sockfd, &toDo, sizeof(int), 0) < 0)
            {
                printf("failed to send masseage\nerrno says: %s\n", strerror(errno));
                exit(1);
            }
            int size = sizeof(command);
            if (send(sockfd, &size, sizeof(int), 0) < 0)
            {
                printf("failed to send string size\nerrno says: %s\n", strerror(errno));
                exit(1);
            }
            if (send(sockfd, command, sizeof(command), 0) < 0)
            {
                printf("failed to send masseage\nerrno says: %s\n", strerror(errno));
                exit(1);
            }
        }
    }
    
    close(sockfd);
    printf("you have been disconnected\n");
    return 0;
}

