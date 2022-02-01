#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <signal.h>

#define SUCCESS 0
#define FAILURE 1
#define FALSE 0
#define TRUE 1
#define SPECIAL_ERROR 2
#define N_LEN_IN_BYTES 4
#define NO_CLIENT -1
#define MIN_PRINTABLE_CHAR 32
#define MAX_PRINTABLE_CHAR 126
#define PRINTABLE_CHARS_NUM 95

int connection = NO_CLIENT;
int sigint_during_communication = FALSE;
uint32_t pcc_total[PRINTABLE_CHARS_NUM] = {0};
uint32_t client_ppc[PRINTABLE_CHARS_NUM] = {0};

//==================== DECLARATIONS ===========================
int is_it_a_tcp_error();
void print_printable_char_counter();
void sigint_handler();
void sigint_handler();
int init_sigint_handler();
int send_bytes(int fd, void *data, size_t overall_len);
int read_bytes(int fd, void *data, size_t overall_len);
void init_client_ppc();
void update_pcc_total();
uint32_t printable_chars_counter(char *data, size_t len);
int main(int argc, char *argv[]);

//================== IMPLEMENTATION ===========================
int is_it_a_tcp_error() {
	if (errno == ETIMEDOUT || errno == ECONNRESET || errno == EPIPE) {
		return TRUE;
	}
	return FALSE;
}

void print_printable_char_counter() {
	for (int i=0; i<PRINTABLE_CHARS_NUM; i++) {
		printf("char '%c' : %u times\n", i+MIN_PRINTABLE_CHAR, pcc_total[i]);
	}
    exit(SUCCESS);
}

void sigint_handler() {
	if (connection >= 0) {
		sigint_during_communication = TRUE;
		return;
	}
	print_printable_char_counter();
}

int init_sigint_handler() {
    struct sigaction new_action;
	memset(&new_action, 0, sizeof(new_action));
    new_action.sa_flags = SA_RESTART;
	new_action.sa_handler  = &sigint_handler;
	if (sigaction(SIGINT, &new_action, 0) != 0) {
		perror("sigaction() failed");
		return FAILURE;
	}
	return SUCCESS;
}

int send_bytes(int fd, void *data, size_t overall_len) {
    int write_bytes = 0;
    size_t overall_bytes_sent = 0;
    while (overall_bytes_sent < overall_len) {
        write_bytes = write(fd, overall_bytes_sent+data, overall_len-overall_bytes_sent);
        if (write_bytes == -1) {
            if (is_it_a_tcp_error()) {
                return SPECIAL_ERROR;
            }
		    return FAILURE;
    }
    overall_bytes_sent += write_bytes;
  }
  return SUCCESS;
}

int read_bytes(int fd, void *data, size_t overall_len) {
	int read_bytes = 0;
    size_t overall_bytes_read = 0;
	while (overall_bytes_read < overall_len) {
		read_bytes = read(fd, overall_bytes_read+data, overall_len-overall_bytes_read);
		if (read_bytes == -1) {
			if (is_it_a_tcp_error()) {
				return SPECIAL_ERROR;
			}
			return FAILURE;
		}
		if (read_bytes == 0) {
			return SPECIAL_ERROR;
		}
		overall_bytes_read += read_bytes;
	}
	return SUCCESS;
}
void init_client_ppc() {
    for (int i=0; i<PRINTABLE_CHARS_NUM; i++) {
		client_ppc[i] = 0;
	}
}
uint32_t printable_chars_counter(char *data, size_t len) {
	char curr;
    uint32_t printable_chars_num = 0;
	init_client_ppc(client_ppc);
	for (int i=0; i<len; i++) {
		curr = data[i];
		if(curr>=MIN_PRINTABLE_CHAR && curr<=MAX_PRINTABLE_CHAR) {
			client_ppc[curr-MIN_PRINTABLE_CHAR] += 1;
            printable_chars_num +=1;
		}
	}
	return printable_chars_num;
}

void update_pcc_total() {
    for (int i=0; i<PRINTABLE_CHARS_NUM; i++) {
		pcc_total[i] += client_ppc[i];
	}
}


int main(int argc, char *argv[]) {
	int server_port, error_flag = SUCCESS, server_fd;
  	char *data;
	uint32_t data_len = 0, printable_chars_num = 0;
	struct sockaddr_in serv_addr, peer_addr;
    socklen_t sockaddr_len;

	if (argc != 2) {
		fprintf(stderr, "one argument was expected but there wasn't exactly one\n");
		exit(FAILURE);
	}
	server_port = atoi(argv[1]);
	init_sigint_handler();

	//create a binded listening socket
	server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd == -1) {
		perror("socket() failed");
		exit(FAILURE);
	}
    sockaddr_len = sizeof(struct sockaddr_in);
    memset(&serv_addr, 0, sockaddr_len);
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port =htons(server_port);

	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0) {
    	perror("setsockopt() failed");
		exit(FAILURE);
	}

	if(0 != bind(server_fd, (struct sockaddr*) &serv_addr, sockaddr_len)) {
		perror("bind() failed");
		exit(FAILURE);
	}

	if(0 != listen(server_fd, 10)) {
		perror("listen() failed");
		exit(FAILURE);
	}

	//work for clients loop
	while(TRUE) {
		connection = NO_CLIENT;
        //if there was a SIGNIT during the last connection, we'll finish.
		if (sigint_during_communication) {
			print_printable_char_counter();
		}

		connection = accept(server_fd, (struct sockaddr*)&peer_addr, (socklen_t*)&sockaddr_len);
		if (connection < 0) {
			perror("accept() failed");
			exit(FAILURE);
		}

		error_flag = read_bytes(connection, (char*)&data_len, N_LEN_IN_BYTES);
		if (error_flag == FAILURE) {
			perror("read_bytes() failed");
			exit(FAILURE);
		}
		if (error_flag == SPECIAL_ERROR) {
			perror("read_bytes() failed due to a TCP error or that the client is killed unexpectedly during read");
			close(connection);
			continue;
		}

		data_len = ntohl(data_len);		
		data = (char*)malloc(data_len);
		if (data == NULL) {
			perror("malloc() failed");
			exit(FAILURE);
		}
		error_flag = read_bytes(connection, data, data_len);
		if (error_flag == FAILURE) {
			free(data);
			perror("read_bytes() failed");
			exit(FAILURE);
		}
		else if (error_flag == SPECIAL_ERROR) {
			free(data);
			perror("read_bytes() failed due to a TCP error or that the client is killed unexpectedly during read");
			close(connection);
			continue;
		}
		
		printable_chars_num = printable_chars_counter(data, data_len);
		printable_chars_num = htonl(printable_chars_num);
		error_flag = send_bytes(connection, (char*)&printable_chars_num, N_LEN_IN_BYTES);
		if (error_flag == FAILURE) {
			perror("send_bytes() failed");
			exit(FAILURE);
		}
		else if (error_flag == SPECIAL_ERROR) {
			perror("send_bytes() failed due to a TCP error");
			close(connection);
			continue;
		}
        update_pcc_total();
        free(data);
		close(connection);
	}
}