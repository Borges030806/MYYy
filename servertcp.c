#include<stdio.h>
#include<sys/types.h>//socket
#include<sys/socket.h>//socket
#include<string.h>//memset
#include<stdlib.h>//sizeof
#include<netinet/in.h>//INADDR_ANY
#include <sys/time.h> 
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <fcntl.h>


#define PORT "4950"  // the port users will be connecting to
#define BACKLOG 5	 // how many pending connections queue will hold
#define SECRETSTRING "628496"
#define MAX_BUF_SIZE 4096
#define TIMEOUT_SECONDS 10
#define MAX_BACKLOG_SIZE 3
pthread_mutex_t mutex;

int childCnt;
char answerString[20]; 
int answerInt;
float answerFloat;
int flag;

/* 
	Calculate the difference between to timevals; store result in an timeval. 
	syntax: a,b,c.
	Result: c=a-b;
*/
int timeval_subtract (struct timeval *result, struct timeval *x, struct timeval *y){
  /* Perform the carry for the later subtraction by updating y. */
  if (x->tv_usec < y->tv_usec) {
    int nsec = (y->tv_usec - x->tv_usec) / 1000000 + 1;
    y->tv_usec -= 1000000 * nsec;
    y->tv_sec += nsec;
  }
  if (x->tv_usec - y->tv_usec > 1000000) {
    int nsec = (x->tv_usec - y->tv_usec) / 1000000;
    y->tv_usec += 1000000 * nsec;
    y->tv_sec -= nsec;
  }

  /* Compute the time remaining to wait.
     tv_usec is certainly positive. */
  result->tv_sec = x->tv_sec - y->tv_sec;
  result->tv_usec = x->tv_usec - y->tv_usec;

  /* Return 1 if result is negative. */
  return x->tv_sec < y->tv_sec;
}


void *get_in_addr(struct sockaddr *sa){
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}


//Convert a struct sockaddr address to a string, IPv4 and IPv6:
char *get_ip_str(const struct sockaddr *sa, char *s, size_t maxlen){
    switch(sa->sa_family) {
        case AF_INET:
            inet_ntop(AF_INET, &(((struct sockaddr_in *)sa)->sin_addr),
                    s, maxlen);
            break;

        case AF_INET6:
            inet_ntop(AF_INET6, &(((struct sockaddr_in6 *)sa)->sin6_addr),
                    s, maxlen);
            break;

        default:
            strncpy(s, "Unknown AF", maxlen);
            return NULL;
    }

    return s;
}

char* generate_operation_string() {
	
    // Define an array of operators
    const char* operators[8] = {"add", "div", "mul", "sub", "fadd", "fdiv", "fmul", "fsub"};
    // Select a random operator
    const char* operation = operators[rand() % 8];
    // Define an array of strings to store the result, leaving enough space
    char* result = (char*)malloc(50 * sizeof(char));
    if (result == NULL) {
        printf("Memory allocation failed.\n");
        exit(1);
    }

    if (strcmp(operation, "add") == 0 || strcmp(operation, "div") == 0 ||
        strcmp(operation, "mul") == 0 || strcmp(operation, "sub") == 0) {
		
		flag = 0;

        // If the first four operators are selected, two random integers are generated
        int value1 = rand() % 100;
        int value2 = rand() % 100;
        // Calculates the result according to the operator
        if (strcmp(operation, "add") == 0) {
            answerInt = value1 + value2;
        } else if (strcmp(operation, "div") == 0) {
            if (value2 != 0) {
                answerInt = value1 / value2;
            } else {
                printf("Error: division by zero.\n");
                exit(1);
            }
        } else if (strcmp(operation, "mul") == 0) {
            answerInt = value1 * value2;
        } else if (strcmp(operation, "sub") == 0) {
            answerInt = value1 - value2;
        }

		// Convert answerInt to a string using sprintf
    	sprintf(answerString, "%d", answerInt);

        // combines the operator and two integers into a string
        sprintf(result, "%s %d %d\n", operation, value1, value2);
    } else {

		flag = 1;

        // If the last four operators are selected, two random floating point numbers are generated
        float value1 = (float)(rand() % 10000) / 100;
        float value2 = (float)(rand() % 10000) / 100;
        // Calculates the result according to the operator
        if (strcmp(operation, "fadd") == 0) {
            answerFloat = value1 + value2;
        } else if (strcmp(operation, "fdiv") == 0) {
            if (value2 != 0) {
                answerFloat = value1 / value2;
            } else {
                printf("Error: division by zero.\n");
                exit(1);
            }
        } else if (strcmp(operation, "fmul") == 0) {
            answerFloat = value1 * value2;
        } else if (strcmp(operation, "fsub") == 0) {
            answerFloat = value1 - value2;
        }

		// Convert answerInt to a string using sprintf
    	sprintf(answerString, "%f", answerFloat);

        // Combines the operator and two floating point arrays into a string
        sprintf(result, "%s %f %f\n", operation, value1, value2);
    }

    return result;
}

