#include "main.h"
#include "watek_komunikacyjny.h"

void *startKomWatek(void *ptr)
{
    MPI_Status status;
    packet_t pakiet;

    while ( !end ) {
        MPI_Recv( &pakiet, 1, MPI_PAKIET_T, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

        pthread_mutex_lock(&lamport_mutex);
            lamport_clock = MAX(lamport_clock, pakiet.ts)+1;
        pthread_mutex_unlock(&lamport_mutex);

        pakiet.src = status.MPI_SOURCE;

        debug("RECEIVED_MESSAGE: %d in %s on pyrkon %d received %s from %d on pyrkon %d", rank, state_strings[get_state()],get_pyrkon_number(), message_strings[status.MPI_TAG], pakiet.src, pakiet.pyrkon_number);
        if(pakiet.pyrkon_number == get_pyrkon_number()){
            handlers[(int)status.MPI_TAG](&pakiet);
        }
        
    }
}
