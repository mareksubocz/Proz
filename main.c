#include "main.h"

MPI_Datatype MPI_PAKIET_T;
pthread_t threadKom, threadMon; 

int pyrkon_number=0;
int pyrkon_escapees_number=0;
int lamport_clock=0;
int my_messages_lamport_clocks[] = {[0 ... LECTURES_NUMBER] = 0};
int my_received_ack[] = {[0 ... LECTURES_NUMBER] = 0};

/////////////////////////////////////////////////
                  /////////////

// Inicjalizacja zmiennych z main.h
pthread_mutex_t stateMut = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t pyrkon_number_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t lamport_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t pyrkon_escapees_number_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t my_messages_lamport_clocks_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t wait_to_enter_pyrkon = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t my_received_ack_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t finish_current_pyrkon = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t waiting_for_ack_mutex = PTHREAD_MUTEX_INITIALIZER;

state_t stan=init_pyrkon;
const char* message_strings[] = {"WANT_PYRKON_TICKET", "WANT_PYRKON_TICKET_ACK" ,"LEFT_PYRKON" };
const char* state_strings[] = {"init_pyrkon", "on_pyrkon","finish_pyrkon"};
//nie zapomnij dopisac nowych do convert_message_to int

f_w handlers[3] = {
    [WANT_PYRKON_TICKET] = want_pyrkon_ticket_handler,
    [WANT_PYRKON_TICKET_ACK] =  want_pyrkon_ticket_ack_handler,
    [LEFT_PYRKON] = someone_left_pyrkon_handler,
};

extern void mainLoop(void);
extern void inicjuj(int *argc, char ***argv);
extern void finalizuj(void);

int main(int argc, char **argv)
{
    /* Tworzenie wątków, inicjalizacja itp */
    inicjuj(&argc,&argv); // tworzy wątek komunikacyjny w "watek_komunikacyjny.c"
    mainLoop();          // w pliku "watek_glowny.c"
    finalizuj();
    return 0;
}

void send_message(message_t message, state_t state){
    packet_t packet;
    packet.ts = get_increased_lamport_clock();
    packet.pyrkon_number = get_pyrkon_number();

    if(message==WANT_PYRKON_TICKET)
        my_messages_lamport_clocks[0] = packet.ts;

    for(int i=0; i<size; i++){
        if(i!=rank){
            sendPacket(&packet, i, message);
            debug("SENDING_REQUEST: %d in %s sends %s to %d on pyrkon %d\n",rank, state_strings[state], message_strings[message], i, packet.pyrkon_number);
        }
    }
}

void sendPacket(packet_t *pkt, int destination, message_t message){
    int freepkt=0;
    if (pkt==0) { pkt = malloc(sizeof(packet_t)); freepkt=1;}
    pkt->src = rank;
    pkt->dst = destination;
    int tag = convert_message_to_int(message);
    MPI_Send( pkt, 1, MPI_PAKIET_T, destination, tag, MPI_COMM_WORLD);
    if (freepkt) free(pkt);
}

int convert_message_to_int(message_t message){
    switch (message){
        case WANT_PYRKON_TICKET:
            return 0;
        case WANT_PYRKON_TICKET_ACK:
            return 1;
        case LEFT_PYRKON:
            return 2;

        default: return -1;
    }
}


void changeState( state_t newState )
{
    pthread_mutex_lock(&stateMut);
    if (end) { 
        pthread_mutex_unlock(&stateMut);
        return;
    }
    stan = newState;
    pthread_mutex_unlock(&stateMut);
}

int get_increased_lamport_clock(){
    pthread_mutex_lock(&lamport_mutex);
        lamport_clock ++;
        int clk = lamport_clock;
    pthread_mutex_unlock(&lamport_mutex);
    return clk;
}

int get_my_messages_lamport_clocks(int index){
    pthread_mutex_lock(&my_messages_lamport_clocks_mutex);
        int clk = my_messages_lamport_clocks[index];
    pthread_mutex_unlock(&my_messages_lamport_clocks_mutex);
    return clk;
}

int get_my_received_ack(int index){
    pthread_mutex_lock(&my_received_ack_mutex);
        int count = my_received_ack[index];
    pthread_mutex_unlock(&my_received_ack_mutex);
    return count;
}

