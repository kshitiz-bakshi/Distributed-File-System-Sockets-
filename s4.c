#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>

#define PORT 5003            // s2: 5001, s3: 5002, s4: 5003
#define SUB_DIR "s4"         // s2: "s2", s3: "s3", s4: "s4"
#define BASE_DIR "/Users/kshitiz/Documents/Uploads"
#define BUFFER_SIZE 1024

int main() {
    char full_dir[512];
    snprintf(full_dir, sizeof(full_dir), "%s/%s", BASE_DIR, SUB_DIR);
    mkdir(BASE_DIR, 0777);
    mkdir(full_dir, 0777);
    printf("[%s] Server running on port %d\n", SUB_DIR, PORT);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    server.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("[SERVER] Bind failed");
        return 1;
    }
    listen(sock, 5);

    while (1) {
        int newsock = accept(sock, NULL, NULL);
        if (newsock < 0) {
            perror("[SERVER] Accept failed");
            continue;
        }

        char filename[256] = {0};
        char subpath[512] = {0};
        read(newsock, filename, 256);
        read(newsock, subpath, 512);

        if (strcmp(filename, "__REMOVEF__") == 0) {
            printf("[%s] REMOVEF request received\n", SUB_DIR);

            char folder_only[512] = {0};
            strncpy(folder_only, subpath, sizeof(folder_only));
            char *last_slash = strrchr(folder_only, '/');
            char *file_only = subpath;
            if (last_slash) {
                *last_slash = '\0';
                file_only = last_slash + 1;
            } else {
                strcpy(folder_only, "");
            }

            char path[1024];
            if (strlen(folder_only) > 0)
                snprintf(path, sizeof(path), "%s/%s/%s/%s", BASE_DIR, SUB_DIR, folder_only, file_only);
            else
                snprintf(path, sizeof(path), "%s/%s/%s", BASE_DIR, SUB_DIR, file_only);

            printf("[%s] Attempting to remove file: %s\n", SUB_DIR, path);
            int result = (remove(path) == 0) ? 1 : 0;
            if (!result) perror("[SERVER] File removal failed");

            write(newsock, &result, sizeof(int));
            close(newsock);
            continue;
        }

        if (strcmp(filename, "__LIST__") == 0) {
            printf("[%s] LIST request\n", SUB_DIR);
            char fullpath[1024];
            snprintf(fullpath, sizeof(fullpath), "%s/%s/%s", BASE_DIR, SUB_DIR, subpath);
        
            char pattern[16];
            strcpy(pattern, "*.zip");
            char cmd[1024];
            snprintf(cmd, sizeof(cmd), "find %s -type f -name \"%s\" | xargs -n 1 basename | sort > /tmp/listing.txt", fullpath, pattern);
            system(cmd);
        
            FILE *fp = fopen("/tmp/listing.txt", "r");
            if (!fp) {
                perror("[SERVER] Listing file open failed");
                long zero = 0;
                write(newsock, &zero, sizeof(long));
                close(newsock);
                continue;
            }
        
            fseek(fp, 0, SEEK_END);
            long size = ftell(fp);
            fseek(fp, 0, SEEK_SET);
            write(newsock, &size, sizeof(long));
        
            char buf[BUFFER_SIZE];
            size_t n;
            while ((n = fread(buf, 1, BUFFER_SIZE, fp)) > 0) {
                send(newsock, buf, n, 0);
            }
            fclose(fp);
            close(newsock);
            continue;
        }
        

        // Detect if it's a download or upload using select
        char peekbuf[8] = {0};
        fd_set rfds;
        struct timeval tv;
        FD_ZERO(&rfds);
        FD_SET(newsock, &rfds);
        tv.tv_sec = 0;
        tv.tv_usec = 500000;  // 0.5s timeout

        int peek_ready = select(newsock + 1, &rfds, NULL, NULL, &tv);
        if (peek_ready <= 0) {
            printf("[%s] No additional data — treating as DOWNLOAD\n", SUB_DIR);

            char folder_only[512] = {0};
            strncpy(folder_only, subpath, sizeof(folder_only));
            char *last_slash = strrchr(folder_only, '/');
            char *file_only = subpath;
            if (last_slash) {
                *last_slash = '\0';
                file_only = last_slash + 1;
            } else {
                strcpy(folder_only, "");
            }

            char filepath[1024];
            if (strlen(folder_only) > 0)
                snprintf(filepath, sizeof(filepath), "%s/%s/%s/%s", BASE_DIR, SUB_DIR, folder_only, file_only);
            else
                snprintf(filepath, sizeof(filepath), "%s/%s/%s", BASE_DIR, SUB_DIR, file_only);

            printf("[%s] Resolved filepath: %s\n", SUB_DIR, filepath);

            FILE *fp = fopen(filepath, "rb");
            if (!fp) {
                perror("[SERVER] File open failed");
                long zero = 0;
                write(newsock, &zero, sizeof(long));
                close(newsock);
                continue;
            }

            fseek(fp, 0, SEEK_END);
            long filesize = ftell(fp);
            fseek(fp, 0, SEEK_SET);
            write(newsock, &filesize, sizeof(long));

            printf("[%s] Sending %s (%ld bytes)\n", SUB_DIR, filepath, filesize);

            char buffer[BUFFER_SIZE];
            size_t m;
            while ((m = fread(buffer, 1, BUFFER_SIZE, fp)) > 0) {
                send(newsock, buffer, m, 0);
            }

            fclose(fp);
            close(newsock);
            continue;
        }

        // Else: treat as upload
        long filesize = 0;
        read(newsock, &filesize, sizeof(long));

        char folder_only[512] = {0};
        strncpy(folder_only, subpath, sizeof(folder_only));
        char *last_slash = strrchr(folder_only, '/');
        if (last_slash) *last_slash = '\0';

        char full_path[1024];
        if (strlen(folder_only) > 0)
            snprintf(full_path, sizeof(full_path), "%s/%s/%s", BASE_DIR, SUB_DIR, folder_only);
        else
            snprintf(full_path, sizeof(full_path), "%s/%s", BASE_DIR, SUB_DIR);
        mkdir(full_path, 0777);

        char filepath[1024];
        snprintf(filepath, sizeof(filepath), "%s/%s", full_path, filename);

        printf("[%s] Receiving %s (%ld bytes) to %s\n", SUB_DIR, filename, filesize, filepath);

        FILE *fp = fopen(filepath, "wb");
        if (!fp) {
            perror("[SERVER] File open failed");
            close(newsock);
            continue;
        }

        char buffer[BUFFER_SIZE];
        long remaining = filesize;
        while (remaining > 0) {
            int r = recv(newsock, buffer, BUFFER_SIZE, 0);
            if (r <= 0) break;
            fwrite(buffer, 1, r, fp);
            remaining -= r;
        }

        fclose(fp);
        close(newsock);
        printf("[%s] Saved %s successfully\n", SUB_DIR, filepath);
    }

    return 0;
}
