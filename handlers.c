#include "main.h"

//funkcje - reakcje na otrzymanie konkretnych typow komunikatow

void want_pyrkon_ticket_handler(packet_t *packet){
	//debug("HANDLER:: %d asks %d about entering Pyrkon\n",packet->src, packet->dst);
	int my_request_clock = get_my_messages_lamport_clocks(0);

	// my_request_clock = -1 oznacza ignorowanie requestow
	//my_request_clock = 0 oznacza ze nie ubiegam sie
	//jesli size==pyrkon_tickets to ostatni nie dostanie zadnej zgody i itak bedzie czekal wiec uwzgledniam go tu
	if ((size<=PYRKON_TICKETS) || ((my_request_clock!=-1) && (my_request_clock==0 || my_request_clock > packet->ts || (my_request_clock == packet->ts && rank > packet->src) ))){
		packet->ts = get_increased_lamport_clock();
		sendPacket(packet,packet->src,WANT_PYRKON_TICKET_ACK);
	} else { //obsluzymy ich jak juz zwolnimy miejsce na pyrkonie
		waiting_for_ack[packet->src].ts = packet->ts;
		waiting_for_ack[packet->src].src = packet->src;
		waiting_for_ack[packet->src].data = packet->data;
		waiting_for_ack[packet->src].dst = packet->dst;
		waiting_for_ack[packet->src].pyrkon_number = packet->pyrkon_number;
	}
}

void want_pyrkon_ticket_ack_handler(packet_t *packet){
	if(get_state()==init_pyrkon){
		//debug("HANDLER:: %d in state %s got ack to enter Pyrkon %d from %d\n", rank, state_strings[get_state()], packet->pyrkon_number, packet->src);
		my_received_ack_increase(0);
		int my_ack_count = get_my_received_ack(0);
		if (my_ack_count >= size - PYRKON_TICKETS){
			println("HANDLER:: %d has %d/%d permissions\n",rank, my_ack_count, size)
			set_my_messages_lamport_clocks(0,-1); //na pyrkonie ignorujemy requesty wejscia na pyrkon
			pthread_mutex_unlock(&wait_to_enter_pyrkon);
		}
	}
}

void someone_left_pyrkon_handler(packet_t *packet){
	//debug("HANDLER:: %d tells %d about leaving Pyrkon\n",packet->src, packet->dst);
	pyrkon_escapees_number_increase();
	if(get_pyrkon_escapees_number()==size){
		pthread_mutex_unlock(&finish_current_pyrkon);
	}
}

