#include "blather.h"
#define DEBUG 1

client_t *server_get_client(server_t *server, int idx) {
  //CHECK: client number is valid
  if(idx >= server->n_clients) {
    perror("server_get_client: index beyond n_clients\n");
    return NULL;
  }

  return &server->client[idx];
}

void server_start(server_t *server, char *server_name, int perms) {
  
  strncpy(server->server_name, server_name, MAXPATH);
  char fifo_name[MAXPATH];
  snprintf(fifo_name, MAXPATH, "%s.fifo", server_name);
  remove(fifo_name);

  int rtnVal = mkfifo(fifo_name, perms);
  check_fail(rtnVal == -1,1,"Unable to create join fifo for server %s", server_name);

  server->join_fd = open(fifo_name,O_RDWR);
  check_fail(server->join_fd == -1,1,"Unable to open join fifo for server %s", server_name);
  server->join_ready = 0;
  server->n_clients = 0;

  // ADVANCED feature below, start logfile and semaphore
  char log_name[MAXPATH];
  snprintf(log_name, MAXPATH, "%s.log", server_name);
  remove(log_name);
  server->log_fd = open(log_name, O_CREAT | O_RDWR, DEFAULT_PERMS);
  check_fail(server->log_fd == -1,1,"Unable to create log file for server %s", server_name);

  char sem_name[MAXPATH];
  snprintf(sem_name, MAXPATH, "/%s.sem", server_name);
  server->log_sem = sem_open(sem_name, O_CREAT, DEFAULT_PERMS, 1);
  check_fail( server->log_sem == SEM_FAILED, 1, "Unable to create log semaphore for server %s", server_name);
  server->time_sec = 0; // set tick to zero
  server_write_who(server);
  // don't use following if server_write_who is using thread to complete
  lseek(server->log_fd, sizeof(who_t), SEEK_SET);
}

void server_shutdown(server_t *server) {
  close(server->join_fd);
  char fifo_name[MAXPATH] = {}; // join fifo filename
  snprintf(fifo_name, MAXPATH, "%s.fifo", server->server_name);
  remove(fifo_name);
  mesg_t shutdown_msg = { .kind = BL_SHUTDOWN, {}, {}}; // shutdown message
  server_broadcast(server, &shutdown_msg);
  for (int i=server->n_clients-1; i>=0; i--) {
    server_remove_client(server, i);
  }
  // ADVANCED: close log file and log semaphore
  close(server->log_fd);
  char sem_name[MAXPATH];
  snprintf(sem_name, MAXPATH, "/%s.sem", server->server_name);
  sem_unlink(sem_name);
}

int server_add_client(server_t *server, join_t *join) {
  if (server->n_clients == MAXCLIENTS) {
    return -1;
  }
  // pointer to the new client
  client_t *new_client = &(server->client[server->n_clients]);
  // filling in the fields
  strncpy(new_client->name, join->name, MAXPATH);
  strncpy(new_client->to_client_fname, join->to_client_fname, MAXPATH);
  strncpy(new_client->to_server_fname, join->to_server_fname, MAXPATH);
  // client file
  new_client->to_client_fd = open(new_client->to_client_fname,O_RDWR);
  check_fail(new_client->to_client_fd == -1,1,"Unable to open to_client fifo for client %s", new_client->name);
  // server file
  new_client->to_server_fd = open(new_client->to_server_fname, O_RDWR);
  check_fail(new_client->to_server_fd == -1,1,"Unable to open to_server fifo for client %s", new_client->name);
  new_client->data_ready = 0;
  new_client->last_contact_time = server->time_sec; // set the contact time to current
  server->n_clients++;
  return 0;
}

int server_remove_client(server_t *server, int idx) {
  if (idx >= server->n_clients)
    return -1;
  client_t *current = &server->client[idx];
  close(current->to_client_fd);
  remove(current->to_client_fname);
  close(current->to_server_fd);
  remove(current->to_server_fname);
  if (idx == server->n_clients-1) {
    server->n_clients--;
    return 0;
  }
  *current = server->client[server->n_clients-1]; // assign current with last
  server->n_clients--;
  return 0;
}

