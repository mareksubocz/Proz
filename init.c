#include "main.h"
#include "init.h"
#include "watek_komunikacyjny.h"

int rank, size;

void check_thread_support(int provided){
    printf("THREAD SUPPORT: %d\n", provided);
    switch (provided) {
        case MPI_THREAD_SINGLE: 
            printf("Brak wsparcia dla wątków, kończę\n");
            /* Nie ma co, trzeba wychodzić */
        fprintf(stderr, "Brak wystarczającego wsparcia dla wątków - wychodzę!\n");
        MPI_Finalize();
        exit(-1);
        break;
        case MPI_THREAD_FUNNELED: 
            printf("tylko te wątki, ktore wykonaly mpi_init_thread mogą wykonać wołania do biblioteki mpi\n");
        break;
        case MPI_THREAD_SERIALIZED: 
            /* Potrzebne zamki wokół wywołań biblioteki MPI */
            printf("tylko jeden watek naraz może wykonać wołania do biblioteki MPI\n");
        break;
        case MPI_THREAD_MULTIPLE: printf("Pełne wsparcie dla wątków\n");
        break;
        default: printf("Nikt nic nie wie\n");
    }
}

/* sprawdza, czy są wątki, tworzy typ MPI_PAKIET_T
*/
void inicjuj(int *argc, char ***argv)
{
    int provided;
    MPI_Init_thread(argc, argv,MPI_THREAD_MULTIPLE, &provided);
    check_thread_support(provided);

    const int nitems=FIELD_NUM; /* bo packet_t ma trzy pola */
    int       blocklengths[FIELD_NUM] = {[0 ... FIELD_NUM-1] = 1};
    MPI_Datatype typy[FIELD_NUM] = {[0 ... FIELD_NUM-1]=MPI_INT};

    MPI_Aint     offsets[FIELD_NUM]; 
    offsets[0] = offsetof(packet_t, ts);
    offsets[1] = offsetof(packet_t, src);
    offsets[2] = offsetof(packet_t, data);
    offsets[3] = offsetof(packet_t, dst);
    offsets[4] = offsetof(packet_t, pyrkon_number);

    MPI_Type_create_struct(nitems, blocklengths, offsets, typy, &MPI_PAKIET_T);
    MPI_Type_commit(&MPI_PAKIET_T);

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    srand(rank);

    pthread_create( &threadKom, NULL, startKomWatek , 0);

}

/* usunięcie zamków, czeka, aż zakończy się drugi wątek, zwalnia przydzielony typ MPI_PAKIET_T
   wywoływane w funkcji main przed końcem
*/
void finalizuj()
{
    pthread_mutex_destroy( &stateMut);
    /* Czekamy, aż wątek potomny się zakończy */
    println("czekam na wątek \"komunikacyjny\"\n" );
    pthread_join(threadKom,NULL);
    if (rank==0) pthread_join(threadMon,NULL);
    MPI_Type_free(&MPI_PAKIET_T);
    MPI_Finalize();
}


