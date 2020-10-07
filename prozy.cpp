#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mpi.h>
#include <pthread.h>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define MAX(x, y) (((x) > (y)) ? (x) : (y))

#define PYRKON_REQ 100
#define PYRKON_ACK 101

#define WORKSHOP_REQ 200
#define WORKSHOP_ACK 201

#define PYRKON_END 300

#define WORKSHOPS_NUM 5
#define TICKETS_NUM 2
#define WORKSHOP_SIZE 1

using namespace std;

enum Etap { 
    INIT_PYRKON, 
    WAIT_PYRKON, 
    PYRKON, 
    WAIT_WORKSHOP, 
    WORKSHOP, 
    FINISH_WORKSHOP, 
    FINISH_PYRKON
};

class Zasoby{
public:
    Zasoby(){
        int rank, size;
        MPI_Comm_rank(MPI_COMM_WORLD, &rank);
        MPI_Comm_size(MPI_COMM_WORLD, &size);
        srand(time(NULL)+rank);
        this->rank = rank;
        this->size = size;
        this->lclock = 0;
        this->pyrkon_num = 1;
        this->memlock = new pthread_mutex_t;
        pthread_mutex_init(this->memlock, NULL);
        this->condlock = new pthread_cond_t;
        pthread_cond_init(this->condlock, NULL);
        this->reset();
    }
    void reset(){
      this->pyrclock = 0;
      this->pyrkon_end_counter = 0;
      this->curr_workshop = -1;
      this->pyrkon_accepts = 0;
      this->etap = WAIT_PYRKON;
      memset(workclock, 0, sizeof(workclock));
      memset(pyrkon_accepts_left, 0, sizeof(pyrkon_accepts_left));
      memset(workshop_accepts, 0, sizeof(workshop_accepts));
      memset(workshop_accepts_left, 0, sizeof(workshop_accepts_left));

      // losowanie pożądanych warsztatów.
      bool atLeastOne = false;
      for (int i = 0; i < WORKSHOPS_NUM; i++){
        workshops_wanted[i] = rand() % 2;
        if(workshops_wanted[i])
            atLeastOne = true;
      }
      if(!atLeastOne)
          workshops_wanted[rand()%WORKSHOPS_NUM] = 1;

    }
    void printWorkshopsLeft(){
        cout<<rank<<": Pozostałe warsztaty: [";
        for(int i = 0; i<WORKSHOPS_NUM-1; i++)
            cout<<workshops_wanted[i]<<",";
        cout<<workshops_wanted[WORKSHOPS_NUM-1]<<"]"<<endl;
    }

    int rank;
    int pyrkon_num;
    int lclock;
    int size;
    int curr_workshop;
    int pyrclock;
    int workclock[WORKSHOPS_NUM];
    int pyrkon_end_counter;
    Etap etap;
    pthread_mutex_t* memlock;
    pthread_cond_t* condlock;
    int workshops_wanted[WORKSHOPS_NUM];
    int pyrkon_accepts;
    bool pyrkon_accepts_left[100];
    int workshop_accepts[WORKSHOPS_NUM];
    bool workshop_accepts_left[100][WORKSHOPS_NUM];
};

void sendToEveryone(Zasoby *zasoby, int tag, int workshop_num){
    zasoby->lclock++;
    if(tag == PYRKON_REQ)
        zasoby->pyrclock = zasoby->lclock;
    else if (tag == WORKSHOP_REQ)
        zasoby->workclock[workshop_num] = zasoby->lclock;
    int message[] = {zasoby->lclock, workshop_num};
    for (int i=0; i < zasoby->size; i++) {
        if(i != zasoby->rank){
            MPI_Send(message, 2, MPI_INT, i, tag, MPI_COMM_WORLD);
        }
    }
}

bool priority(int lclock, int rank, int lclock2, int rank2) {
  if (lclock == 0)
    return false;
  if (lclock2 > lclock || (lclock2 == lclock && rank2 > rank))
    return true;
  return false;
}

void initPyrkon(Zasoby *zasoby){
    if(zasoby->rank == 0)
        cout<<"-----------KONIEC PYRKONU-----------"<<endl;
    sleep(5);
    zasoby->reset();
    zasoby->pyrkon_num++;
    if(zasoby->rank == 0)
        cout<<"-----------ZACZYNAMY PYRKON NUMER "<<zasoby->pyrkon_num<<"-----------"<<endl;
}

void getIntoPyrkon(Zasoby *zasoby){
    zasoby->printWorkshopsLeft();
    int duration = (rand() % 4)+1;
    cout<<zasoby->rank<<": czeka przez "<<duration<<" sekund"<<endl;
    sleep(duration);

    sendToEveryone(zasoby, PYRKON_REQ, 0);
    pthread_mutex_lock(zasoby->memlock);
        //sprawdz czy mozesz wejsc, jak nie to czekaj
        if(zasoby->pyrkon_accepts < zasoby->size - TICKETS_NUM){
            pthread_cond_wait(zasoby->condlock, zasoby->memlock);
        }
        zasoby->etap = PYRKON;
        zasoby->lclock++;
        cout<<zasoby->rank<<": wszedł na Pyrkon."<<endl;
        zasoby->etap = WAIT_WORKSHOP;
    pthread_mutex_unlock(zasoby->memlock);
}

