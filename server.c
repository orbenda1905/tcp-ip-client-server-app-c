//
//  main.c
//  finalServer
//
//  Created by Or Ben David on 11/22/15.
//  Copyright © 2015 Or Ben David. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>

#define LS 1
#define DOWNLOAD 2
#define UPLOAD 3
#define UPLOADCMP 4
#define GET_OUT 5
#define CHAT 6
#define N 1024

#define PORT "9034"   // port we're listening on

// get sockaddr, IPv4 or IPv6:


void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void removeClient(int nbytes, int fd, fd_set *masterSet)
{
    // got error or connection closed by client
    if (nbytes == 0)
    {
        // connection closed
        printf("selectserver: socket %d hung up\n", fd);
    }
    else
    {
        perror("recv");
    }
    close(fd); // bye!
    FD_CLR(fd, masterSet); // remove from master set
}

void lsCommand(int sockfd, DIR *dir, struct dirent *entry, char *path, fd_set *masterSet)
{
    int counter = 0;
    while ((entry = readdir(dir)) != NULL)
    {
        counter++;
    }
    if ((send(sockfd, &counter, sizeof(int), 0)) < 0)
    {
        printf("faild send the number of files");
        removeClient(-1, sockfd, masterSet);
        closedir(dir);
        return;
    }
    closedir(dir);
    dir = opendir(path);
    printf("content(%d files):\n", counter);
    while ((entry = readdir(dir)) != NULL)
    {
        int size = strlen(entry->d_name);
        if ((send(sockfd, &size, sizeof(int), 0)) < 0)
        {
            printf("fail to sent lsFileName size\nerrno says: %s\n", strerror(errno));
            removeClient(-1, sockfd, masterSet);
            break;
        }
        if ((send(sockfd, entry->d_name, size, 0)) < 0)
        {
            printf("fail to send ls name/nerrno says: %s\n", strerror(errno));
            removeClient(-1, sockfd, masterSet);
            break;
        }
        printf(" - %s\n", entry->d_name);
    }
    closedir(dir);
}

void downldCmd(int sockfd, char *fileName, fd_set *masterSet)
{
    int fileD = 0;
    int flag = 0;
    int loop = 1;
    int fileSize = 0;
    struct stat stBuf;
    char *data;
    while(loop)
    {
        if ((fileD = open(fileName, O_RDWR, S_IRWXU)) < 0)
        {
            printf("failed to open the file\nerrno says: %s\n", strerror(errno));
            flag = 1;
            break;
        }
        if (stat(fileName, &stBuf) < 0)
        {
            printf("failed to get file 'stat'\nerrno says: %s\n", strerror(errno));
            flag = 2;
            close(fileD);
            break;
        }
        fileSize = (int)stBuf.st_size;
        if ((data = mmap((caddr_t)0, fileSize, PROT_READ, MAP_SHARED, fileD, 0)) == (caddr_t)-1)
        {
            printf("failed to open 'mmap'\nerrno says: %s\n", strerror(errno));
            flag = 3;
            close(fileD);
            break;
        }
        --loop;
    }
    if (send(sockfd, &flag, sizeof(int), 0) < 0)
    {
        printf("failed to send the flag\nerrno says: %s\n", strerror(errno));
        close(fileD);
        if (flag)
        {
            if (flag > 1)
            {
                close(fileD);
            }
            if (flag > 2)
            {
                if (munmap(data, fileSize) < 0)
                {
                    printf("'munmap' failed\nernnro says: %s\n", strerror(errno));
                    exit(1);
                }
            }
        }
        removeClient(-1, sockfd, masterSet);
    }
    if (flag)
    {
        if (flag > 1)
        {
            close(fileD);
            if (flag > 2)
            {
                if (munmap(data, fileSize) < 0)
                {
                    printf("'munmap' failed\nernnro says: %s\n", strerror(errno));
                    exit(1);
                }
            }
        }
        return;
    }
    fileSize = (int)stBuf.st_size;
    if (send(sockfd, &fileSize, sizeof(int), 0) < 0)
    {
        printf("failed to send fileSize to cilent\nerrno says: %s\n", strerror(errno));
        close(fileD);
        close(sockfd);
        if (munmap(data, fileSize) < 0)
        {
            printf("'munmap' failed\nernnro says: %s\n", strerror(errno));
            exit(1);
        }
        FD_CLR(sockfd, masterSet);
        return;
    }
    int bytesLeft = fileSize;
    int bytesSent = 0;
    int totalSent = 0;
    int blockSize = 0;
    int counter = 0;
    while (totalSent < fileSize)
    {
        blockSize = N;
        if (N > (fileSize - totalSent))
        {
            blockSize = fileSize - totalSent;
        }
        if (send(sockfd, &blockSize, sizeof(int), 0) < 0)//sending the size of the block that's gonna be send
        {
            flag = 1;
            printf("failed to send block size\nerrno says: %s\n", strerror(errno));
            break;
        }
        while(bytesSent < blockSize)//sending the block
        {
            if ((counter = send(sockfd, data + totalSent + bytesSent, blockSize - bytesSent, 0)) < 0)
            {
                flag = 1;
                printf("failed to send bytes\nerrno says: %s\n", strerror(errno));
                break;
            }
            bytesSent += counter;
        }
        if (flag)
        {
            break;
        }
        totalSent += bytesSent;
        bytesSent = 0;
    }
    close(fileD);
    if (flag)
    {
        close(sockfd);
        FD_CLR(sockfd, masterSet);
    }
    if (munmap(data, fileSize) < 0)
    {
        printf("'munmap' failed\nernno says: %s\n", strerror(errno));
        if (!flag)
        {
            close(sockfd);
        }
        exit(1);
    }
}