int server_broadcast(server_t *server, mesg_t *mesg) {
  int result, flag = 0;
  for (int i=0; i<server->n_clients; i++) {
    result = write(server->client[i].to_client_fd, mesg, sizeof(mesg_t));
    if (result == -1) {
      flag = -1; // return error is -1 indicate an error
    }
  }
  // ADVANCED: log unless this is a ping
  if(mesg->kind != BL_PING) {
    server_log_message(server, mesg);
  }
  return flag;
}

void server_check_sources(server_t *server) {
  dbg_printf("checking sources\n");
  fd_set server_set;
  FD_ZERO(&server_set);

  //Set the join fd
  FD_SET(server->join_fd, &server_set);

  //Set all clients' FDs
  int maxfd_server =server->join_fd;
  for(int i=0; i<server->n_clients; i++) {
    FD_SET(server->client[i].to_server_fd, &server_set);
    if(server->client[i].to_server_fd > maxfd_server)
      maxfd_server = server->client[i].to_server_fd;
  }
  //Select
  if (select(maxfd_server+1, &server_set, NULL, NULL, NULL) == -1)
    return;

  //Check if set
  for(int i=0; i<server->n_clients; i++) {
    if(FD_ISSET(server->client[i].to_server_fd, &server_set))
      server->client[i].data_ready = 1;
  }
  if(FD_ISSET(server->join_fd, &server_set)) {
    server->join_ready = 1;
    puts("received join request");
  }
}

int server_join_ready(server_t *server){
  return server->join_ready;
}

int server_handle_join(server_t *server){
  join_t newReq;

  server->join_ready = 0;
  int rtnVal = read(server->join_fd, &newReq, sizeof(join_t));
  if(rtnVal == -1) {
    perror("Join FIFO read failed");
    return -1;
  }

  if (server_add_client(server, &newReq) == -1) {
    perror("Failed to add client. Reached Maximum number of clients");
    return -1;
  }

  //Create a JOINED message
  mesg_t joined_msg = {.kind = BL_JOINED};
  strncpy(joined_msg.name, newReq.name,MAXNAME);

  //Broadcast JOINED message
  if (server_broadcast(server, &joined_msg) == -1) {
    perror("Failed to broadcast a JOINED message");
    return -1;
  }

  return 0;
}

int server_client_ready(server_t *server, int idx){
  return server->client[idx].data_ready;
}

int server_handle_client(server_t *server, int idx){
  server->client[idx].data_ready=0;

  mesg_t newMsg;
  int rtnVal = read(server->client[idx].to_server_fd,&newMsg, sizeof(mesg_t));

  if(rtnVal == -1) {
    perror("Failed to read a message from client");
    return -1;
  }

  switch(newMsg.kind) {
  case BL_MESG:
    server_broadcast(server, &newMsg);
    break;
  case BL_DEPARTED:
    server_broadcast(server, &newMsg);
    server_remove_client(server, idx);
    break;
  case BL_PING:
    server->client[idx].last_contact_time = server->time_sec;
    break;
  default: break;
  }
  return 0;
}


void server_write_who(server_t *server){
  who_t act_users= {.n_clients = server->n_clients};
  for(int i=0; i < act_users.n_clients; i++){
    strncpy(act_users.names[i], server->client[i].name, MAXNAME);
  }

  sem_wait(server->log_sem);
  pwrite(server->log_fd, &act_users, sizeof(who_t),0);
  sem_post(server->log_sem);
}
  
void server_tick(server_t *server) {
  server->time_sec++;
}

void server_ping_clients(server_t *server) {
  mesg_t message = { .kind = BL_PING };
  server_broadcast(server, &message);
}

void server_remove_disconnected(server_t *server, int timeout) {
  mesg_t message = { .kind = BL_DISCONNECTED };
  for(int i=0; i<server->n_clients; i++) {
    if((server->time_sec - server->client[i].last_contact_time)>=timeout) {
      strncpy(message.name, server->client[i].name, MAXNAME); // get the name
      server_remove_client(server, i); // remove
      server_broadcast(server, &message);
    }
  }
}

void server_log_message(server_t *server, mesg_t *mesg) {
  int rtnVal = write(server->log_fd, mesg, sizeof(mesg_t));
  check_fail(rtnVal < 1,1,"Unable to log message.");
}
