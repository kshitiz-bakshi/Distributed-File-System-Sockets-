#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>

//Client Side For handling client commands
#define PORT 5100
#define BUFFER_SIZE 1024

int connect_to_server() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    server.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("[CLIENT] Failed to connect to server");
        close(sock);
        return -1;
    }
    return sock;
}

void upload_basic(const char *filename) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        printf("[CLIENT] File open error: %s\n", filename);
        // perror("[CLIENT] File open error");
        return;
    }

    int sock = connect_to_server();
    if (sock < 0) {
        fclose(fp);
        return;
    }

    const char *shortname = strrchr(filename, '/');
    if (!shortname) shortname = filename;
    else shortname++;

    fseek(fp, 0, SEEK_END);
    long filesize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char cmd[10] = {0};
    strncpy(cmd, "UPLOAD", 6);
    write(sock, cmd, 10);
    write(sock, shortname, 256);
    write(sock, &filesize, sizeof(long));

    char buffer[BUFFER_SIZE];
    size_t n;
    while ((n = fread(buffer, 1, BUFFER_SIZE, fp)) > 0) {
        send(sock, buffer, n, 0);
    }

    fclose(fp);
    close(sock);
    printf("[CLIENT] Basic file '%s' upload complete.\n", shortname);
}

void uploadf_command(const char *filename, const char *destpath) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        printf("[CLIENT] File open error:\n");
        perror("Error");
        return;
    }

    int sock = connect_to_server();
    if (sock < 0) {
        fclose(fp);
        return;
    }

    const char *shortname = strrchr(filename, '/');
    if (!shortname) shortname = filename;
    else shortname++;

    fseek(fp, 0, SEEK_END);
    long filesize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char cmd[10] = {0};
    strncpy(cmd, "UPLOADF", 7);
    write(sock, cmd, 10);
    write(sock, shortname, 256);
    write(sock, destpath, 512);
    write(sock, &filesize, sizeof(long));

    char buffer[BUFFER_SIZE];
    size_t n;
    while ((n = fread(buffer, 1, BUFFER_SIZE, fp)) > 0) {
        send(sock, buffer, n, 0);
    }

    fclose(fp);
    close(sock);
    printf("[CLIENT] File '%s' upload to '%s' complete.\n", shortname, destpath);
}

void downlf_command(const char *remote_path) {
    int sock = connect_to_server();
    if (sock < 0) return;

    char cmd[10] = {0};
    strncpy(cmd, "DOWNLF", 6);
    write(sock, cmd, 10);
    write(sock, remote_path, 512);  // Send full ~S1/... path

    char filename[256] = {0};
    long filesize;
    read(sock, filename, 256);
    read(sock, &filesize, sizeof(long));

    if (filesize <= 0) {
        printf("[CLIENT] File not found or error occurred.\n");
        close(sock);
        return;
    }

    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        perror("[CLIENT] File open error");
        close(sock);
        return;
    }

    char buffer[BUFFER_SIZE];
    long remaining = filesize;
    while (remaining > 0) {
        int n = recv(sock, buffer, BUFFER_SIZE, 0);
        if (n <= 0) break;
        fwrite(buffer, 1, n, fp);
        remaining -= n;
    }

    fclose(fp);
    close(sock);
    printf("[CLIENT] Downloaded file: %s (%ld bytes)\n", filename, filesize);
}


