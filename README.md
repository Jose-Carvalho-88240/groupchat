# groupchat
>To compile the program:

    > make all (compiled for the host)
    > make RASPall (compiled for the target)

>To clean the compiled images:

    > make clean

>To run the program:

    - CONNECT THE SERVER
        > make run.server (runs server on the host)
        
        > make RASPrun.server (runs server on the target)
        > ./server.elf (PORT)

    - CONNECT THE CLIENT
        > make run.client (runs the client on the host
        when target is not connected: IP 127.0.0.1)

        > make run.client.rasp (runs the client on the host
        when target is connected: IP 10.42.0.242)

    - SEND MESSAGES
        > ./send.elf (message)
            (!) there's the possibility to disconnect from the
            server by typing /quit on the message field
            
            (!) messages and other system info is stored in
            /var/log/groupchat_(client name).log
    
