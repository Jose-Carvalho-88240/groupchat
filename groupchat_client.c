
/*****************************************************************************/
/*** 						groupchat_client.c                             ***/
/***                                                                       ***/
/***                     Demonstrate a TCP client.                         ***/
/*****************************************************************************/
#include  <sys/syslog.h>
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
#include <sys/stat.h>
#include <fcntl.h>

#define MSGQOBJ_NAME    "/queue"
#define MAX_MSG_LEN 100000
mqd_t msgq_id;
unsigned int sender;
int sd;
char name[20];
struct mq_attr msgq_attr;
int fd=-1;

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

enum clientStatus
{
	AFK=0,
	ONLINE
};
enum clientStatus status;

struct itimerval itv;
#define TIMEOUT 60

const char c_quit[] = "/quit";

unsigned int messages_sent_min = 0;

void panic(char *msg);
#define panic(m)   \
	{              \
		perror(m); \
		abort();   \
	}

void print(char* msg)
{
	char path[30];
	strcpy(path, "/var/log/groupchat_");
	strcat(path, name);
	strcat(path, ".log");
	fd = open(path, O_CREAT | O_WRONLY | O_APPEND);
	if (fd < 0) {
		perror("open");
		exit(EXIT_FAILURE);
	}
	write(fd, msg, strlen(msg));
}

/*
 * Every TIMEOUT checks if the client's status
 * has changed:
 * 			> his messages sent per minute
 * 			> his current status
*/
static void checkStatus(int signo) 
{
	static int previous=-1;
	switch (signo)
	{
	case (SIGALRM):
		/*
		 * If the client is ONLINE and hasn't sent new messages in 
		 * the last minute, change it to AFK
		*/
		if ((messages_sent_min == previous) && status == ONLINE)
		{
			status = AFK;
			print("<< You are now AFK. >>\n");
		}

		/*
		 * If the client is AFK and has sent new messages in 
		 * the last minute, change it to ONLINE
		*/
		else if ((messages_sent_min != previous) && status != ONLINE)
		{
			status = ONLINE;
			print("<< You are ONLINE again. >>\n");
		}

		previous=messages_sent_min;
		messages_sent_min=0;
		break;
	}
}

/*
 * writeFunction deals with the commands
 * from the client and sends to the server
*/
void *writeFunction(void *arg)
{
	int sd = *(int *)arg; /* get & convert the socket */
	char msg[128]; 
	command_t cmd;
	while (1)
	{
		mq_getattr(msgq_id, &msgq_attr);
		if(msgq_attr.mq_curmsgs>0)
		{
			int err = mq_receive(msgq_id,msg,MAX_MSG_LEN,&sender);
			if(err == -1)
			{
				perror("In mqreceive.\n");
				exit(1);
			}

			if (!strncmp(msg, c_quit, 5))
			{
				cmd.type = _KILL;
				send(sd, &cmd, sizeof(cmd), 0);
				print("<< Connection ended. >>\n");
				exit(0);
			}
			else
			{
				messages_sent_min++;
				/*
				* If the client status is AFK, change it 
				* by calling the checkStatus by triggering
				* the timer
				*/
				if (status == AFK)
				{
					//Update local status
					itv.it_interval.tv_sec = TIMEOUT;
					itv.it_interval.tv_usec = 0;
					itv.it_value.tv_sec = 0;
					itv.it_value.tv_usec = 10;
					setitimer(ITIMER_REAL, &itv, NULL);

					//Wait for the interrupt
					pause();

					//Send status to server
					cmd.type=_STATUS;
					send(sd,&cmd,sizeof(cmd),0);
					send(sd,&status,sizeof(status),0);
				}

				cmd.type=_ECHO;
				strcpy(cmd.message,msg);
				send(sd,&cmd,sizeof(cmd),0);
			}
		}
	}
}

