### Building and running your application

When you're ready, accept your application by running:
`docker compose up --build`.

MY STEPS OF CREATING AN CACHE PROXY
by YUXUAN YANG
DUKE UNIVERSITY
22 FEB 2022

(1) When proxy accept, the first thing is to become a daemon and establish a socket connection for listening incomming connections.

(2) Proxy will set a Server TCP socket on port 12345, thus the proxy will listen on 12345 for incomming client connections.

(3) Multiple client may request at the same time, thus a multi-processing strategy strategy is used

(4) netcat can be used to send request from a client to proxy.

(5) proxy handle the request, parse, open a Client TCP connection to server, send request, get response, parse response, cache, write response back to client

===================================================================================================
Step 1 socket establishment
socket.hpp socket.cpp

Write class buildServer to establish a socket, bind it on port and listen to the port for server. Write class acceptConnection to accept call and return descriptor

Write class buildClient to establish a socket, bind it on port and listen to the port for client.

Daemon cycle: Startup => Initial Setup => Become Daemon => While(true){} => ShutDown

For while(true) session related to socket:
while(true){
req = accept();
resp = process_request(req);
send_response(req,resp);
}
We need to use Parallelism to process multi request at the same time
compare Parallelism and Concurrency:
Parallelism: Executing more than one thing at the same time
Concurrency: Making progress on more than on thing at a time(does not need to be done at             the same time), might just exists at the same time.
And Parallelism belongs to Concurrency

Parallelism I: fork per-request:
while (true) {
req = accept();
pid_t p = fork();
if (p < 0) {/*handle error */}
else if (p == 0) {
resp = process_request(req);
send_response(req,resp);
exit(EXIT_SUCCESS);
}
//cleanup: close/free req
//need to wait for p w/o blocking
}
Parallelism II: pre-fork, note that in request_loop continue do 3 process mentioned above
for (int i = 0; i < n_procs; i++) {
//set up IPC here
pid_t p = fork();
if (p < 0) { /* handle error */}
else if (p == 0) {
request_loop(); //never returns
abort(); //unreachable
}
else {
children.push_back(p);
}
}


How to Make program to be a daemon
Writing C code programming
What you should do to let your program be a daemon

1.  use fork( ) to create a child process(with PID)
    see man page credentials

    pid_t cpid = fork() //created child process;
    if(cpid == -1){exit(EXIT_FAILURE);}
    if(cpid >0){exit(EXIT_SUCCESS);}// here leave parent process


        pid_t pid = getpid() get process ID (PID) // defined in <sys/types.h>
        note that PID will preserved across an execve()
    
        setsid() create new sessions
        All processes in a process group share the same process group ID, which is the process group            leader. All processes in a session share the same PID of process who called setsid()

        getuid() getgid()
        UserID type uid_t, GroupID type gid_t //defined in <sys/types.h>

2.  dissociate from controlling tty(use setsid( ))

    what is tty: linux terminal(tty1 tty2)
    what is controlling tty: tty related to process session, send the session leader/other members/s            ignals to help to control
    why we must disassociate daemon process from the tty: leave it's original process group, avoid s    ending more signals related to terminal operation

    pid_t spid = setsid(); // create new ID for child process
    if(cpid == -1){exit(EXIT_FALURE);}


3.  point stdin/stdout/stderr at /dev/null( use dup2( ))

    stdout file descriptor is 1
    stderr file descriptor is 2

    To dump all output to void(that won't be useful), we redirect them to dev/null
    for example if we don't need all stdout, but we only need stderr, we can dump stdout to null and redirect stderr to stdout by:
    grep -r <some command> /sys/ > /dev/null 2>&1
    if we want to redirect both to null we can do as:
    grep -r <some command> /sys/ &> /dev/null

    To redirect all in C code:


    fd = open("/dev/null",O_RDWR, 0);//with 3 permission O_RDONLY O_WRONLY or O_RDWR(Read Write Respectively)

    //dup2(int oldfd, int newfd);
    //dup2 duplicate file descriptor thus newfd point to oldfd
    
    if (fd != -1)  
    {  
        dup2 (fd, STDIN_FILENO);//STDIN_FILENO redirect to fd(/dev/null) file descriptor  
        dup2 (fd, STDOUT_FILENO);  //same
        dup2 (fd, STDERR_FILENO);  //same
  
        if (fd > 2)  
        {  
            close (fd);  
        }  
    }  


4. Chdir to "/"
   /* Change the current working directory. */  
   if ((chdir("/")) < 0)  
   {  
   exit(EXIT_FAILURE);  
   }
5. set umask to 0
   //change permission mask to number&0777
   //the following change it to 0
   //for security reason
   umask(0);
6. use fork( ) again to make the process not a session leader
   cpid = fork();

To using docker container as testing environment, we need to config it.

Note that when first fork() called, Docker's  container exits and terminate as well. To avoid this, using bash script as entry point and use that script to call proxy then have a while 1 loop in that script.

Also need to detach the container from current session as well, run "docker-compose up -d". to reattach using "docker attach <container-id>"

===================================================================================================
Step 2 Writing While(true){} Proxy Creating TCP socket on port 12345 and use netcat approach to send client request to proxy.

Daemon cycle: Startup => Initial Setup => Become Daemon => While(true){} => ShutDown
For the whole while part, the process divided it into following parts


while(true){
//1. Client constructs request message
//2. Client send request to proxy
//3. Proxy listen on for receive message
//4. Proxy Purses message received
//5. Proxy Interpretes the message(Communicate to server, error checking)
//5. Proxy open an connection to server, pass client request
//6. Server listen on connection for a request
//7. Server Purses message received
//8. Server Interprets the message
//9. Server Respond to the request
//10 Proxy listen on for respond
//11 Proxy respond to Client
//12 Client examines received responses
//13 Client interpret the results

                req = accept();
                resp = process_request(req);
                send_response(req,resp);
        }

Thus Proxy performs both like Client and Server

