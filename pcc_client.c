#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>

#define SUCCESS 0
#define FAILURE 1
#define N_LEN_IN_BYTES 4
//==================== DECLARATIONS ===========================
int create_client_socket();
void send_bytes(int fd, void *data, size_t overall_len);
void read_bytes(int fd, void *data, size_t overall_len);
int main(int argc, char *argv[]);

//================== IMPLEMENTATION ===========================
int create_client_socket() {
  int client_fd;
  client_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (client_fd == -1) {
    perror("socket() failed");
    exit(FAILURE);
  }
  return client_fd;
}

void send_bytes(int fd, void *data, size_t overall_len) {
  int write_bytes = 0;
  size_t overall_bytes_sent = 0;
  while (overall_bytes_sent < overall_len) {
    write_bytes = write(fd, overall_bytes_sent+data, overall_len-overall_bytes_sent);
    if (write_bytes == -1) {
      perror("send() failed");
      exit(FAILURE);
    }
    overall_bytes_sent += write_bytes;
  }
  return;
}

void read_bytes(int fd, void *data, size_t overall_len) {
  int bytes_read = 0;
  size_t overall_bytes_read = 0;
  while (overall_bytes_read < overall_len) {
    bytes_read = read(fd, overall_bytes_read+data, overall_len-overall_bytes_read);
    if (bytes_read == -1) {
      perror("read() failed");
      exit(FAILURE);
    }
    overall_bytes_read += bytes_read;
  }
  return;
}

int main(int argc, char *argv[]) {
  int server_port, client_fd;
  char *server_ip, *file_path, *data_to_send;
  FILE *fp;
  struct sockaddr_in serv_addr;
  struct stat file_info;
  size_t data_len;
  uint32_t file_len, network_file_len, printable_char_num = 0; 
  //file_len = N, network_file_len = N (in network byte order), printable_char_num = C.

  //input handling
  if (argc != 4) {
    fprintf(stderr, "3 arguments were expected but there weren't exactly 3 of them\n");
    exit(FAILURE);
  }
  server_ip = argv[1];
  server_port = atoi(argv[2]);
  file_path = argv[3];

  //file init
  fp = fopen(file_path, "rb"); //open a binary file for reading
  if (fp == NULL) {
    perror("fopen failed()");
    exit(FAILURE);
    }
  if (stat(file_path, &file_info) == -1) {
    perror("stat() failed");
    exit(FAILURE);
  }
  file_len = file_info.st_size;

  //client socket & connection init
  client_fd = create_client_socket();
  memset(&serv_addr, 0, sizeof(struct sockaddr_in));
  serv_addr.sin_family = AF_INET;
  if (inet_pton(AF_INET, server_ip, &serv_addr.sin_addr.s_addr) != 1) {
    perror("inet_pton() failed");
    exit(FAILURE);
  }
  serv_addr.sin_port = htons(server_port);
  if(0 != connect(client_fd,
                  (struct sockaddr*) &serv_addr,
                  sizeof(struct sockaddr_in))) {
    perror("connect() failed");
    exit(FAILURE);
  }

  //The client sends the server the number of bytes that will be transferred.
  network_file_len = htonl(file_len);
  send_bytes(client_fd, &network_file_len, N_LEN_IN_BYTES);

  //The client sends the server N bytes (the file’s content).
  data_to_send = (char*)malloc(file_len);
  if (data_to_send == NULL) {
    perror("malloc() failed");
    exit(FAILURE);
	}
  data_len = fread(data_to_send, 1, file_len, fp);
  if (data_len != file_len) {
    perror("fread() failed");
    exit(FAILURE);
  }
  send_bytes(client_fd, data_to_send, data_len);

  //The server sends the client C, the number of printable characters.
  read_bytes(client_fd, &printable_char_num, N_LEN_IN_BYTES);
  printable_char_num = ntohl(printable_char_num);
  printf("# of printable characters: %u\n", printable_char_num);

  free(data_to_send);
  close(client_fd);
  fclose(fp);
  exit(SUCCESS);
}