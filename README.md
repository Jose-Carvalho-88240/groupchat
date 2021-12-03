# Compiling and running the program

To **compile the program**:

- For the host:
    
        make all
        
- For the target:  
    
        make RASPall


To **clean the compiled images**:

    make clean


To **run the program**:

- CONNECT THE SERVER

    - Runs server on the host
    
            make run.server
    
    - Runs server on the target
    
            make RASPrun.server
        
    - General command
    
            ./server.elf (PORT)

- CONNECT THE CLIENT

    - Runs the client on the host when target is not connected: IP 127.0.0.1
    
            make run.client
    
    - Runs the client on the host when target is connected: IP 10.42.0.242
    
            make run.client.rasp

    - General command
    
            ./client.elf (IP) (PORT)
            
- SEND MESSAGES

        ./send.elf (message)
        
    **( ! )** there's the possibility to disconnect from the
    server by typing **/quit** on the message field
            
    **( ! )** messages and other system info is stored in
    **/var/log/groupchat_(client name).log**

# System Behaviour

Aspects to take into account:

  > Once the **server is open** and the **clients are connected to it** it's possible to communicate with other clients via the 
file **groupchat_client_send.c** 

  > One essential thing to consider in this group chat is that **clients are connected in different devices and the network
is the same** (since the TCP IP connection is local, since there's no port forwarding). This implicates that message
queues (used to communicate from the groupchat_client_send to the client) are unique for each device.

System behaviour:

  > To know who is **connected to the server**, the user must type **/clients**. This request would transfered to the client
daemon, which would then parse the command and send it to the server. The server, upon receiving the command, would return
to the daemon the **clients' name list** which would be returned to the terminal of which the user sent the initial command.

  > To **create or join a new channel**, in order to send messages to specific clients, the user must type the command
**/send.elf /channel <name1> <name2>**. This would create a channel specific to the user, client 1 and client 2. In order to send messages 
to everyone connected to the server, one could use the command **./send.elf /channel all**.

  > To send a message the command **/send.elf <message>** is used. This would redirect the <message> to the clients specified in the
previous /channel command. 
    
# Tasks Overview
    
The entities of the program are:
    
    Client
    Client Message Manager Daemon
    Server

The Client process will:

      - Open the message queue on which the Client Daemon is
    listening to

      - Send the message or command sent by the user using the
    command /send (...)

      - Close the message queue

      - Exit the process

The Client Message Manager Daemon:

  > Main process:

    - Connect to the message queue that the user will
    communicate with

    - Establish the TCP-IP connection with the server

    - Ask user name

    - Send the user information to the server

    - Run on the background

    - Create two threads: read and write

  > Read Task
    
    - Receives command from the server

    - Parse the command

    - Run the command:
    
        - echo: print the message to a log file

        - client: print clients' information to
        a log file

  > Write Task
    - Checks if there are messages left on the message
    queue

    - If there are, process them based on the information:
    
        - Whether it contains command (/channel or /clients)
        or message

        - If the user is AFK and sent the message, put the status
        as ONLINE

        - Run the command
    
            - message: send the echo command with the message to
            the server

            - channel: send the channel command with the users to
            the server

            - client: send the client command to the server

The server:

  > The server main process:

    - Open the TCP-IP connection

    - Accept and process clients' information

    - Opens chat thread for each client

  > The chat thread will:
    
    - Receive commands from the Client's Daemon

    - Process each command and run it:
    
        - message: echo the message to all the users specified
        in the channel of the user

        - channel: change the users' channel

        - client: return to the Client's Daemon the list of
        clients connected to the server
