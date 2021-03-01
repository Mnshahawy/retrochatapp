#include "blather.h"
#include <errno.h>

//Global signaled flag
volatile sig_atomic_t signaled = 0;
volatile sig_atomic_t alarmed = 0;

//Create a server
server_t my_server ={};
server_t *server = &my_server;

//Signal Handelr Function
void sig_handler(int sig){
  if(sig == SIGALRM) alarmed =1 ;
  else signaled = 1;
}

//Pthread for writing who_t
void *act_users_worker(void *arg){
  while(!signaled){
    if(alarmed) server_write_who(server);
  }
  return NULL;
}

int main(int argc, char *argv[]) {

  int rtnVal;  //To hold return values for sysCalls or functions.

  //CHECK: A server name was provided.
  check_fail(argc < 2, 0, "USAGE: bl-server SERVER_NAME\n");

  //Signal Handler
  struct sigaction sig_act = {};
  sig_act.sa_handler = sig_handler;
  sigaction(SIGTERM, &sig_act, NULL);
  sigaction(SIGINT,  &sig_act, NULL);
  sigaction(SIGALRM, &sig_act, NULL);

  //Create a pthread
  pthread_t act_users;

  //Start the Server.
  server_start(server, argv[1],DEFAULT_PERMS);

  //Start alarm
  alarm(1);

  //Start the active users thread
  pthread_create(&act_users, NULL, act_users_worker, NULL);

  //Indefinite Loop
  while(!signaled) {

    //Update resources
    server_check_sources(server);

    if(alarmed) {
      alarmed = 0;
      alarm(1);
      server_tick(server);
      server_ping_clients(server);
      server_remove_disconnected(server, 10);
    }

    //If join is available, handle the request
    if(server_join_ready(server)) {
      rtnVal = server_handle_join(server);
      check_fail(rtnVal == -1,0,"%s\n", strerror(errno));
    }

    //If a client's data is ready, process the data.
    for(int i=0; i< my_server.n_clients; i++) {
      if(server_client_ready(server,i)) {
	rtnVal = server_handle_client(server,i);
	check_fail(rtnVal == -1,0,"%s\n", strerror(errno));
      }
    }
  }

  //Shutdown the server after being signaled.
  puts("Shuting Down...");
  pthread_join(act_users, NULL);
  server_shutdown(server);
  return 0;
}