/*
 * readFunction deals with the commands
 * received from the server
*/
void *readFunction(void *arg)
{
	int sd = *(int *)arg; /* get & convert the socket */
	command_t cmd;
	while (1)
	{
		if (recv(sd, &cmd, sizeof(cmd), 0) > 0) /* if receive was successful */
		{
			switch (cmd.type)
			{
			/*
			 * Received status request from server:
			 *			> Send to the server a _STATUS 
			 *		command to prepare him to receive
			 *		client's status
			 *			> Send to the server the client's
			 *		status
			 *
			*/
			case _STATUS: 
				send(sd,&cmd,sizeof(cmd),0);
				send(sd,&status,sizeof(status),0);
			break;

			/* prints message received by the server */
			case _ECHO:
				strcat(cmd.message,"\n");
				print(cmd.message);
				break;
			}
		}
	}
}

int main(int count, char *args[])
{
	struct hostent *host;
	struct sockaddr_in addr;
	pid_t pid, sid;
	
	int sd, port;

	if (count != 3)
	{
		printf("usage: %s <servername> <protocol or portnum>\n", args[0]);
		exit(0);
	}

	/*---Get server's IP and standard service connection--*/
	host = gethostbyname(args[1]);
	if (!isdigit(args[2][0]))
	{
		struct servent *srv = getservbyname(args[2], "tcp");
		if (srv == NULL)
			panic(args[2]);
		printf("%s: port=%d\n", srv->s_name, ntohs(srv->s_port));
		port = srv->s_port;
	}
	else
		port = htons(atoi(args[2]));

	/*---Create socket and connect to server---*/
	sd = socket(PF_INET, SOCK_STREAM, 0); /* create socket */
	if (sd < 0)
		panic("socket");
	memset(&addr, 0, sizeof(addr));							/* create & zero struct */
	addr.sin_family = AF_INET;								/* select internet protocol */
	addr.sin_port = port;									/* set the port # */
	addr.sin_addr.s_addr = *(long *)(host->h_addr_list[0]); /* set the addr */


	msgq_id = mq_open(MSGQOBJ_NAME, O_RDWR | O_CREAT , S_IRWXU | S_IRWXG, NULL);
	if (msgq_id == (mqd_t)-1) {
		perror("In mq_open()");
		exit(1);
	}
	/* getting the attributes from the queue   --  mq_getattr() */
	mq_getattr(msgq_id, &msgq_attr);

	/*---If connection successful, send the message and read results---*/
	if (connect(sd, (struct sockaddr *)&addr, sizeof(addr)) == 0)
	{
   	 	printf("Enter your name: ");
		scanf("%[^\n]", name);

		send(sd,name,strlen(name),0);
		status=ONLINE;

		printf("<< Welcome %s! >>\n", name);

		if(recv(sd, &fd, sizeof(fd), 0) < 0)
			exit(0);

		signal(SIGALRM, checkStatus);

		itv.it_interval.tv_sec = TIMEOUT;
		itv.it_interval.tv_usec = 0;
		itv.it_value.tv_sec = 0;
		itv.it_value.tv_usec = 100;
		setitimer(ITIMER_REAL, &itv, NULL);

		pid = fork(); // create a new process

		if (pid < 0) { // on error exit
			syslog(LOG_ERR, "%s\n", "fork");
			exit(EXIT_FAILURE);
		}

		if (pid > 0){  
			printf("Deamon PID: %d\n", pid);	
			exit(EXIT_SUCCESS); // parent process (exit)
		}
		sid = setsid(); // create a new session

		if (sid < 0) { // on error exit
			syslog(LOG_ERR, "%s\n", "setsid");
			exit(EXIT_FAILURE);
		}
		// make '/' the root directory
		if (chdir("/") < 0) { // on error exit
			syslog(LOG_ERR, "%s\n", "chdir");
			exit(EXIT_FAILURE);
		}
		umask(0);
		close(STDIN_FILENO);  // close standard input file descriptor
		close(STDOUT_FILENO); // close standard output file descriptor
		close(STDERR_FILENO); // close standard error file descriptor

		print("<< Welcome! >>\n");
		
		pthread_t write, read;
		pthread_create(&read, 0, readFunction, &sd);
		pthread_create(&write, 0, writeFunction, &sd);

		pthread_join(write,(void*)&sd);
		pthread_join(read,(void*)&sd);

		pthread_exit(NULL);

	}

	else
		panic("connect");
}
