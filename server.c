#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <time.h>

#define PORT 8080
#define BUFFER_SIZE 1024

pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

struct client_info {
	int fd;
	struct in_addr addr;
};

void *handle_client(void *arg);
void send_error_page(int client_fd);
void log_request(const char *client_ip, const char *method, const char *path, int status_code);

int main() {
	int server_fd, client_fd;
	struct sockaddr_in server_addr, client_addr;
	socklen_t addr_len = sizeof(client_addr);

	server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd == -1) {
		perror("socket failed");
		exit(EXIT_FAILURE);
	}


	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_port = htons(PORT);

	if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
		perror("bind failed");
		close(server_fd);
		exit(EXIT_FAILURE);
	}


	if (listen(server_fd, 10) < 0) {
		perror("listen failed");
		close(server_fd);
		exit(EXIT_FAILURE);
	}
	printf("Server listening on port %d...\n", PORT);

	while(1) {
		client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
		if (client_fd < 0) {
			perror("accept failed");
			continue;
		}
		
		struct client_info *info = malloc(sizeof(struct client_info));
		info->fd = client_fd;
		info->addr = client_addr.sin_addr;

		pthread_t tid;
		if (pthread_create(&tid, NULL, handle_client, info) != 0) {
			perror("pthread_create failed");
			close(client_fd);
			free(info);
			continue;
		}
		printf("Client connected: %s\n", inet_ntoa(client_addr.sin_addr));
		pthread_detach(tid);
	}

	close(server_fd);
	return 0;
}

void *handle_client(void *arg) {
	struct client_info *info = (struct client_info *)arg;
	int client_fd = info->fd;
	char *client_ip = inet_ntoa(info->addr);

	char buffer[BUFFER_SIZE];
	ssize_t bytes_read;

	bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
	if (bytes_read < 0) {
		perror("recv failed");
		close(client_fd);
		return NULL;
	}
	buffer[bytes_read] = '\0';
	printf("Request:\n%s\n", buffer);

	if (strncmp(buffer, "GET ", 4) == 0) {
		char method[8], resource[1024];
		sscanf(buffer, "%s %s", method, resource);

		if (resource == NULL) {
			printf("Bad request\n");
			close(client_fd);
			return NULL;
		}

		if (strcmp(resource, "/") == 0) {
			strcpy(resource, "/index.html");
		}
		printf("Requested resource: %s\n", resource);

		char filepath[BUFFER_SIZE];
		snprintf(filepath, sizeof(filepath), "html%s", resource);
		printf("Serving file: %s\n", filepath);

		FILE *fp = fopen(filepath, "r");
		if (fp == NULL) {
			log_request(client_ip, method, resource, 404);
			send_error_page(client_fd);
			close(client_fd);
			return NULL;
		}

		fseek(fp, 0, SEEK_END);
		long file_size = ftell(fp);
		rewind(fp);

		char *file_content = malloc(file_size + 1);
		if (!file_content) {
			perror("malloc failed");
			fclose(fp);
			close(client_fd);
			return NULL;
		}
		fread(file_content, 1, file_size, fp);
		file_content[file_size] = '\0';
		fclose(fp);

		char header[BUFFER_SIZE];
		snprintf(header, sizeof(header),
			"HTTP/1.1 200 OK\r\n"
			"Content-Type: text/html\r\n"
			"Content-Length: %ld\r\n"
			"Connection: close\r\n"
			"\r\n", file_size
		);
		send(client_fd, header, strlen(header), 0);
		send(client_fd, file_content, file_size, 0);
		
		log_request(client_ip, method, resource, 200);
		free(file_content);
	} else {
		log_request(client_ip, "UNKNOWN", "/", 400);
		char error_response[] = 
			"HTTP/1.1 400 Bad Request\r\n"
			"Content-Type: text/html\r\n"
			"Connection: close\r\n"
			"\r\n"
			"<html><body><h1>400 Bad Request</h1></body></html>";
		send(client_fd, error_response, sizeof(error_response) - 1, 0);
	}
	close(client_fd);
	return NULL;
}

void send_error_page(int client_fd) {
	FILE *not_found_page = fopen("html/404.html", "r");
	if (not_found_page) {
		fseek(not_found_page, 0, SEEK_END);
		long size = ftell(not_found_page);
		rewind(not_found_page);

		char *body = malloc(size + 1);
		fread(body, 1, size, not_found_page);
		body[size] = '\0';
		fclose(not_found_page);	
			
		char not_found_response[BUFFER_SIZE];
		snprintf(not_found_response, sizeof(not_found_response),
			"HTTP/1.1 404 Not Found\r\n"
			"Content-Type: text/html\r\n"
			"Content-Length: %ld\r\n"
			"Connection: close\r\n"
			"\r\n", size);
		send(client_fd, not_found_response, strlen(not_found_response), 0);
		send(client_fd, body, size, 0);
		free(body);
	} else {
		const char *fallback = 
			"HTML/1.1 404 Not Found\r\n"
			"Content-Type: text/plain\r\n"
			"Connection: close\r\n"
			"\r\n"
			"404 Not Found";
		send(client_fd, fallback, strlen(fallback), 0);
	}
	return;
}

void log_request(const char *client_ip, const char *method, const char *path, int status_code) {
	pthread_mutex_lock(&log_mutex);

	FILE *log = fopen("server.log", "a");
	if (log == NULL) {
		perror("log file open failed");
		pthread_mutex_unlock(&log_mutex);
		return;
	}

	time_t now = time(NULL);
	struct tm *t = localtime(&now);
	char timebuf[64];
	strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", t);

	fprintf(log, "[%s] %s \"%s %s\" %d\n", timebuf, client_ip, method, path, status_code);
	fclose(log);

	pthread_mutex_unlock(&log_mutex);
}