void uploadCmd(int sockfd, char *fileName, fd_set *masterSet)
{
    int fileSize = 0;
    int fileD = 0;
    int flag = 0;//indicates wether there was a failure
    int loop = 1;
    char *data;
    while (loop)
    {
        if (recv(sockfd, &fileSize, sizeof(int), 0) < 0)
        {
            printf("failed to get file size\nernno says: %s\n", strerror(errno));
            close(fileD);
            removeClient(-1, sockfd, masterSet);
            return;
        }
        if ((fileD = open(fileName, O_CREAT | O_RDWR, S_IRWXU)) < 0)
        {
            flag = 1;
            printf("failed to open new file\nernno says: %s\n", strerror(errno));
            break;
            
        }//we have created the new file
        if (lseek(fileD, fileSize - 1, SEEK_SET) == -1)
        {
            flag = 2;
            printf("failed 'lseek'\n");
            close(fileD);
            break;
        }
        if (write(fileD, "", 1) != 1)
        {
            flag = 2;
            printf("failed writing to file\n");
            close(fileD);
            break;
        }
        if ((data =  mmap(NULL, fileSize, PROT_READ | PROT_WRITE, MAP_SHARED, fileD, 0)) == MAP_FAILED)
        {
            flag = 3;
            printf("failed to create mmap\nerrno says: %s\n", strerror(errno));
            close(fileD);
            break;
        }
        --loop;
    }
    if (send(sockfd, &flag, sizeof(int), 0) < 0)
    {
        printf("failed to send the flag\nerrno says: %s\n", strerror(errno));
        
        if (flag > 1)
        {
            close(fileD);
            if (flag > 2)
            {
                if (munmap(data, fileSize) < 0)
                {
                    printf("'munmap' failed\nernnro says: %s\n", strerror(errno));
                    close(sockfd);
                    exit(1);
                }
            }
        }
        removeClient(-1, sockfd, masterSet);
    }
    if (flag)
    {
        if (flag > 1)
        {
            close(fileD);
            if (flag > 2)
            {
                if (munmap(data, fileSize) < 0)
                {
                    printf("'munmap' failed\nernnro says: %s\n", strerror(errno));
                    close(sockfd);
                    exit(1);
                }
            }
        }
        return;
    }
    int totalRecvBytes = 0;
    int bytesRecv = 0;
    int currentBYtesBlock = 0;
    int counter = 0;
    char buffer[N] = {0};
    while (totalRecvBytes < fileSize)
    {
        if (recv(sockfd, &currentBYtesBlock, sizeof(int), 0) < 0)
        {
            printf("failed to receive bytes block size\nerrno says: %s\n", strerror(errno));
            break;
        }
        if (currentBYtesBlock < 0)
        {
            flag = 1;
            printf("client failed to read from the file\n");
            break;
        }
        if (currentBYtesBlock == 0)
        {
            break;
        }
        while (bytesRecv < currentBYtesBlock)//receiving the block
        {
            if ((counter = recv(sockfd, buffer + bytesRecv, currentBYtesBlock - bytesRecv, 0)) < 0)
            {
                flag = 1;
                printf("failed to receive bytes from client\nerrno says: %s\n", strerror(errno));
                break;
            }
            bytesRecv += counter;
        }
        if (flag)
        {
            break;
        }
        bytesRecv = 0;
        int i = 0;
        while (i < currentBYtesBlock)
        {
            data[totalRecvBytes + i] = buffer[i];
            i++;
        }
        totalRecvBytes += currentBYtesBlock;
        bzero(buffer, N);
    }
    
    close(fileD);
    if (flag)
    {
        close(sockfd);
        FD_CLR(sockfd, masterSet);
    }
    if (munmap(data, fileSize) < 0)
    {
        printf("'munmap' failed\nernnro says: %s\n", strerror(errno));
        if (!flag)
        {
            close(sockfd);
        }
        close(sockfd);
        exit(1);
    }
    
}

