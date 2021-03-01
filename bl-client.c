#include "blather.h"
#define DEBUG 1
//Global simpio_t struct
simpio_t my_simpio;
simpio_t *simpio = &my_simpio;

//Global client_t struct
client_t my_client;
client_t *client = &my_client;

//Log file file descriptor and semaphore
sem_t *log_sem;
int log_fd;


//Client and server threads.
pthread_t client_thread;
pthread_t server_thread;

//Threads' Functions.
void *client_worker(void *arg) {
  mesg_t message = { .kind = BL_MESG};
  char *buffer = simpio->buf;
  strncpy(message.name,client->name,MAXNAME);
  while(!simpio->end_of_input) {
    simpio_reset(simpio);
    iprintf(simpio, "");                                          // print prompt
    while(!simpio->line_ready && !simpio->end_of_input){          // read until line is complete
      simpio_get_char(simpio);
    }
    if(simpio->line_ready) {
      //If a command, handle it
      if(buffer[0]=='%')
        client_handle_command(buffer);
      //Otherwise, send out the message
      else {
	       strncpy(message.body, buffer, MAXLINE);
	       write(client->to_server_fd, &message, sizeof(mesg_t));
      }
    }
  }
  message.kind = BL_DEPARTED;
  write(client->to_server_fd, &message, sizeof(mesg_t));
  pthread_cancel(server_thread);
  return NULL;
}

void *server_worker(void *arg) {
  mesg_t message;
  int rtnVal;
  int shutdown = 0;
  while(!shutdown) {
    rtnVal = read(client->to_client_fd, &message, sizeof(mesg_t));
    if( rtnVal > 0){
      shutdown = client_print_message(&message);
    }
  }
  pthread_cancel(client_thread);
  return NULL;
}

int main (int argc, char *argv[]){
  //CHECK: Both Server name and Client name are provided
  check_fail(argc < 3, 0, "USAGE: bl-client SERVER_NAME CLIENT_NAME\n");

  int rtnVal;

  //Client Name
  char client_name[MAXNAME];
  strncpy(client_name, argv[2],MAXNAME);
  strncpy(client->name, argv[2], MAXNAME);

  //Client prompt
  snprintf(simpio->prompt, MAXNAME, "%s%s", client_name,PROMPT);

  //Server join fifo
  char join_fname[MAXPATH];
  snprintf(join_fname, MAXPATH, "%s.fifo", argv[1]);

  //to_client fifo
  char to_client_fname[MAXPATH];
  snprintf(to_client_fname, MAXPATH, "%s.client.fifo", client_name);
  remove(to_client_fname); // make sure file removed
  rtnVal = mkfifo(to_client_fname, DEFAULT_PERMS);
  check_fail(rtnVal == -1,1,"Unable to create to_client fifo for client %s", client_name);

  //to_server fifo
  char to_server_fname[MAXPATH];
  snprintf(to_server_fname, MAXPATH, "%s.server.fifo", client_name);
  remove(to_server_fname); // make sure file removed
  rtnVal = mkfifo(to_server_fname, DEFAULT_PERMS);
  check_fail(rtnVal == -1,1,"Unable to create to_server fifo for client %s", client_name);

  //open fifo files
  client->to_client_fd = open(to_client_fname, O_RDWR);
  check_fail(client->to_client_fd == -1,1,"Failed to open fifo %s", to_client_fname);
  client->to_server_fd = open(to_server_fname, O_RDWR);
  check_fail(client->to_server_fd == -1,1,"Failed to open fifo %s", to_server_fname);

  //Initialize a join request
  join_t join_req;
  strncpy(join_req.name, client_name,MAXNAME);
  strncpy(join_req.to_client_fname, to_client_fname,MAXPATH);
  strncpy(join_req.to_server_fname, to_server_fname,MAXPATH);

  //Open a join file descriptor and submit a join request
  int join_fd = open(join_fname, O_WRONLY);
  check_fail(join_fd== -1,1,"Failed to open fifo %s", join_fname);

  rtnVal = write(join_fd, &join_req, sizeof(join_t));
  check_fail(rtnVal <= 0,1,"Failed to write to %s", join_fname);
  close(join_fd);

  //Open log file
  char log_name[MAXPATH];
  snprintf(log_name, MAXPATH, "%s.log", argv[1]);
  log_fd = open(log_name, O_RDONLY);
  check_fail(log_fd == -1,1,"Unable to open log file for server %s", argv[1]);

  //Open a semaphore
  char sem_name[MAXPATH];
  snprintf(sem_name, MAXPATH, "/%s.sem", argv[1]);
  log_sem = sem_open(sem_name, 0);
  check_fail(log_sem == SEM_FAILED, 1, "Unable to open log semaphore for server %s", argv[1]);

  //Initialize Simple Input/Output struct and change term mode
  simpio_set_prompt(simpio, simpio->prompt);
  simpio_reset(simpio);
  simpio_noncanonical_terminal_mode();

  //Pthread create
  pthread_create(&client_thread, NULL, client_worker, NULL);
  pthread_create(&server_thread, NULL, server_worker, NULL);
  pthread_join(client_thread, NULL); // joining threads
  pthread_join(server_thread, NULL);
  close(client->to_client_fd); // close file descriptors
  close(client->to_server_fd);
  close(log_fd);
  sem_unlink(sem_name);
  simpio_reset_terminal_mode();
  printf("\n");
  return 0;
}

