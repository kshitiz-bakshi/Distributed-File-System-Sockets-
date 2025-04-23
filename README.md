# Distributed File System (Using Socket Programming)

This project is a **Distributed File System** where files are uploaded, downloaded, and managed across multiple servers. The system uses **socket programming** to allow communication between the client and servers. The client only interacts with the main server (S1), and S1 handles file distribution to other servers.

## Features

- **File Upload**: Clients can upload `.c`, `.pdf`, `.txt`, and `.zip` files to the main server (S1). S1 stores `.c` files locally and forwards other file types to secondary servers (S2, S3, S4).
- **File Download**: Clients can download files from S1. If the requested file is stored on a secondary server (S2, S3, or S4), S1 will fetch it and send it to the client.
- **File Management**: Supports file deletion and listing of files across servers.
- **Communication**: All servers communicate using sockets, and the client only communicates with S1, which hides the complexity of file distribution.

## Architecture

- **S1 (Main Server)**: Handles client requests. It stores `.c` files locally and forwards other file types to S2, S3, and S4.
- **S2, S3, S4 (Secondary Servers)**: Store `.pdf`, `.txt`, and `.zip` files, respectively.
- **Client**: The client interacts only with S1, which abstracts the underlying server distribution logic.

## How to Use

1. **Clone the Repository**:
   ```bash
   git clone <repository-url>
Compile the Code: In the project folder, compile the servers and client:

bash
Copy
gcc S1.c -o S1
gcc S2.c -o S2
gcc S3.c -o S3
gcc S4.c -o S4
gcc w25clients.c -o w25clients
Run the Servers: Open four terminals and run each of the following commands in separate terminals:

Terminal 1: ./S1

Terminal 2: ./S2

Terminal 3: ./S3

Terminal 4: ./S4

Run the Client: In a separate terminal, start the client:

bash
Copy
./w25clients
Available Commands
uploadf <filename> <destination_path>: Upload a file to S1. .pdf, .txt, and .zip files will be forwarded to the corresponding secondary server (S2, S3, S4).

downlf <filename>: Download a file from S1 (it may be fetched from another server).

removef <filename>: Delete a file from the system.

downltar <filetype>: Download a .tar file containing all files of a specific type (.c, .pdf, .txt).

dispfnames <pathname>: Display the names of all files in a specified directory.

Technologies Used
C Programming

Socket Programming

Client-Server Architecture

Unix/Linux
