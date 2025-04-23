#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <fcntl.h>

//S1 Main Server 
#define PORT 5100
#define BUFFER_SIZE 1024
#define BASE_DIR "/Users/kshitiz/Documents/Uploads"
#define SUB_DIR "s1"

#define DUMMY256_SIZE 256
#define DUMMY512_SIZE 512
char DUMMY256[DUMMY256_SIZE] = {0};
char DUMMY512[DUMMY512_SIZE] = {0};


void forward_stream_direct(const char *ip, int port, const char *filename, const char *subpath, long filesize, int source_sock) {
    printf("[S1] Forwarding %s (%ld bytes) directly to %s:%d\n", filename, filesize, ip, port);

    int s = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in dest;
    dest.sin_family = AF_INET;
    dest.sin_port = htons(port);
    dest.sin_addr.s_addr = inet_addr(ip);

    if (connect(s, (struct sockaddr *)&dest, sizeof(dest)) < 0) {
        printf("[S1] Failed to connect to destination server\n");
        // perror("[S1] Failed to connect to destination server"); //
        close(s);
        return;
    }

    write(s, filename, 256);
    write(s, subpath, 512);
    write(s, &filesize, sizeof(long));

    char buffer[BUFFER_SIZE];
    long remaining = filesize;
    while (remaining > 0) {
        int n = recv(source_sock, buffer, BUFFER_SIZE, 0);
        if (n <= 0) break;
        send(s, buffer, n, 0);
        remaining -= n;
    }

    close(s);
    printf("[S1] Forwarding of %s complete.\n", filename);
}

void save_locally(const char *filepath, const char *filename, long filesize, int source_sock) {
    FILE *fp = fopen(filepath, "wb");
    if (!fp) {
        printf("[S1] Failed to open file for saving: %s\n", filepath);
        perror("[S1] Error");
        return;
    }

    printf("[S1] Saving %s locally at %s\n", filename, filepath);

    char buffer[BUFFER_SIZE];
    long remaining = filesize;
    while (remaining > 0) {
        int n = recv(source_sock, buffer, BUFFER_SIZE, 0);
        if (n <= 0) break;
        fwrite(buffer, 1, n, fp);
        remaining -= n;
    }
    fclose(fp);
    printf("[S1] Saved %s successfully.\n", filename);
}

void handle_removef(const char *remotepath, int client_sock) {
    printf("[S1] Received REMOVEF request for: %s\n", remotepath);

    char *filename = strrchr(remotepath, '/');
    if (!filename) {
        printf("[S1] Invalid path for removal.\n");
        int result = 0;
        write(client_sock, &result, sizeof(int));
        return;
    }
    filename++;

    const char *ext = strrchr(filename, '.');
    if (!ext) {
        printf("[S1] Extension not found for file: %s\n", filename);
        int result = 0;
        write(client_sock, &result, sizeof(int));
        return;
    }

    char subpath[512] = {0};
    strncpy(subpath, remotepath + 4, sizeof(subpath)); // skip ~S1
    printf("[S1] Extracted subpath: %s\n", subpath);

    if (strcmp(ext, ".c") == 0) {
        char filepath[512];
        snprintf(filepath, sizeof(filepath), "%s/%s/%s", BASE_DIR, SUB_DIR, subpath);
        printf("[S1] Removing local file: %s\n", filepath);
        int success = remove(filepath) == 0 ? 1 : 0;
        if (!success) perror("[S1] Local remove failed");
        write(client_sock, &success, sizeof(int));
    } else {
        const char *ip = "127.0.0.1";
        int port = strcmp(ext, ".pdf") == 0 ? 5001 : strcmp(ext, ".txt") == 0 ? 5002 : 5003;

        printf("[S1] Forwarding remove request to port %d\n", port);
        int s = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in dest;
        dest.sin_family = AF_INET;
        dest.sin_port = htons(port);
        dest.sin_addr.s_addr = inet_addr(ip);

        if (connect(s, (struct sockaddr *)&dest, sizeof(dest)) < 0) {
            perror("[S1] Failed to connect to backend server for remove");
            int result = 0;
            write(client_sock, &result, sizeof(int));
            return;
        }

        char remove_cmd[256] = "__REMOVEF__";
        write(s, remove_cmd, 256);
        write(s, subpath, 512);

        int result = 0;
        read(s, &result, sizeof(int));
        close(s);
        write(client_sock, &result, sizeof(int));
        printf("[S1] Backend returned delete status: %d\n", result);
    }
}