void getIntoWorkshop(Zasoby *zasoby){
    for(int i = 0; i<WORKSHOPS_NUM; i++){
        if(zasoby->workshops_wanted[i])
            sendToEveryone(zasoby, WORKSHOP_REQ, i);
    }

    //wieczna pętla po workshopach
    for(int i = 0; ; i = (i+1) % WORKSHOPS_NUM){
        if(zasoby->workshops_wanted[i])
            if(zasoby->workshop_accepts[i] >= zasoby->size - WORKSHOP_SIZE){
                pthread_mutex_lock(zasoby->memlock);
                zasoby->curr_workshop = i;
                zasoby->lclock++;
                zasoby->etap = WORKSHOP;
                /* cout<<zasoby->rank<<" wszedl na warsztat "<<i<<endl; */
                pthread_mutex_unlock(zasoby->memlock);
                break;
            }

    }
}

void workshop(Zasoby *zasoby){
    pthread_mutex_lock(zasoby->memlock);
    //zerujemy otrzymane pozwolenia
    for(int i = 0; i<WORKSHOPS_NUM; i++)
        zasoby->workshop_accepts[i] = 0;
    //zwalniamy innym resztę workshopów
    for (int i = 0; i < WORKSHOPS_NUM; i++) {
        if(i == zasoby->curr_workshop) continue;
        if(zasoby->workshops_wanted[i]){
            for(int j = 0; j<zasoby->size; j++){
                if(j == zasoby->rank) continue;
                if (zasoby->workshop_accepts_left[j][i]) {
                    int message[] = {zasoby->lclock, i};
                    MPI_Send(message, 2, MPI_INT, j, WORKSHOP_ACK,
                            MPI_COMM_WORLD);
                    zasoby->workshop_accepts_left[j][i] = 0;
                }
            }
        }
    }
    
    //wykonanie workshopa
    cout<<zasoby->rank<<": wszedł na warsztat "<<zasoby->curr_workshop<<endl;
    int duration = (rand()%5)+3;
    //FIXME: usun
    duration += 10;
    sleep(duration);
    cout<<zasoby->rank<<": zakończył warsztat "<<zasoby->curr_workshop<<endl;

    zasoby->workshops_wanted[zasoby->curr_workshop] = 0;
    bool wantedWorkshopLeft = false;
    for(int i = 0; i<WORKSHOPS_NUM; i++)
        if(zasoby->workshops_wanted[i])
            wantedWorkshopLeft = true;
    if(wantedWorkshopLeft)
        zasoby->etap = WAIT_WORKSHOP;
    else
        zasoby->etap = FINISH_PYRKON;
    pthread_mutex_unlock(zasoby->memlock);
    for (int i = 0; i < zasoby->size; i++) {
        if(zasoby->workshop_accepts_left[i][zasoby->curr_workshop]){
            int message[] = {zasoby->lclock, zasoby->curr_workshop};
            MPI_Send(message, 2, MPI_INT, i, WORKSHOP_ACK, MPI_COMM_WORLD);
        }
    }
   zasoby->printWorkshopsLeft(); 
}

void exitPyrkon(Zasoby *zasoby){
    cout<<zasoby->rank<<": wyszedł z Pyrkonu."<<endl;
    for(int i = 0; i<zasoby->size; i++){
        if(zasoby->rank == i) continue;
        if (zasoby->pyrkon_accepts_left[i]) {
            int message[] = {zasoby->lclock, 0};
          /* cout<<zasoby->rank<<" wysyla poznego akcepta do "<<i<<endl; */
          MPI_Send(message, 2, MPI_INT, i, PYRKON_ACK, MPI_COMM_WORLD);
        }
    }
    sendToEveryone(zasoby, PYRKON_END, 0);
    //czekam na to aż wszyscy skończą
    pthread_mutex_lock(zasoby->memlock);
    if(!(zasoby->pyrkon_end_counter == zasoby->size-1))
        pthread_cond_wait(zasoby->condlock, zasoby->memlock);
    zasoby->etap = INIT_PYRKON;
    pthread_mutex_unlock(zasoby->memlock);
}