// Thread function: listens to the connection queue size and closes the tail-end connection
void* monitor_backlog(void* arg) {
    while (1) {

        pthread_mutex_lock(&mutex);
		int listenfd = *((int*)arg);
		
		
        // Gets the size of the connection queue
		int backlog_size;
		if (fcntl(listenfd, F_GETFD, &backlog_size) == -1) {
			perror("fcntl");
			exit(EXIT_FAILURE);
		}
		//printf("++++++++++++++%d\n",backlog_size);
        if (backlog_size > MAX_BACKLOG_SIZE) {
			//printf("KILL  Thread+++++++++++++++++++++++++\n");
            // Close the connection at the end of the line
            for (int i = 0; i < backlog_size - BACKLOG; ++i) {
                int connfd = accept(listenfd, NULL, NULL);
                if (connfd == -1) {
                    perror("accept");
                    continue;
                }
                if (i == backlog_size - BACKLOG - 1) {
                    // Only close the connection at the end of the line
                    close(connfd);
                    printf("Closed the last connection in backlog.\n");
                } else {
                    close(connfd);
                }
            }
        }

        pthread_mutex_unlock(&mutex);

        // Check again after a period of sleep
        sleep(1);
    }
    return NULL;
}

static const char* program_name;

int main(int argc, char *argv[]){
	int listenfd;//to create socket
	int connfd;//to accept connection
	childCnt=0;// Will increment for each spawned child.

	
	struct addrinfo hints, *servinfo, *p;
	int rv;

	
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // Use the local IP address

	// Obtain local address information
	if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// Iterate through the obtained address information, create a socket and bind the address
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((listenfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
			perror("server: socket");
			continue;
		}

		if (bind(listenfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(listenfd);
			perror("server: bind");
			continue;
		}

		break; // If the binding succeeds, the loop exits
	}

	// Set the timeout period
    struct timeval timeout;
    timeout.tv_sec = TIMEOUT_SECONDS;
    timeout.tv_usec = 0;
    if (setsockopt(listenfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

	freeaddrinfo(servinfo); // Release the address information structure

	if (p == NULL)  {
		fprintf(stderr, "server: failed to bind\n");
		return 1;
	}
		
	//Listening
	if (listen(listenfd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}


	printf("server: waiting for connections...\n");

	struct sockaddr_in serverAddress;//server receive on this address
	struct sockaddr_in clientAddress;//server sends to client on this address
	struct sockaddr_storage their_addr;
 
	int n;
	char msg[1450];
	char cli[INET6_ADDRSTRLEN];
	int clientAddressLength;
	int pid;
	childCnt = 0;
	int readSize;
	char command[10];
	char optionstring[128];

	const char* separator = strrchr(argv[0], '/');
	if ( separator ){
		program_name = separator + 1;
	} else {
		program_name = argv[0];
	}


	// Initializes the mutex
    if (pthread_mutex_init(&mutex, NULL) != 0) {
        perror("pthread_mutex_init");
        exit(EXIT_FAILURE);
    }

    // Create a thread
    pthread_t thread;
    if (pthread_create(&thread, NULL, monitor_backlog, (void*)&listenfd) != 0) {
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

	while(1) {
		socklen_t sin_size = sizeof their_addr;
		connfd = accept(listenfd, (struct sockaddr *)&their_addr, &sin_size);
		if (connfd == -1) {
			//perror("accept");
			continue;
		}

		// Get client IP address
		char clientIP[INET6_ADDRSTRLEN];
		inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), clientIP, sizeof clientIP);
		//printf("server: got connection from %s\n", clientIP);
		printf("server: Connection %d from %s\n",childCnt, clientIP);


		
		struct sockaddr_in *local_sin=(struct sockaddr_in*)&their_addr;

		printf("server: Sending protocol string \n");
		char protocol_string[] = "TEXT TCP 1.0\n\n";
		if (send(connfd, protocol_string, strlen(protocol_string), 0) == -1) {
			perror("send");
			close(connfd);
			continue; // Continue listening for the next connection
		}

		
		readSize=recv(connfd,&msg,sizeof(msg)-1,0);
		if (readSize < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				// Receive timeout
				printf("Receive timeout occurred.\n");
				// Execute the processing operation after the timeout
				if (send(connfd, "ERROR to", 8, 0) == -1){
					perror("send");
					close(connfd);
					continue; //leave loop execution, go back to the while, main accept() loop. 
				}
				close(connfd);
				continue; // Continue listening for the next connection
			} else {
				//perror("recv");
				exit(EXIT_FAILURE);
			}
		} else if (readSize == 0) {
			// The peer end closes the connection
			printf("Connection closed by peer.\n");
		} 

		sscanf(msg,"%s ",command);
		if(strcmp(command,"OK")==0){

		}
		else{
			printf("error!");
			break;
		}
		// Handle client connection here
		
		while (1) {

			// Generate an action string
			char* operation_string = generate_operation_string();

			// Send the operation string to the client
			send(connfd, operation_string, strlen(operation_string), 0);
			printf("Sent question to client: %s", operation_string);

			// Free the memory of the operation string
			free(operation_string);

			// Receive client messages
			readSize = recv(connfd, &msg, sizeof(msg) - 1, 0);
			if (readSize < 0) {
				if (errno == EAGAIN || errno == EWOULDBLOCK) {
					// Receive timeout
					printf("Receive timeout occurred.\n");
					// Execute the processing operation after the timeout
					if (send(connfd, "ERROR to", 8, 0) == -1){
						perror("send");
						close(connfd);
						continue; //leave loop execution, go back to the while, main accept() loop. 
					}
					close(connfd);
					//continue; // Continue listening for the next connection
					break;
				} else {
					//perror("recv");
					exit(EXIT_FAILURE);
				}
			} else if (readSize == 0) {
				// The peer end closes the connection
				printf("Connection closed by peer.\n");
			} 
			printf("Child[%d] (%s:%d): recv(%d) .\n", childCnt, clientIP, ntohs(local_sin->sin_port), readSize);
			if (readSize == 0) {
				printf("Child [%d] died.\n", childCnt);
				close(connfd);
				break;
			}
			msg[readSize] = 0;

			int rv = sscanf(msg, "%s ", command);
			printf("rv=%d Decoded command as: %s \n", rv, command);

			// the result is two decimal places to compare with the predefined answers
			if(flag == 0){
				int answer = atoi(command);
				if (answer == answerInt) {
					printf("OK\n");
				} else {
					printf("ERROR\n");
					printf("Answer is : %s \n", answerString);
					break; // If the results are not equal, exit the loop
				}
			}
			else{
				float answer = atof(command);
				if ((answer-answerFloat)<0.0001 && (answer-answerFloat)>-0.0001) {
					printf("OK\n");
				} else {
					printf("ERROR\n");
					printf("Answer is : %s \n", answerString);
					break; // If the results are not equal, exit the loop
				}
			}


			//initiative closing
			if (strcmp(command, "close") == 0) {
				// Client sent 'command', check that it provided the correct magic word.
				rv = sscanf(msg, "%s %s", command, optionstring);
				printf("rv=%d Decoded command + option as: %s %s\n", rv, command, optionstring);
				if (strcmp(optionstring, SECRETSTRING) == 0) {
					printf("Yes, master I'll close.\n");
					sprintf(msg, "Perfect!.");
					send(connfd, &msg, strlen(msg), 0);
					shutdown(connfd, SHUT_RDWR);
					close(connfd);
					break;
				}
			}

		}

		printf("Socket closed()\n");
		
		// Close the connection
		// Destroy the mutex

    	pthread_mutex_destroy(&mutex);
		close(connfd);
	}


	
 
 return 0;
}