void prcclient(int client_sock) {
    while (1) {
        char cmd[10] = {0};
        if (read(client_sock, cmd, 10) <= 0) break;
        printf("[S1] Received command: %s\n", cmd);

        if (strcmp(cmd, "UPLOAD") == 0) {
            char filename[256];
            long filesize;
            read(client_sock, filename, 256);
            read(client_sock, &filesize, sizeof(long));

            char *ext = strrchr(filename, '.');
            if (ext && strcmp(ext, ".c") == 0) {
                char filepath[512];
                snprintf(filepath, sizeof(filepath), "%s/%s/%s", BASE_DIR, SUB_DIR, filename);
                save_locally(filepath, filename, filesize, client_sock);
            } else {
                const char *ip = "127.0.0.1";
                int port = strcmp(ext, ".pdf") == 0 ? 5001 : strcmp(ext, ".txt") == 0 ? 5002 : 5003;
                forward_stream_direct(ip, port, filename, "", filesize, client_sock);
            }
        }
        else if (strcmp(cmd, "UPLOADF") == 0) {
            char filename[256], destpath[512];
            long filesize;
            read(client_sock, filename, 256);
            read(client_sock, destpath, 512);
            read(client_sock, &filesize, sizeof(long));

            char *subpath = destpath + 4; // skip ~S1/
            char *ext = strrchr(filename, '.');

            char full_save_dir[512];
            snprintf(full_save_dir, sizeof(full_save_dir), "%s/%s/%s", BASE_DIR, SUB_DIR, subpath);
            mkdir(BASE_DIR, 0777);
            char temp[512];
            snprintf(temp, sizeof(temp), "%s/%s", BASE_DIR, SUB_DIR);
            mkdir(temp, 0777);
            mkdir(full_save_dir, 0777);

            char filepath[512];
            snprintf(filepath, sizeof(filepath), "%s/%s", full_save_dir, filename);
            save_locally(filepath, filename, filesize, client_sock);

            if (ext && strcmp(ext, ".c") != 0) {
                const char *ip = "127.0.0.1";
                int port = strcmp(ext, ".pdf") == 0 ? 5001 : strcmp(ext, ".txt") == 0 ? 5002 : 5003;

                FILE *check = fopen(filepath, "rb");
                if (check) {
                    fclose(check);
                    forward_stream_direct(ip, port, filename, subpath, filesize, open(filepath, O_RDONLY));
                    remove(filepath);
                    printf("[S1] Deleted %s from S1 after forwarding.\n", filepath);
                }
            }
        }
        else if (strcmp(cmd, "DOWNLF") == 0) {
            char remotepath[512] = {0};
            read(client_sock, remotepath, 512);
            printf("[S1] Received DOWNLF request for: %s\n", remotepath);
        
            char *filename = strrchr(remotepath, '/');
            if (!filename) {
                long zero = 0;
                write(client_sock, "", 256);
                write(client_sock, &zero, sizeof(long));
                printf("[S1] Invalid path received.\n");
                return;
            }
            filename++;
        
            const char *ext = strrchr(filename, '.');
            if (!ext) {
                long zero = 0;
                write(client_sock, "", 256);
                write(client_sock, &zero, sizeof(long));
                printf("[S1] Extension not found for file: %s\n", filename);
                return;
            }
        
            char subpath[512] = {0};
            strncpy(subpath, remotepath + 4, sizeof(subpath)); // skip ~S1
            printf("[S1] Extracted subpath: %s\n", subpath);
        
            const char *server_ip = "127.0.0.1";
            int port = 0;
            char filepath[1024];
        
            if (strcmp(ext, ".c") == 0) {
                snprintf(filepath, sizeof(filepath), "%s/%s/%s", BASE_DIR, SUB_DIR, subpath);
                printf("[S1] Fetching local .c file from: %s\n", filepath);
        
                FILE *fp = fopen(filepath, "rb");
                if (!fp) {
                    perror("[S1] File open failed");
                    long zero = 0;
                    write(client_sock, "", 256);
                    write(client_sock, &zero, sizeof(long));
                    return;
                }
        
                fseek(fp, 0, SEEK_END);
                long filesize = ftell(fp);
                fseek(fp, 0, SEEK_SET);
        
                write(client_sock, filename, 256);
                write(client_sock, &filesize, sizeof(long));
        
                char buffer[BUFFER_SIZE];
                size_t n;
                while ((n = fread(buffer, 1, BUFFER_SIZE, fp)) > 0) {
                    send(client_sock, buffer, n, 0);
                }
                fclose(fp);
                printf("[S1] Sent .c file (%ld bytes) to client\n", filesize);
            } else {
                if (strcmp(ext, ".pdf") == 0) port = 5001;
                else if (strcmp(ext, ".txt") == 0) port = 5002;
                else if (strcmp(ext, ".zip") == 0) port = 5003;
                else {
                    long zero = 0;
                    write(client_sock, "", 256);
                    write(client_sock, &zero, sizeof(long));
                    printf("[S1] Unsupported file type: %s\n", ext);
                    return;
                }
        
                printf("[S1] Connecting to server at port %d to fetch file: %s\n", port, filename);
        
                int s = socket(AF_INET, SOCK_STREAM, 0);
                struct sockaddr_in dest;
                dest.sin_family = AF_INET;
                dest.sin_port = htons(port);
                dest.sin_addr.s_addr = inet_addr(server_ip);
        
                if (connect(s, (struct sockaddr *)&dest, sizeof(dest)) < 0) {
                    perror("[S1] Failed to connect to destination server");
                    long zero = 0;
                    write(client_sock, "", 256);
                    write(client_sock, &zero, sizeof(long));
                    close(s);
                    return;
                }
        
                write(s, filename, 256);
                write(s, subpath, 512);
                printf("[S1] Sent filename and subpath to backend server\n");
        
                long filesize;
                read(s, &filesize, sizeof(long));
        
                if (filesize <= 0) {
                    write(client_sock, "", 256);
                    write(client_sock, &filesize, sizeof(long));
                    printf("[S1] Backend server reported file size <= 0\n");
                    close(s);
                    return;
                }
        
                write(client_sock, filename, 256);
                write(client_sock, &filesize, sizeof(long));
        
                char buffer[BUFFER_SIZE];
                long remaining = filesize;
                while (remaining > 0) {
                    int n = recv(s, buffer, BUFFER_SIZE, 0);
                    if (n <= 0) break;
                    send(client_sock, buffer, n, 0);
                    remaining -= n;
                }
                close(s);
                printf("[S1] Successfully forwarded file (%ld bytes) to client\n", filesize);
            }
        }
        else if (strcmp(cmd, "REMOVEF") == 0) {
            char remotepath[512] = {0};
            read(client_sock, remotepath, 512);
            handle_removef(remotepath, client_sock);
        }
        else if (strcmp(cmd, "DOWNLTAR") == 0) {
            char filetype[10] = {0};
            read(client_sock, filetype, 10);
            printf("[S1] Received DOWNLTAR request for type: %s\n", filetype);
        
            char tarname[256];
            char cmd[512];
            long filesize;
            FILE *fp;
        
            if (strcmp(filetype, ".c") == 0) {
                snprintf(tarname, sizeof(tarname), "cfiles.tar");
                snprintf(cmd, sizeof(cmd), "cd %s/%s && find . -name \"*.c\" | tar -cf %s -T -", BASE_DIR, SUB_DIR, tarname);
                printf("[S1] Running: %s\n", cmd);
                system(cmd);
        
                char tarpath[512];
                snprintf(tarpath, sizeof(tarpath), "%s/%s/%s", BASE_DIR, SUB_DIR, tarname);
                fp = fopen(tarpath, "rb");
                if (!fp) {
                    perror("[S1] Tar open failed");
                    filesize = 0;
                    write(client_sock, tarname, 256);
                    write(client_sock, &filesize, sizeof(long));
                    return;
                }
        
                fseek(fp, 0, SEEK_END);
                filesize = ftell(fp);
                fseek(fp, 0, SEEK_SET);
                write(client_sock, tarname, 256);
                write(client_sock, &filesize, sizeof(long));
        
                char buffer[BUFFER_SIZE];
                size_t n;
                while ((n = fread(buffer, 1, BUFFER_SIZE, fp)) > 0)
                    send(client_sock, buffer, n, 0);
        
                fclose(fp);
                remove(tarpath);
                printf("[S1] Sent local tar file: %s (%ld bytes)\n", tarname, filesize);
            }
            else if (strcmp(filetype, ".pdf") == 0 || strcmp(filetype, ".txt") == 0) {
                int port = strcmp(filetype, ".pdf") == 0 ? 5001 : 5002;
                const char *tarfile = strcmp(filetype, ".pdf") == 0 ? "pdf.tar" : "text.tar";
                const char *ip = "127.0.0.1";
            
                int s = socket(AF_INET, SOCK_STREAM, 0);
                struct sockaddr_in dest;
                dest.sin_family = AF_INET;
                dest.sin_port = htons(port);
                dest.sin_addr.s_addr = inet_addr(ip);
            
                printf("[S1] Connecting to server at port %d to request %s\n", port, tarfile);
                if (connect(s, (struct sockaddr *)&dest, sizeof(dest)) < 0) {
                    perror("[S1] Failed to connect to backend for tar");
                    long zero = 0;
                    write(client_sock, tarfile, 256);
                    write(client_sock, &zero, sizeof(long));
                    return;
                }
            
                char tarcmd[256] = "__TAR__";
                write(s, tarcmd, 256);
                write(s, "", 512);  // Send dummy subpath
                printf("[S1] Sent __TAR__ and dummy path to backend\n");
            
                long filesize;
                read(s, &filesize, sizeof(long));
                write(client_sock, tarfile, 256);
                write(client_sock, &filesize, sizeof(long));
            
                if (filesize <= 0) {
                    printf("[S1] Backend server returned zero-size tar\n");
                    close(s);
                    return;
                }
            
                char buffer[BUFFER_SIZE];
                long remaining = filesize;
                while (remaining > 0) {
                    int n = recv(s, buffer, BUFFER_SIZE, 0);
                    if (n <= 0) break;
                    send(client_sock, buffer, n, 0);
                    remaining -= n;
                }
                close(s);
                printf("[S1] Sent %s (%ld bytes) to client\n", tarfile, filesize);
            }
            
            else {
                printf("[S1] Invalid filetype for tar\n");
                long zero = 0;
                write(client_sock, "", 256);
                write(client_sock, &zero, sizeof(long));
            }
        }        
        else if (strncmp(cmd, "DISPFNAMES", 10) == 0) {
            char path[512] = {0};
            read(client_sock, path, sizeof(path));
            printf("[S1] Received DISPFNAMES request for path: %s\n", path);
        
            char *subpath = path + 4; // skip ~S1/
            char fullpath[1024];
            snprintf(fullpath, sizeof(fullpath), "%s/%s/%s", BASE_DIR, SUB_DIR, subpath);
        
            system("rm -f /tmp/cfiles.txt");
            char c_cmd[1024];
            snprintf(c_cmd, sizeof(c_cmd), "find %s -type f -name \"*.c\" | xargs -n 1 basename | sort > /tmp/cfiles.txt", fullpath);
            system(c_cmd);
        
            const char *exts[] = { ".pdf", ".txt", ".zip" };
            int ports[] = { 5001, 5002, 5003 };
            const char *temps[] = { "/tmp/pdffiles.txt", "/tmp/txtfiles.txt", "/tmp/zipfiles.txt" };
        
            for (int i = 0; i < 3; ++i) {
                int s = socket(AF_INET, SOCK_STREAM, 0);
                struct sockaddr_in dest;
                dest.sin_family = AF_INET;
                dest.sin_port = htons(ports[i]);
                dest.sin_addr.s_addr = inet_addr("127.0.0.1");
        
                if (connect(s, (struct sockaddr *)&dest, sizeof(dest)) < 0) {
                    perror("[S1] Could not connect to backend for file list");
                    system("touch /tmp/empty.txt");
                    rename("/tmp/empty.txt", temps[i]);
                    continue;
                }
        
                char request[256] = "__LIST__";
                write(s, request, 256);
                write(s, subpath, 512);
        
                FILE *outfp = fopen(temps[i], "w");
                long len = 0;
                read(s, &len, sizeof(long));
                char buf[1024];
                while (len > 0) {
                    int n = recv(s, buf, sizeof(buf), 0);
                    fwrite(buf, 1, n, outfp);
                    len -= n;
                }
                fclose(outfp);
                close(s);
            }
        
            // Merge all results
            system("cat /tmp/cfiles.txt /tmp/pdffiles.txt /tmp/txtfiles.txt /tmp/zipfiles.txt > /tmp/allfiles.txt");
        
            FILE *final = fopen("/tmp/allfiles.txt", "r");
            fseek(final, 0, SEEK_END);
            long total = ftell(final);
            fseek(final, 0, SEEK_SET);
            write(client_sock, &total, sizeof(long));
        
            char buffer[1024];
            while (!feof(final)) {
                size_t n = fread(buffer, 1, sizeof(buffer), final);
                if (n > 0) send(client_sock, buffer, n, 0);
            }
            fclose(final);
            printf("[S1] Sent file list (%ld bytes)\n", total);
        }
        
    }
    close(client_sock);
}

int main() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    server.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("[S1] Bind failed");
        exit(1);
    }

    listen(sock, 5);
    printf("[S1] Server is running on port %d\n", PORT);

    while (1) {
        int client_sock = accept(sock, NULL, NULL);
        if (client_sock < 0) {
            perror("[S1] Accept failed");
            continue;
        }
        if (fork() == 0) {
            close(sock);
            prcclient(client_sock);
            exit(0);
        }
        close(client_sock);
    }

    return 0;
}