void* start_responding(void* void_zasoby){
    Zasoby *zasoby = (Zasoby*) void_zasoby;
    int message[2];
    int clock_rec;
    int workshop_rec;
    int tag_rec;
    int source_rec;
    MPI_Status status_rec;
    for(;;){
        MPI_Recv(message, 2, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status_rec);
        clock_rec = message[0];
        workshop_rec = message[1];
        tag_rec = status_rec.MPI_TAG;
        source_rec = status_rec.MPI_SOURCE;
        pthread_mutex_lock(zasoby->memlock);
         
        if(tag_rec == PYRKON_REQ){
            /* cout<<zasoby->rank<<" dostał req od "<<source_rec<<endl; */
            zasoby->pyrkon_accepts_left[source_rec] = 1;
            if(!priority(zasoby->pyrclock, zasoby->rank, clock_rec, source_rec)){
                /* cout<<zasoby->rank<<" wysyla akcepta do "<<source_rec<<endl; */
                int message_reply[] = {zasoby->lclock, 0};
                MPI_Send(
                        message_reply, 
                        2, 
                        MPI_INT, 
                        source_rec, 
                        PYRKON_ACK, 
                        MPI_COMM_WORLD
                );
                zasoby->pyrkon_accepts_left[source_rec] = 0;
            }
        }
        else if (tag_rec == PYRKON_ACK && zasoby->etap == WAIT_PYRKON){
            /* cout<<zasoby->rank<<" dostał akcepta od "<<source_rec<<endl; */
            zasoby->pyrkon_accepts++;
            if(zasoby->pyrkon_accepts >= zasoby->size - TICKETS_NUM){
                pthread_cond_signal(zasoby->condlock);
            }
        }
        else if(tag_rec == WORKSHOP_REQ){
            zasoby->workshop_accepts_left[source_rec][workshop_rec] = true;
            if(zasoby->etap != WAIT_WORKSHOP || !priority(zasoby->workclock[workshop_rec],
                        zasoby->rank,
                        clock_rec,
                        source_rec)){
                /* cout<<zasoby->rank<<" wysyla ACK do "<<source_rec<<" na ws "<<workshop_rec<<endl; */
                int message_reply[] = {zasoby->lclock, 0};
                message_reply[0] = zasoby->lclock;
                message_reply[1] = workshop_rec;
                MPI_Send(
                        message_reply, 
                        2, 
                        MPI_INT, 
                        source_rec, 
                        WORKSHOP_ACK, 
                        MPI_COMM_WORLD
                );
                zasoby->workshop_accepts_left[source_rec][workshop_rec] = false;
            }
        }
        else if(tag_rec == WORKSHOP_ACK && zasoby->etap == WAIT_WORKSHOP){
            /* cout<<zasoby->rank<<" dostałem ack na "<<workshop_rec<<endl; */
            /* pthread_mutex_lock(zasoby->memlock); */
            zasoby->workshop_accepts[workshop_rec]++;
            /* pthread_mutex_unlock(zasoby->memlock); */
        }
        else if(tag_rec == PYRKON_END){
            zasoby->pyrkon_end_counter++;
            if(zasoby->etap == FINISH_PYRKON && 
                    zasoby->pyrkon_end_counter == zasoby->size-1){
                /* cout<<zasoby->pyrkon_end_counter<<zasoby->size<<endl; */
                pthread_cond_signal(zasoby->condlock);
            }
            else if (zasoby->pyrkon_end_counter > zasoby->size-1)
                cout<<"JEST WIĘKSZE!!!!!!!!!!!!!"<<endl;
        }

        zasoby->lclock = max(zasoby->lclock, clock_rec) + 1;
        pthread_mutex_unlock(zasoby->memlock);
    }

    //TODO: zrob jakies wyjscie z petli
    return zasoby;
}
int main(int argc, char** argv){
    int threads_provided;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_SERIALIZED, &threads_provided);

    Zasoby* zasoby = new Zasoby();
    /* if(zasoby->rank == 0) */
    /*     cout<<"DOSTARCZONY POZIOM WSPARCIA WĄTKÓW: "<<threads_provided<<endl; */

    //zaczynamy proces odpowiadający
    pthread_t respond_thread;
    pthread_create(&respond_thread, NULL, &start_responding, (void*) zasoby);

    if(zasoby->rank == 0)
        cout<<"DOSTARCZONY POZIOM WSPARCIA WĄTKÓW: "<<threads_provided<<endl<<
        "-----------ZACZYNAMY PYRKON NUMER 1-----------"<<endl;

    //pętla główna programu
    MPI_Barrier(MPI_COMM_WORLD);
    for(;;){
        if(zasoby->etap == INIT_PYRKON){
            initPyrkon(zasoby);
        }
        if(zasoby->etap == WAIT_PYRKON) {
            getIntoPyrkon(zasoby);
        }
        else if (zasoby->etap == WAIT_WORKSHOP){
            getIntoWorkshop(zasoby);
        }
        else if (zasoby->etap == WORKSHOP){
            workshop(zasoby);
        }
        else if (zasoby->etap == FINISH_PYRKON){
            exitPyrkon(zasoby);
        }
    }
    //TODO: Ustalanie nowego pyrkonu
    MPI_Finalize();
    return 0;
}