void compressedUpload(int sockfd, char *fileName, fd_set *masterSet)
{
    int fileD = 0;
    int flag = 0;
    if ((fileD = open(fileName, O_CREAT | O_RDWR, S_IRWXU)) < 0)
    {
        printf("failed to open new file\nernno says: %s\n", strerror(errno));
        flag = 1;
        return;
    }
    if (send(sockfd, &flag, sizeof(int), 0) < 0)
    {
        printf("failed to send the flag to the client\nerrno says: %s\n", strerror(errno));
        close(fileD);
        removeClient(-1, sockfd, masterSet);
        return;
    }
    if (flag)
    {
        if (fileD)
        {
            close(fileD);
        }
        return;
    }
    int wroteOnBuffer = 0;
    int counter = 0;
    int toWriteOnBuffer = N-1;
    int wroteOnFile = 0;
    char buffer[N] = {0};
    //get the bytes by block, each time
    while (toWriteOnBuffer == N-1)
    {//receive the size of the first block
        if (recv(sockfd, &toWriteOnBuffer, sizeof(int), 0) < 0)
        {
            printf("failed to receive number of bytes to writen\nerrno says: %s\n", strerror(errno));
            close(fileD);
            close(sockfd);
            FD_CLR(sockfd, masterSet);
            return;
        }
        if (toWriteOnBuffer <= 0)
        {
            int flag = 0;
            if (recv(sockfd, &flag, sizeof(int), 0) < 0)
            {
                printf("failed to receive flag\nerrno says: %s\n", strerror(errno));
                close(fileD);
                close(sockfd);
                FD_CLR(sockfd, masterSet);
                return;
            }
            if (flag == 1)
            {
                printf("success\n");
            }
            if (flag == 0)
            {
                printf("client failed to read from file\n");
            }
            close(fileD);
            return;
        }
        while (wroteOnBuffer < toWriteOnBuffer)//receiving the actual bytes of the block and write them on the buffer
        {
            if ((counter = recv(sockfd, buffer + wroteOnBuffer, toWriteOnBuffer - wroteOnBuffer, 0)) < 0)
            {
                printf("socket problem with client\nerrno says: %s\n", strerror(errno));
                close(fileD);
                close(sockfd);
                FD_CLR(sockfd, masterSet);
                return;
            }
            wroteOnBuffer += counter;
        }//we received the block. now we need to write it on the actual file
        
        while (wroteOnFile < toWriteOnBuffer)
        {
            if ((counter = write(fileD, buffer + wroteOnFile, toWriteOnBuffer - wroteOnFile)) < 0)
            {
                printf("failed to write on file\nerrno says: %s\n", strerror(errno));
                close(fileD);
                close(sockfd);
                FD_CLR(sockfd, masterSet);
                return;
            }
            wroteOnFile += counter;
        }
        bzero(buffer, N);
        wroteOnFile = 0;
        wroteOnBuffer = 0;
    }
}