int get_pyrkon_number(){
    pthread_mutex_lock(&pyrkon_number_mutex);
        int number = pyrkon_number;
    pthread_mutex_unlock(&pyrkon_number_mutex);
    return number;
}

int get_state(){
    pthread_mutex_lock(&stateMut);
        int state = stan;
    pthread_mutex_unlock(&stateMut);
    return state;
}

int get_pyrkon_escapees_number(){
    pthread_mutex_lock(&pyrkon_escapees_number_mutex);
        int number = pyrkon_escapees_number;
    pthread_mutex_unlock(&pyrkon_escapees_number_mutex);
    return number;
}

void my_received_ack_increase(int index){
    pthread_mutex_lock(&my_received_ack_mutex);
        my_received_ack[index]++;
    pthread_mutex_unlock(&my_received_ack_mutex);
}

void pyrkon_number_increase(){
    pthread_mutex_lock(&pyrkon_number_mutex);
        pyrkon_number++;
    pthread_mutex_unlock(&pyrkon_number_mutex);
}

void pyrkon_escapees_number_increase(){
    pthread_mutex_lock(&pyrkon_escapees_number_mutex);
        pyrkon_escapees_number++;
    pthread_mutex_unlock(&pyrkon_escapees_number_mutex);
}

void pyrkon_escapees_number_reset(){
    pthread_mutex_lock(&pyrkon_escapees_number_mutex);
        pyrkon_escapees_number=0;
    pthread_mutex_unlock(&pyrkon_escapees_number_mutex);
}

void my_messages_lamport_clocks_reset(){
    pthread_mutex_lock(&my_messages_lamport_clocks_mutex);
        memset(my_messages_lamport_clocks, 0, LECTURES_NUMBER*sizeof(int));
    pthread_mutex_unlock(&my_messages_lamport_clocks_mutex);
}

void my_received_ack_reset(){
    pthread_mutex_lock(&my_received_ack_mutex);
        memset(my_received_ack, 0, LECTURES_NUMBER*sizeof(int)); 
    pthread_mutex_unlock(&my_received_ack_mutex);
}

void waiting_for_ack_reset(){
    pthread_mutex_lock(&waiting_for_ack_mutex);
    waiting_for_ack = malloc(size * sizeof(packet_t));
    for(int i=0; i<size; i++){
        waiting_for_ack[i].ts = -111;
    }
    pthread_mutex_unlock(&waiting_for_ack_mutex);
}

void lamport_clock_reset(){
    pthread_mutex_lock(&lamport_mutex);
        lamport_clock=0;
    pthread_mutex_unlock(&lamport_mutex);
}

void pyrkon_reset_stats(){
    pyrkon_escapees_number_reset();
    my_received_ack_reset();
    my_messages_lamport_clocks_reset();
    waiting_for_ack_reset();
    lamport_clock_reset();
}

void pyrkon_release_the_last(){
    if(get_pyrkon_escapees_number()==size){
        pthread_mutex_unlock(&finish_current_pyrkon);
    }
}

void free_my_pyrkon_ticket(){
    for (int i = 0; i<size; i++){
        if(waiting_for_ack[i].ts!=-111){
            packet_t packet = waiting_for_ack[i];
            packet.ts = get_increased_lamport_clock();
            debug("HANDLER-external:: %d in %s sends %s to %d with clock %d\n", rank, state_strings[get_state()], message_strings[WANT_PYRKON_TICKET_ACK], packet.src, packet.ts);
            sendPacket(&packet,packet.src,WANT_PYRKON_TICKET_ACK);
        }
    }
}

void set_my_messages_lamport_clocks(int index, int value){
    pthread_mutex_lock(&my_messages_lamport_clocks_mutex);
        my_messages_lamport_clocks[index]=value;
    pthread_mutex_unlock(&my_messages_lamport_clocks_mutex);
}

void set_my_received_ack(int index, int value){
    pthread_mutex_lock(&my_received_ack_mutex);
        my_received_ack[index]=value;
    pthread_mutex_unlock(&my_received_ack_mutex);
}