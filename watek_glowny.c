#include "main.h"
#include "watek_glowny.h"


void mainLoop(){
    srandom(rank);
    waiting_for_ack_reset();
    
    while (!end) {

        switch(stan){

            case init_pyrkon:{
                init_pyrkon_behavior();
                break;
            }
            case on_pyrkon:{
                on_pyrkon_tmp_behavior();
                break;
            }
            case finish_pyrkon:{
                 finish_pyrkon_behavior();
                break;
            }
            default: break;
        }
        
        }
}

void init_pyrkon_behavior(){
    println("STAN:: %s pyrkon_number: %d\n", state_strings[stan], get_pyrkon_number());
    sleep(SEC_IN_STATE);

    pthread_mutex_lock(&wait_to_enter_pyrkon);
    send_message(WANT_PYRKON_TICKET, stan);

    //oczekiwanie na odblokowanie po otrzymaniu odpowiedniej liczby ack
    pthread_mutex_lock(&wait_to_enter_pyrkon);
    pthread_mutex_unlock(&wait_to_enter_pyrkon);

    changeState(on_pyrkon);
}

void on_pyrkon_tmp_behavior(){
    sleep(SEC_IN_STATE);

    int num = (rand() % (100 - 0 + 1));
    println("Having fun on pyrkon!!\n");
    if (num%8==0) {
        println("STAN:: %s pyrkon_number: %d\n", state_strings[stan], get_pyrkon_number());
        changeState(finish_pyrkon);
    }
}

void finish_pyrkon_behavior(){
    println("STAN:: %s pyrkon_number: %d\n",state_strings[stan], get_pyrkon_number());
    sleep(SEC_IN_STATE);

    pthread_mutex_lock(&finish_current_pyrkon);

    pyrkon_escapees_number_increase();
    free_my_pyrkon_ticket();
    //wyslanie komunikatu o wyjsciu z pyrkonu
    send_message(LEFT_PYRKON, stan);

    pyrkon_release_the_last(); //jesli proces jest ostatnim wychodzacym, to sam musi sie wypuscic
    //funkcja blokujaca by poczekac az odpowiednia ilosc wyjsc pusci nas dalej
    pthread_mutex_lock(&finish_current_pyrkon);
    pthread_mutex_unlock(&finish_current_pyrkon);
    //funkcja do posprzatania statystyk z tego pyrkonu
    
    pyrkon_number_increase();
    pyrkon_reset_stats();

    sleep(SEC_IN_STATE);
    changeState(init_pyrkon);


}