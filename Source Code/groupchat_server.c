#include <stdio.h>
#include <sys/socket.h>
#include <mqueue.h>	/* mq_* functions */
#include <sys/types.h>
#include <resolv.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <sys/time.h>
#include <unistd.h>

void panic(char *msg);
#define panic(m)   \
	{              \
		perror(m); \
		abort();   \
	}

#define MAX_CLIENT_NUM 3

#define CHECK_STATUS_TIME 5

/*The set of possible commands*/
#define NULL_COMMAND 0
#define _ECHO 1
#define _STATUS 2
#define _KILL 3

typedef struct command command_t;
struct command
{
	unsigned int type;
	char message[128];
};
command_t cmd;

enum clientStatus
{
	AFK = 0,
	ONLINE = 1
};

typedef struct client_socket_info client_socket_info_t;
struct client_socket_info
{
	int socket;
	enum clientStatus status;
	int index;
	char name[20];
	
};

client_socket_info_t socket_table[MAX_CLIENT_NUM];

int threads = 0;

#define MAX_CLIENT_NUM			10
#define MAX_MSG_LEN     100000

int fd0;
#define ledON 1
#define ledOFF 0

void initLED()
{
	system("insmod led.ko");
	fd0 = open("/dev/led0", O_WRONLY);
	system("echo none >/sys/class/leds/led0/trigger");
}

void led()
{
	static int i=ledON;
	printf("SWITCHING LED\n");
	write(fd0, &i, 1);
	if(i)
		i=ledOFF;	
	else
		i=ledON;	
	printf("SWITCHED\n");
}
/*
 * Periodic update from the server to all 
 * clients to request his status
*/
static void sendPeriodicUpdate(int signo)
{
	int i;

	switch (signo)
	{
	case (SIGALRM):
		cmd.type = _STATUS;
		for (i = 0; i < MAX_CLIENT_NUM; i++)
		{
			send(socket_table[i].socket, &cmd, sizeof(cmd), 0);
		}
		break;
	}
}

/*
 * chatFunction deals with all the the commands
 * sent from the clients
*/
void *chatFunction(void *arg)
{
	command_t cmd;
	client_socket_info_t *info = (client_socket_info_t *)arg; /* get & convert the socket */
	enum clientStatus stat;

	while (1)
	{
		if(recv(info->socket, &cmd, sizeof(cmd), 0) > 0)
		{
			switch (cmd.type)
			{
			// Status response from the client
			case _STATUS:
				if (recv(info->socket, &stat, sizeof(stat), 0) > 0)
				{
					// If the client's status has changed, change it
					if (info->status != stat)
					{
						info->status = stat;
						if (stat == AFK)
							printf("%s is now AFK.\n", info->name);
						else
							printf("%s is now ONLINE.\n", info->name);
					}
				}
				else
					printf("No response from %s.\n", info->name);
				break;

			// Echo the message from one client to all
			case _ECHO:
				led();

				printf("%s wants to echo < %s > to all.\n", info->name, cmd.message);

				// Adds the sender's name to the message
				char temp[128];
				strcpy(temp, info->name);
				strcat(temp, ": ");
				strcpy(cmd.message, strcat(temp, cmd.message));

				// Echo to all clients
				for (int i = 0; i < MAX_CLIENT_NUM; i++)
				{
					if (socket_table[i].status)
					{
						send(socket_table[i].socket, &cmd, sizeof(cmd), 0);
					}			
				}
				break;

			case _KILL:
				printf("%s has left.\n", info->name);

				socket_table[info->index].status = AFK; /* marked as a already closed channel*/

				threads--;

				// char path[30];
				// strcpy(path, "rm groupchat_");
				// strcat(path, info->name);
				// strcat(path, ".log");
				// system(path);

				// Shuts down the socket
				shutdown(info->socket, SHUT_RD);
				shutdown(info->socket, SHUT_WR);
				shutdown(info->socket, SHUT_RDWR);

				// If there aren't any clients left, close the server
				if (!threads)
				{
					close(fd0);
					printf("All clients left, server shutting down...\n");
					exit(0);
				}

				pthread_exit(NULL);
				
			case NULL_COMMAND:
				break;
			}
		}		
	}
	return 0; /* terminate the thread */
}

int main(int count, char *args[])
{
	struct sockaddr_in addr;
	int listen_sd, port;

	if (count != 2)
	{
		printf("usage: %s <protocol or portnum>\n", args[0]);
		exit(0);
	}

	/*---Get server's IP and standard service connection--*/
	if (!isdigit(args[1][0]))
	{
		struct servent *srv = getservbyname(args[1], "tcp");
		if (srv == NULL)
			panic(args[1]);
		printf("%s: port=%d\n", srv->s_name, ntohs(srv->s_port));
		port = srv->s_port;
	}
	else
		port = htons(atoi(args[1]));

	/*--- create socket ---*/
	listen_sd = socket(PF_INET, SOCK_STREAM, 0);
	if (listen_sd < 0)
		panic("socket");

	/*--- bind port/address to socket ---*/
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = port;
	addr.sin_addr.s_addr = INADDR_ANY; /* any interface */
	if (bind(listen_sd, (struct sockaddr *)&addr, sizeof(addr)) != 0)
		panic("bind");

	/*--- make into listener with 3 slots ---*/
	if (listen(listen_sd, MAX_CLIENT_NUM) != 0)
		panic("listen")

		/*--- begin waiting for connections ---*/
		else
		{
			struct itimerval itv;

			signal(SIGALRM, sendPeriodicUpdate);

			itv.it_interval.tv_sec = CHECK_STATUS_TIME;
			itv.it_interval.tv_usec = 0;
			itv.it_value.tv_sec = CHECK_STATUS_TIME;
			itv.it_value.tv_usec = 0;
			setitimer(ITIMER_REAL, &itv, NULL);

			pthread_t threadArray[MAX_CLIENT_NUM];
			int j;

			initLED();
			while (1) /* process all incoming clients */
			{
				int n = sizeof(addr);
				int sd = accept(listen_sd, (struct sockaddr *)&addr, &n); /* accept connection */
				
				if (sd != -1 && threads < MAX_CLIENT_NUM)
				{
					/* 
					 *	Looking for an empty space in the 
					 *  socket table (might not necessarily
					 *  be the last position of this array)
					*/
					for(j=0; j<MAX_CLIENT_NUM; j++)
					{
						if(!socket_table[j].status)
						{
							break;
						}
					}

					socket_table[j].socket = sd;
					socket_table[j].status = ONLINE; /*means connection opened*/
					socket_table[j].index = j;
					if(recv(sd,socket_table[j].name,sizeof(socket_table[j].name),0)>0)
					{
						printf("%s has entered.\n", socket_table[j].name);
	
						// Creates a thread for each client
						pthread_create(&threadArray[j], 0, chatFunction, &socket_table[j]); /* start thread */
						pthread_detach(threadArray[j]);		/* don't track it */

						threads++;
					}	
				}
			}
		}
}
