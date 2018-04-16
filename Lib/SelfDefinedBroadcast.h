#include "Struct.h"

using namespace std;

/*
  Our broadcast function (highly similar to the scatter)
  1. Master sends all the messages that are to be sent to that world to the postman.
  2. After postman receives the messages, it will put the mail into the all the mailBoxes in the mailRoom
  3. All the threads in all the worlds will try to fetch mails from their own mailBoxes
*/
void SELF_DEFINED_BROADCAST(
  MailRoom& mailRoom,
  vector<double>* buffer,
  int num_messages, int message_size, MPI_Datatype dt,
  int my_rank, int my_tid,
  int sender_rank, int sender_tid,
  int postman_tid,
  int world_size, int num_threads,
  int option){

    /* ---------- Receive ----------*/
    if (option == 1){
      //Step 3
      pair<int, int> sender_info = mailRoom.fetchMail(my_tid, &(*buffer)[0], "BROADCAST");
      return;
    }

    //Sender rountine
    if (my_rank == sender_rank && my_tid == sender_tid){
      MPI_Bcast(&(*buffer)[0],  num_messages, dt, sender_rank, MPI_COMM_WORLD);
      //Step 2 part 2, Put the message into ech mailbox
      for (int t = 0; t < num_threads; t++){
        mailRoom.putMail(buffer, t, sender_rank, sender_tid, "BROADCAST");
      }
    }

    //Postman routine
    if (my_tid == postman_tid && my_rank != sender_rank){
      //Step 2 part 1, Receive the message
      MPI_Bcast(&(*buffer)[0],  num_messages, dt, sender_rank, MPI_COMM_WORLD);
      //Step 2 part 2, Put the message into ech mailbox
      for (int t = 0; t < num_threads; t++){
        mailRoom.putMail(buffer, t, sender_rank, sender_tid, "BROADCAST");
      }
    }
}