void getNewConnection(struct sockaddr_storage *remoteaddr, socklen_t *addrlen, int *newfd, int *listener, int *fdmax, fd_set *master, char *remoteIP)
{
    *addrlen = sizeof *remoteaddr;
    *newfd = accept(*listener, (struct sockaddr *)remoteaddr, addrlen);
    
    if (*newfd == -1) {
        perror("accept");
    }
    else
    {
        FD_SET(*newfd, master); // add to master set
        if (*newfd > *fdmax) {    // keep track of the max
            *fdmax = *newfd;
        }
        send(*newfd, "Hello you", 9, 0);
        printf("selectserver: new connection from %s on "
               "socket %d\n",
               inet_ntop(remoteaddr->ss_family,
                         get_in_addr((struct sockaddr*)remoteaddr),
                         remoteIP, INET6_ADDRSTRLEN),
               *newfd);
    }
}



int main(void)
{
    fd_set master;    // master file descriptor list
    fd_set read_fds;  // temp file descriptor list for select()
    int fdmax;        // maximum file descriptor number
    
    int listener = 0;     // listening socket descriptor
    int newfd;        // newly accept()ed socket descriptor
    struct sockaddr_storage remoteaddr; // client address
    socklen_t addrlen;
    
    char buf[N];    // buffer for client data
    char path[N];
    int nbytes;
    int operate = 0;
    
    char remoteIP[INET6_ADDRSTRLEN];
    
    int yes=1;        // for setsockopt() SO_REUSEADDR, below
    int i, j, rv;
    
    struct addrinfo hints, *ai, *p;
    
    DIR *dir;
    struct dirent *entry;
    
    FD_ZERO(&master);    // clear the master and temp sets
    FD_ZERO(&read_fds);
    
    int stringSize = 0;
    
    // get us a socket and bind it
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if ((rv = getaddrinfo(NULL, PORT, &hints, &ai)) != 0)
    {
        fprintf(stderr, "selectserver: %s\n", gai_strerror(rv));
        exit(1);
    }
    
    for(p = ai; p != NULL; p = p->ai_next)
    {
        listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (listener < 0)
        {
            continue;
        }
        
        // lose the pesky "address already in use" error message
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
        
        if (bind(listener, p->ai_addr, p->ai_addrlen) < 0)
        {
            close(listener);
            continue;
        }
        
        break;
    }
    
    // if we got here, it means we didn't get bound
    if (p == NULL)
    {
        fprintf(stderr, "selectserver: failed to bind\n");
        exit(2);
    }
    
    freeaddrinfo(ai); // all done with this
    
    // listen
    if (listen(listener, 10) == -1)
    {
        perror("listen");
        exit(3);
    }
    
    // add the listener to the master set
    FD_SET(listener, &master);
    
    // keep track of the biggest file descriptor
    fdmax = listener; // so far, it's this one
    getcwd(path, N);
    dir = opendir(path);
    if (!dir)
    {
        printf("failed setting server's root\n");
        exit(1);
    }
    
    printf("server has been initialized\n");
    
    // main loop
    for(;;)
    {
        bzero(buf, sizeof(buf));
        
        read_fds = master; // copy it
        if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1)
        {
            perror("select");
            exit(4);
        }
        
        // run through the existing connections looking for data to read
        for(i = 0; i <= fdmax; i++)
        {
            if (FD_ISSET(i, &read_fds))
            { // we got one!!
                if (i == listener)
                {
                    getNewConnection(&remoteaddr, &addrlen, &newfd, &listener, &fdmax, &master, remoteIP);
                }
                else
                {
                    // handle data from a client
                    if ((nbytes = recv(i, &operate, sizeof(int), 0)) <= 0)
                    {
                        removeClient(nbytes, i, &master);
                    }
                    else
                    {
                        
                        // we got some data from a client
                        //i want turn apart the client's command.
                        //if it's not an upload/download/ls then i'll just send it to everyone
                        switch (operate)
                        {
                            case LS:
                                lsCommand(i, dir, entry, path, &master);
                                dir = opendir(path);
                                break;
                                
                            case DOWNLOAD://the client want to download a file from the server
                                if (recv(i, &stringSize, sizeof(int), 0) < 0)
                                {
                                    printf("failed to receive string size\nerrno says: %s\n", strerror(errno));
                                    removeClient(-1, i, &master);
                                    break;
                                }
                                if (recv(i, buf, stringSize, 0) < 0)
                                {
                                    printf("failed receive file name\n");
                                    removeClient(-1, i, &master);
                                    break;
                                }
                                downldCmd(i, buf, &master);
                                break;
                                
                            case UPLOAD://the client want to transfer a file to the serve
                                if (recv(i, &stringSize, sizeof(int), 0) < 0)
                                {
                                    printf("failed to receive string size\nerrno says: %s\n", strerror(errno));
                                    removeClient(-1, i, &master);
                                    break;
                                }
                                if (recv(i, buf, stringSize, 0) < 0)
                                {
                                    printf("failed to receive file name\nerrno says: %s\n", strerror(errno));
                                    removeClient(-1, i, &master);
                                    break;
                                }
                                uploadCmd(i, buf, &master);
                                break;
                                
                            case UPLOADCMP:
                                if (recv(i, &stringSize, sizeof(int), 0) < 0)
                                {
                                    printf("failed to receive string size\nerrno says: %s\n", strerror(errno));
                                    removeClient(-1, i, &master);
                                    break;
                                }
                                if (recv(i, buf, stringSize, 0) < 0)
                                {
                                    printf("failed to receive file name\nerrno says: %s\n", strerror(errno));
                                    removeClient(-1, i, &master);
                                    break;
                                }
                                compressedUpload(i, buf, &master);
                                break;
                                
                            case GET_OUT:
                                printf("socket %d disconnected\n", i);
                                removeClient(0, i, &master);
                                break;
//                            case CHAT:
//                                if (recv(i, buf, stringSize, 0) < 0)
//                                {
//                                    printf("failed to receive file name\nerrno says: %s\n", strerror(errno));
//                                    removeClient(-1, i, &master);
//                                    break;
//                                }
//                                if (recv(i, buf, stringSize, 0) < 0)
//                                {
//                                    printf("failed to receive file name\nerrno says: %s\n", strerror(errno));
//                                    removeClient(-1, i, &master);
//                                    break;
//                                }
//                                for (j = 0; j < fdmax; j++)
//                                {
//                                    if (FD_ISSET(j, &master))
//                                    {
//                                        if (j != listener && j != i)
//                                        {
//                                            if (send(j, buf, stringSize, 0) < 0)
//                                            {
//                                                printf("failed to send message to socket %d\nerrno says: %s\n", j, strerror(errno));
//                                                removeClient(-1, j, &master);
//                                            }
//                                        }
//                                    }
//                                }
                                
                        }
                    }
                } // END handle data from client
            } // END got new incoming connection
        } // END looping through file descriptors
    } // END for(;;)--and you thought it would never end!
    free(path);
    return 0;
}
