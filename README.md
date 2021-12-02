# Compiling and running the Group Chat
To compile the program:

- For the host:
    
        make all
        
- For the target:  
    
        make RASPall

To clean the compiled images:

    make clean

To run the program:

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