int main() {
    char input[1024];
    while (1) {
        printf("w25clients$ ");
        fflush(stdout);

        if (!fgets(input, sizeof(input), stdin)) break;
        input[strcspn(input, "\n")] = 0;

        if (strncmp(input, "uploadf", 7) == 0) {
            char filename[512], destpath[512];
            if (sscanf(input, "uploadf %s %s", filename, destpath) != 2) {
                printf("[CLIENT] Syntax error. Use: uploadf <filename> <destination_path>\n");
                continue;
            }
            if (strncmp(destpath, "~S1/", 4) != 0) {
                printf("[CLIENT] Invalid destination path. You can only upload to paths under ~S1/.\n");
                continue;
            }            

            if (access(filename, F_OK) != 0) {
                printf("[CLIENT] File '%s' does not exist.\n", filename);
                continue;
            }

            uploadf_command(filename, destpath);
        } else if (access(input, F_OK) == 0) {
            upload_basic(input);
        } else if (strncmp(input, "downlf", 6) == 0) {
            char remotepath[512];
            if (sscanf(input, "downlf %s", remotepath) != 1) {
                printf("[CLIENT] Syntax error. Use: downlf <~S1/path/to/file>\n");
                continue;
            }
            if (strncmp(remotepath, "~S1/", 4) != 0) {
                printf("[CLIENT] Invalid path. Only ~S1/... is allowed.\n");
                continue;
            }
            downlf_command(remotepath);
        }
        
        else if (strncmp(input, "removef", 7) == 0) {
            char remotepath[512];
            if (sscanf(input, "removef %s", remotepath) != 1) {
                printf("[CLIENT] Syntax error. Use: removef <~S1/path/to/file>\n");
                continue;
            }
        
            int sock = connect_to_server();
            if (sock < 0) continue;
        
            char cmd[10] = {0};
            strncpy(cmd, "REMOVEF", 7);
            write(sock, cmd, 10);
            write(sock, remotepath, 512);
        
            int result = 0;
            read(sock, &result, sizeof(int));
            close(sock);
        
            if (result == 1)
                printf("[CLIENT] File successfully removed: %s\n", remotepath);
            else
                printf("[CLIENT] Failed to remove file: %s\n", remotepath);
        }
        else if (strncmp(input, "downltar", 8) == 0) {
            char ext[10];
            if (sscanf(input, "downltar %s", ext) != 1) {
                printf("[CLIENT] Syntax error. Use: downltar <.c/.pdf/.txt>\n");
                continue;
            }
        
            int sock = connect_to_server();
            if (sock < 0) continue;
        
            char cmd[10] = "DOWNLTAR";
            write(sock, cmd, 10);
            write(sock, ext, 10);
        
            char filename[256] = {0};
            long filesize;
            read(sock, filename, 256);
            read(sock, &filesize, sizeof(long));
        
            if (filesize <= 0) {
                printf("[CLIENT] Failed to retrieve tar file.\n");
                close(sock);
                continue;
            }
        
            FILE *fp = fopen(filename, "wb");
            if (!fp) {
                perror("[CLIENT] File open error");
                close(sock);
                continue;
            }
        
            char buffer[BUFFER_SIZE];
            long remaining = filesize;
            while (remaining > 0) {
                int n = recv(sock, buffer, BUFFER_SIZE, 0);
                if (n <= 0) break;
                fwrite(buffer, 1, n, fp);
                remaining -= n;
            }
        
            fclose(fp);
            close(sock);
            printf("[CLIENT] Tar file downloaded: %s (%ld bytes)\n", filename, filesize);
        }     
        else if (strncmp(input, "dispfnames", 10) == 0) {
            char path[512];
            if (sscanf(input, "dispfnames %s", path) != 1) {
                printf("[CLIENT] Syntax: dispfnames <~S1/path>\n");
                continue;
            }
        
            int sock = connect_to_server();
            if (sock < 0) {
                printf("[CLIENT] Failed to connect to server.\n");
                continue;
            }
        
            // Send 10-byte command
            char cmd[10] = {0};
            strncpy(cmd, "DISPFNAMES", 10);
            if (write(sock, cmd, 10) != 10) {
                perror("[CLIENT] Failed to send command");
                close(sock);
                continue;
            }
        
            // Send the directory path
            if (write(sock, path, 512) != 512) {
                perror("[CLIENT] Failed to send path");
                close(sock);
                continue;
            }
        
            long size = 0;
            if (read(sock, &size, sizeof(long)) != sizeof(long)) {
                perror("[CLIENT] Failed to receive file list size");
                close(sock);
                continue;
            }
        
            if (size <= 0) {
                printf("[CLIENT] No files found or an error occurred.\n");
                close(sock);
                continue;
            }
        
            char buffer[1024];
            long remaining = size;
            printf("[CLIENT] Files found in %s:\n", path);
        
            while (remaining > 0) {
                int n = recv(sock, buffer, sizeof(buffer), 0);
                if (n <= 0) break;
                fwrite(buffer, 1, n, stdout);
                remaining -= n;
            }
        
            printf("\n");
            close(sock);
        }
                           
        else {
            printf("[CLIENT] Unknown command or file '%s' not found.\n", input);
        }
    }
    return 0;
}