void client_handle_command(char *buffer){
  int last_num, rtnVal;
  who_t users;
  mesg_t log_msg = {};

  if(!strncmp(buffer, "%help", 6)) {
    iprintf(simpio, "BLATHER COMMANDS\n");
    iprintf(simpio, "%%help\t: show this message\n");
    iprintf(simpio, "%%who\t: show the current clients\n");
    iprintf(simpio, "%%last N\t: display the last N messages\n");
  }
  else if(!strncmp(buffer, "%who", 5)) {
    sem_wait(log_sem);
    pread(log_fd, &users, sizeof(who_t), 0);
    sem_post(log_sem);
    iprintf(simpio, "====================\n");
    iprintf(simpio, "%d CLIENTS\n", users.n_clients);
    for(int i=0; i<users.n_clients; i++) {
      iprintf(simpio, "%d: %s\n", i, users.names[i]);
    }
    iprintf(simpio, "====================\n");
      }
  else if(!strncmp(buffer, "%last", 5)) {
    rtnVal = sscanf(buffer,"%%last %d", &last_num);
    iprintf(simpio, "====================\n");
    if (rtnVal == 0) {
      iprintf(simpio, "No number of messages specified\n");
    }
    else {
      iprintf(simpio, "LAST %d MESSAGES\n", last_num);
      sem_wait(log_sem);
      lseek(log_fd, (-1)*last_num*sizeof(mesg_t), SEEK_END);
      for(int i=0; i<last_num; i++) {
        rtnVal = read(log_fd, &log_msg, sizeof(mesg_t));
        if (rtnVal < 1)
          break;
        //Print logged message
        rtnVal = client_print_message(&message);  //We're ignoring the return value here
      }
      sem_post(log_sem);
    }
    iprintf(simpio, "====================\n");
      }
}

int client_print_message(mesg_t *message){
  int shutdown = 0;
  switch(message->kind) {
  case BL_MESG:
    iprintf(simpio, "[%s] : %s\n", message->name, message->body);
    break;
  case BL_JOINED:
    iprintf(simpio, "-- %s JOINED --\n", message->name);
    break;
  case BL_DEPARTED:
    iprintf(simpio, "-- %s DEPARTED --\n", message->name);
    break;
  case BL_SHUTDOWN:
    iprintf(simpio, "!!! server is shutting down !!!\n");
    shutdown = 1;
    break;
  //ADVANCED: BL_DISCONNECTED and BL_PING
  case BL_DISCONNECTED:
    iprintf(simpio, "-- %s DISCONNECTED --\n", message->name);
    break;
  case BL_PING:
    write(client->to_server_fd, message, sizeof(mesg_t));
    break;
  default:
    break;
  }
  return shutdown;
}
