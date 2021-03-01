#include "blather.h"

int main (int argc, char *argv[]){

  //CHECK: Log file name is provided
  check_fail(argc < 2, 0, "USAGE: bl-showlog LOG_FILE\n");

  //Structs for messages and who_t
  who_t clients;
  mesg_t message;

  //Open the file
  int log_fd = open(argv[1], O_RDONLY);
  check_fail(log_fd == -1,1,"Failed to open log file (%s)", argv[1]);

  //Read the who_t struct
  read(log_fd, &clients, sizeof(who_t));

  // Clients header
  printf("%d CLIENTS\n", clients.n_clients);

  //Print the clients
  for(int i=0; i<clients.n_clients; i++) {
    printf("%d: %s\n", i, clients.names[i]);
  }

  //Messages header
  printf("MESSAGES\n");

  // print the messages
  while(read(log_fd, &message, sizeof(mesg_t)) > 0) {
    switch(message.kind) {
    case BL_MESG:
      printf("[%s] : %s\n", message.name, message.body);
      break;
    case BL_JOINED:
      printf("-- %s JOINED --\n", message.name);
      break;
    case BL_DEPARTED:
      printf("-- %s DEPARTED --\n", message.name);
      break;
    case BL_SHUTDOWN:
      printf("!!! server is shutting down !!!\n");
      break;
    case BL_DISCONNECTED:
      printf("-- %s DISCONNECTED --\n", message.name);
      break;
    default:
      break;
    }
  }

  return 0;
}
