#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <locale.h>
#include <math.h>
#include <time.h>
#include <ctype.h>
#include <pthread.h>
#include <signal.h>

#include "linkedlist.c"

sigset_t set;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cv = PTHREAD_COND_INITIALIZER;
pthread_t arrival_thread;
pthread_t token_depositing_thread;
pthread_t s1_thread;
pthread_t s2_thread;
pthread_t sig_catch_thread;

LinkedList Q1;
LinkedList Q2;
//LinkedList prearrival_packet_queue;
LinkedList completed_packet_queue;
double lambda; //packet arrival rate, packets per second
double mu; //server service rate, packets per second
double r; //token arrival rate, tokens per second
int b; //bucket depth, tokens
int p; //token requirement for packet, tokens
int num; //number of packets to transmit
int tsfile_true;
int token_bucket;
int token_bucket_capacity;
int last_packet_sent;
double total_interarrival_time;
double tokens_dropped;
double tokens_produced;
double packets_dropped;
double packets_arrived;
int packet_shutdown;
int token_shutdown;
int s1_shutdown;
int s2_shutdown;
double total_time_spent_q1 = 0;
double total_time_spent_q2 = 0;
double total_time_spent_s1 = 0;
double total_time_spent_s2 = 0;
double total_time_spent_system = 0;
double total_service_time = 0;
int cancel;
FILE *fp;

struct timeval tv1;

typedef struct myPacket {
	double interarrival_time; //msec
	double service_time; //msec
	int token_req;
	int curr_packet_num;
	double time_entered_q1; //msec
	double time_entered_q2; //msec
	double time_entered_server; //msec
	double time_spent_q1; //msec
	double time_spent_q2; //msec
	double time_spent_s1; //msec
	double time_spent_s2; //msec
	double time_spent_system; //msec

} Packet;

static Packet* CreatePacketTraceDrivenMode(int curr_packet_num) {

	char buf[80] = { 0 };
	char *start_ptr = buf;

	Packet *data = NULL;

	//read a line
	if (fgets(buf, sizeof(buf), fp) != 0) {
		start_ptr = buf;

		//create list object
		data = malloc(sizeof(Packet));

		//extract parameters
		start_ptr = buf;

		//packet arrival time (lambda)

		data->interarrival_time = atof(start_ptr);

		while (!isspace(*start_ptr))
			start_ptr++;
		while (isspace(*start_ptr))
			start_ptr++;

		//token requirement for packet (p)
		data->token_req = atoi(start_ptr);

		while (!isspace(*start_ptr))
			start_ptr++;
		while (isspace(*start_ptr))
			start_ptr++;

		//service time (mu)
		data->service_time = atof(start_ptr);

		//packet number
		data->curr_packet_num = curr_packet_num;

		//everything else
		data->time_entered_q1 = 0; //msec
		data->time_entered_q2 = 0; //msec
		data->time_entered_server = 0; //msec
		data->time_spent_q1 = 0; //msec
		data->time_spent_q2 = 0; //msec
		data->time_spent_s1 = 0; //msec
		data->time_spent_s2 = 0; //msec
		data->time_spent_system = 0; //msec

	}
	return data;
}

static Packet* CreatePacketDeterministicMode(int curr_packet_num) {

	Packet *data = malloc(sizeof(Packet));

	data->interarrival_time = 1 / lambda * 1000; //sec->msec
	data->service_time = 1 / mu * 1000; //sec->msec
	data->token_req = p;
	data->curr_packet_num = curr_packet_num;

	//everything else
	data->time_entered_q1 = 0; //msec
	data->time_entered_q2 = 0; //msec
	data->time_entered_server = 0; //msec
	data->time_spent_q1 = 0; //msec
	data->time_spent_q2 = 0; //msec
	data->time_spent_s1 = 0; //msec
	data->time_spent_s2 = 0; //msec
	data->time_spent_system = 0; //msec

	if (1 / lambda > 10) {
		data->interarrival_time = 10000; //10 sec = 10000 msec
	}
	if (1 / mu > 10) {
		data->service_time = 10000;
	}

	return data;

}

void PrintParameters(int tsfile_true, double lambda, double mu, double r, int b,
		int p, int num, char* tsfile) {
	fprintf(stdout, "Emulation Parameters\n");
	fprintf(stdout, "number to arrive = %i\n", num);

	if (!tsfile_true){
		fprintf(stdout, "lambda = %g\n", lambda);
		fprintf(stdout, "mu = %g\n", mu);
	}

	fprintf(stdout, "r = %g\n", r);
	fprintf(stdout, "B = %i\n", b);

	if (tsfile_true) {
		fprintf(stdout, "tsfile = %s\n\n", tsfile);
	} else {
		fprintf(stdout, "P = %i\n\n", p);
	}

}

void *PacketArrival(void *arg) {

	int curr_packet_num = 1;
	Packet *data = NULL;
	LinkedListElem *q1_elem;
	struct timeval tv2;
	double curr_time = 0;

	for (;;) {

		if (curr_packet_num <= num) {

			//create a packet
			if (tsfile_true == 1){ //trace driven mode
				data = CreatePacketTraceDrivenMode(curr_packet_num);

			}else{ //deterministic mode
				data = CreatePacketDeterministicMode(curr_packet_num);
			}


			struct timespec t = { 0 };
			t.tv_sec = (int) (data->interarrival_time / 1000);
			t.tv_nsec = (data->interarrival_time - (t.tv_sec * 1000)) * 1000000;

			gettimeofday(&tv2, NULL);
			curr_time = (tv2.tv_sec - tv1.tv_sec) * 1000.0; // sec to msec
			curr_time += (tv2.tv_usec - tv1.tv_usec) / 1000.0;   // usec to msec
			double start_interarrival_time = curr_time;

			nanosleep(&t, NULL); //use rem for signal handling

			pthread_mutex_lock(&mutex);

			///////////Add Packet to Q1////////////////

			gettimeofday(&tv2, NULL);
			curr_time = (tv2.tv_sec - tv1.tv_sec) * 1000.0;   // sec to msec
			curr_time += (tv2.tv_usec - tv1.tv_usec) / 1000.0;   // usec to msec
			double end_interarrival_time = curr_time;
			double real_interarrival_time = end_interarrival_time
					- start_interarrival_time;
			total_interarrival_time += real_interarrival_time;

			packets_arrived++;

			if (data->token_req > token_bucket_capacity) { //drop packet

				packets_dropped++;
				printf(
						"%012.3fms: p%i arrives, needs %i tokens, inter-arrival time = %0.6gms, dropped \n",
						curr_time, curr_packet_num, data->token_req,
						real_interarrival_time);

				//write code to drop the packet here

			} else { //arrives, add to q1

				printf(
						"%012.3fms: p%i arrives, needs %i tokens, inter-arrival time = %0.6gms \n",
						curr_time, curr_packet_num, data->token_req,
						real_interarrival_time);

				LinkedListAppend(&Q1, (struct Packet*) data);

				gettimeofday(&tv2, NULL);
				curr_time = (tv2.tv_sec - tv1.tv_sec) * 1000.0;   // sec to msec
				curr_time += (tv2.tv_usec - tv1.tv_usec) / 1000.0; // usec to msec

				data->time_entered_q1 = curr_time;

				printf("%012.3fms: p%i enters Q1\n", curr_time,
						curr_packet_num);

			}

			curr_packet_num++;

			pthread_cond_broadcast(&cv);
			pthread_mutex_unlock(&mutex);

		}

		/////////////Move Packet from Q1 to Q2//////////////////
		if (!LinkedListEmpty(&Q1)) {

			q1_elem = LinkedListFirst(&Q1);
			Packet *q1_data = (Packet *) (q1_elem->obj);

			if (token_bucket >= q1_data->token_req) {

				token_bucket -= q1_data->token_req;

				LinkedListUnlink(&Q1, q1_elem);

				gettimeofday(&tv2, NULL);
				curr_time = (tv2.tv_sec - tv1.tv_sec) * 1000.0;   // sec to msec
				curr_time += (tv2.tv_usec - tv1.tv_usec) / 1000.0; // usec to msec

				q1_data->time_spent_q1 = curr_time - q1_data->time_entered_q1;
				q1_data->time_spent_system += q1_data->time_spent_q1;

				printf(
						"%012.3fms: p%i leaves Q1, time in Q1 = %0.6gms. token bucket now has %i tokens \n",
						curr_time, q1_data->curr_packet_num,
						q1_data->time_spent_q1, token_bucket);

				LinkedListAppend(&Q2, (struct Packet*) q1_data);

				gettimeofday(&tv2, NULL);
				curr_time = (tv2.tv_sec - tv1.tv_sec) * 1000.0;   // sec to msec
				curr_time += (tv2.tv_usec - tv1.tv_usec) / 1000.0; // usec to msec

				q1_data->time_entered_q2 = curr_time;

				printf("%012.3fms: p%i enters Q2\n", curr_time,
						q1_data->curr_packet_num);

			}

		}

		if (curr_packet_num > num) {
			last_packet_sent = 1;
		}
		if (curr_packet_num > num && LinkedListEmpty(&Q1)) {
			//printf("Packet Arrival Shutdown\n");

			packet_shutdown = 1;
			if (packet_shutdown == 1 && token_shutdown == 1 && LinkedListEmpty(&Q1)
					&& LinkedListEmpty(&Q2)) {
				cancel = 1;
			}

			pthread_mutex_lock(&mutex);
			pthread_cond_broadcast(&cv);
			pthread_mutex_unlock(&mutex);
			break;
		}
	}

	return (0);
}

void *TokenArrival(void *arg) {

	int curr_token = 0;
	double token_arrival_time = 1 / r; //sec
	if (token_arrival_time > 10) {
		token_arrival_time = 10; //sec
	}

	for (;;) {

		struct timespec t = { 0 };

		t.tv_sec = (int) (token_arrival_time);
		t.tv_nsec = (token_arrival_time - (t.tv_sec)) * 1000000000;
		nanosleep(&t, NULL); //use rem for signal handling

		pthread_mutex_lock(&mutex);

		curr_token++;
		tokens_produced++; //redundant

		struct timeval tv2;
		gettimeofday(&tv2, NULL);
		double curr_time = (tv2.tv_sec - tv1.tv_sec) * 1000.0;   // sec to msec
		curr_time += (tv2.tv_usec - tv1.tv_usec) / 1000.0;   // usec to msec

		if (token_bucket >= token_bucket_capacity) { //if at capacity, drop the token
			printf("%012.3fms: token t%i arrives, dropped\n", curr_time,
					curr_token);
			tokens_dropped++;

		} else {

			token_bucket++;
			printf(
					"%012.3fms: token t%i arrives, token bucket now has %i tokens \n",
					curr_time, curr_token, token_bucket);
		}

		pthread_cond_broadcast(&cv);
		pthread_mutex_unlock(&mutex);

		if (LinkedListEmpty(&Q1) && last_packet_sent == 1) {
			//printf("Token Arrival Shutdown\n");
			token_shutdown = 1;

			if (packet_shutdown == 1 && token_shutdown == 1 && LinkedListEmpty(&Q1)
					&& LinkedListEmpty(&Q2)) {
				cancel = 1;
			}

			pthread_mutex_lock(&mutex);
			pthread_cond_broadcast(&cv);
			pthread_mutex_unlock(&mutex);
			break;
		}

	}

	return (0);
}

void *ServerOne(void *arg) {

	LinkedListElem *elem;
	int curr_packet_num;

	for (;;) {

		pthread_mutex_lock(&mutex);

		while (LinkedListEmpty(&Q2) && s2_shutdown == 0 && cancel == 0) { // && !shutdown
			//printf("S1 wait a\n");
			pthread_cond_wait(&cv, &mutex);
			//printf("S1 wait b\n");

		}

		if (s2_shutdown == 1 || cancel == 1) {
			//printf("Server 1 Shutdown 1\n");
			s1_shutdown = 1;
			pthread_mutex_unlock(&mutex);

			if (s1_shutdown == 1 && s2_shutdown == 1 && token_shutdown == 1 && packet_shutdown
					== 1) {
				pthread_cancel(sig_catch_thread);
			}

			break;
		}

		elem = LinkedListFirst(&Q2);

		LinkedListUnlink(&Q2, elem);

		Packet *data = (Packet *) (elem->obj);

		curr_packet_num = data->curr_packet_num;

		struct timeval tv2;
		gettimeofday(&tv2, NULL);
		double curr_time = (tv2.tv_sec - tv1.tv_sec) * 1000.0;   // sec to msec
		curr_time += (tv2.tv_usec - tv1.tv_usec) / 1000.0;   // usec to msec
		data->time_entered_server = curr_time;

		data->time_spent_q2 = curr_time - data->time_entered_q2;
		data->time_spent_system += data->time_spent_q2;

		printf("%012.3fms: p%i leaves Q2, time in Q2 = %0.6gms. \n", curr_time,
				data->curr_packet_num, data->time_spent_q2);

		gettimeofday(&tv2, NULL);
		curr_time = (tv2.tv_sec - tv1.tv_sec) * 1000.0;   // sec to msec
		curr_time += (tv2.tv_usec - tv1.tv_usec) / 1000.0;   // usec to msec
		data->time_entered_server = curr_time;

		printf(
				"%012.3fms: p%i begins service at S1, requesting  %gms of service \n",
				curr_time, curr_packet_num, data->service_time);

		pthread_mutex_unlock(&mutex);

		struct timespec t = { 0 };
		t.tv_sec = (int) (data->service_time / 1000);
		t.tv_nsec = (data->service_time - (t.tv_sec * 1000)) * 1000000;
		nanosleep(&t, NULL); //use rem for signal handling

		gettimeofday(&tv2, NULL);
		curr_time = (tv2.tv_sec - tv1.tv_sec) * 1000.0;   // sec to msec
		curr_time += (tv2.tv_usec - tv1.tv_usec) / 1000.0;   // usec to msec

		data->time_spent_s1 = curr_time - data->time_entered_server;
		data->time_spent_system += data->time_spent_s1;

		printf(
				"%012.3fms: p%i departs from S1, service time =   %0.6gms, time in system = %0.6gms \n",
				curr_time, curr_packet_num, data->time_spent_s1,
				data->time_spent_system);

		LinkedListAppend(&completed_packet_queue, (struct Packet*) data);

		if (LinkedListEmpty(&Q2) && LinkedListEmpty(&Q1)
				&& last_packet_sent == 1) {

			pthread_mutex_lock(&mutex);
			pthread_cond_broadcast(&cv);
			pthread_mutex_unlock(&mutex);

			//printf("Server 1 Shutdown 2\n");
			s1_shutdown = 1;

			if (s1_shutdown == 1 && s2_shutdown == 1 && token_shutdown == 1 && packet_shutdown
					== 1) {
				pthread_cancel(sig_catch_thread);
			}

			break;
		}

	}
	return (0);
}

void *ServerTwo(void *arg) {

	LinkedListElem *elem;
	int curr_packet_num;

	for (;;) {

		pthread_mutex_lock(&mutex);

		while (LinkedListEmpty(&Q2) && s1_shutdown == 0 && cancel == 0) { // && !shutdown
			//printf("S2 wait a\n");
			pthread_cond_wait(&cv, &mutex);
			//printf("S2 wait b\n");
		}

		if (s1_shutdown == 1 || cancel == 1) {
			//printf("Server 2 Shutdown 1\n");
			s2_shutdown = 1;
			pthread_mutex_unlock(&mutex);

			if (s1_shutdown == 1 && s2_shutdown == 1 && token_shutdown == 1 && packet_shutdown
					== 1) {
				pthread_cancel(sig_catch_thread);
			}

			break;
		}

		elem = LinkedListFirst(&Q2);

		LinkedListUnlink(&Q2, elem);

		Packet *data = (Packet *) (elem->obj);

		curr_packet_num = data->curr_packet_num;

		struct timeval tv2;
		gettimeofday(&tv2, NULL);
		double curr_time = (tv2.tv_sec - tv1.tv_sec) * 1000.0;   // sec to msec
		curr_time += (tv2.tv_usec - tv1.tv_usec) / 1000.0;   // usec to msec
		data->time_entered_server = curr_time;

		data->time_spent_q2 = curr_time - data->time_entered_q2;
		data->time_spent_system += data->time_spent_q2;

		printf("%012.3fms: p%i leaves Q2, time in Q2 = %0.6gms. \n", curr_time,
				data->curr_packet_num, data->time_spent_q2);

		gettimeofday(&tv2, NULL);
		curr_time = (tv2.tv_sec - tv1.tv_sec) * 1000.0;   // sec to msec
		curr_time += (tv2.tv_usec - tv1.tv_usec) / 1000.0;   // usec to msec
		data->time_entered_server = curr_time;

		printf(
				"%012.3fms: p%i arrives, begins service at S2, requesting  %gms of service \n",
				curr_time, curr_packet_num, data->service_time);

		pthread_mutex_unlock(&mutex);

		struct timespec t = { 0 };
		t.tv_sec = (int) (data->service_time / 1000);
		t.tv_nsec = (data->service_time - (t.tv_sec * 1000)) * 1000000;
		nanosleep(&t, NULL); //use rem for signal handling

		gettimeofday(&tv2, NULL);
		curr_time = (tv2.tv_sec - tv1.tv_sec) * 1000.0;   // sec to msec
		curr_time += (tv2.tv_usec - tv1.tv_usec) / 1000.0;   // usec to msec

		data->time_spent_s2 = curr_time - data->time_entered_server;

		data->time_spent_system += data->time_spent_s2;

		printf(
				"%012.3fms: p%i departs from S2, service time =   %0.6gms, time in system = %0.6gms \n",
				curr_time, curr_packet_num, data->time_spent_s2,
				data->time_spent_system);

		LinkedListAppend(&completed_packet_queue, (struct Packet*) data);


		if (LinkedListEmpty(&Q2) && LinkedListEmpty(&Q1)
				&& last_packet_sent == 1) {

			pthread_mutex_lock(&mutex);
			pthread_cond_broadcast(&cv);
			pthread_mutex_unlock(&mutex);

			//printf("Server 2 Shutdown 2\n");
			s2_shutdown = 1;

			if (s1_shutdown == 1 && s2_shutdown == 1 && token_shutdown == 1 && packet_shutdown
					== 1) {
				pthread_cancel(sig_catch_thread);
			}

			break;
		}

	}
	return (0);
}

void *monitor() {
	int sig;
	while (1) {
		//printf("x  \n\n");
		sigwait(&set, &sig);
		//printf("y  \n\n");

		pthread_mutex_lock(&mutex);

		struct timeval tv2;
		gettimeofday(&tv2, NULL);
		double curr_time = (tv2.tv_sec - tv1.tv_sec) * 1000.0;   // sec to msec
		curr_time += (tv2.tv_usec - tv1.tv_usec) / 1000.0;   // usec to msec

		printf(
				"%012.3fms: SIGINT caught, no new packets or tokens will be allowed \n",
				curr_time);

		//printf("a \n\n");
		pthread_cancel(arrival_thread);
		pthread_cancel(token_depositing_thread);

		LinkedListElem *elem = NULL;

		for (elem = LinkedListFirst(&Q1); elem != NULL;
				elem = LinkedListNext(&Q1, elem)) {

			Packet *data = (Packet *) (elem->obj);

			printf("%012.3fms: p%i removed from Q1 \n", curr_time,
					data->curr_packet_num);
		}

		for (elem = LinkedListFirst(&Q2); elem != NULL;
				elem = LinkedListNext(&Q2, elem)) {

			Packet *data = (Packet *) (elem->obj);

			printf("%012.3fms: p%i removed from Q2 \n", curr_time,
					data->curr_packet_num);
		}

		LinkedListUnlinkAll(&Q1);
		LinkedListUnlinkAll(&Q2);
		cancel = 1;
		last_packet_sent = 1;
		//printf("b  \n\n");

		pthread_cond_broadcast(&cv);
		pthread_mutex_unlock(&mutex);
		break;

	}
	//printf("c1  \n\n");
	return (0);

}

int main(int argc, char *argv[]) {

	//Initialize Variables
	char tsfile[1024] = { 0 };
	lambda = 1; //packet arrival rate, packets per second
	mu = 0.35; //server service rate, packets per second
	r = 1.5; //token arrival rate, tokens per second
	b = 10; //bucket depth, tokens
	p = 3; //token requirement for packet, tokens
	num = 20; //number of packets to transmit
	tsfile_true = 0;
	token_bucket_capacity = b;
	total_interarrival_time = 0;
	last_packet_sent = 0;
	tokens_dropped = 0;
	tokens_produced = 0;
	packets_dropped = 0;
	packets_arrived = 0;
	packet_shutdown = 0;
	token_shutdown = 0;
	s1_shutdown = 0;
	s2_shutdown = 0;
	cancel = 0;
	memset(&Q1, 0, sizeof(LinkedList));
	memset(&Q2, 0, sizeof(LinkedList));
	memset(&completed_packet_queue, 0, sizeof(LinkedList));


	//Command Handling
	if (argc % 2 == 0) { //if arg length is wrong
		fprintf(stderr, "ERROR: malinformed command. Missing inputs. \n");
		exit(1);
	}

	//Command line handling
	int i = 1;
	for (i = 1; i < argc; i = i + 2) {

		if (argc > 1) { //if its not just ./warmup2

			//if the flag is wrong
			if (strcmp(argv[i], "-lambda") != 0 && strcmp(argv[i], "-mu") != 0
					&& strcmp(argv[i], "-r") != 0 && strcmp(argv[i], "-B") != 0
					&& strcmp(argv[i], "-P") != 0 && strcmp(argv[i], "-n") != 0
					&& strcmp(argv[i], "-t") != 0) {
				fprintf(stderr,
						"ERROR: malinformed command.  Flag is wrong.\n");
				exit(1);
			}

			//if the parameter is not a number for all but y
			if (strcmp(argv[i], "-lambda") == 0 || strcmp(argv[i], "-mu") == 0
					|| strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "-B") == 0
					|| strcmp(argv[i], "-P") == 0 || strcmp(argv[i], "-n") == 0){
				char* end;
				if (strtod(argv[i + 1], &end) == 0.0) {
					fprintf(stderr,
							"ERROR: malinformed command. Parameter input is wrong. \n");
					exit(1);
				}
			}
		}
	}

	//Get Parameters
	for (i = 1; i < argc; i = i + 2) { //start check after warmup2 cmd input

		if (strcmp(argv[i], "-t") == 0) { //if -t, get parameters from tsfile and break
			tsfile_true = 1;
			strncpy(tsfile, argv[i + 1], 1024);
			fp = fopen(argv[i + 1], "r");
			if (fp == NULL) {
				fprintf(stderr,
						"ERROR: File %s cannot be opened or file does not exist. \n", argv[i+1]);
				exit(1);
			}
		} else if (strcmp(argv[i], "-lambda") == 0) {
			lambda = atof(argv[i + 1]);
		} else if (strcmp(argv[i], "-mu") == 0) {
			mu = atof(argv[i + 1]);
		} else if (strcmp(argv[i], "-r") == 0) {
			r = atof(argv[i + 1]);
		} else if (strcmp(argv[i], "-B") == 0) {
			b = atoi(argv[i + 1]);
			token_bucket_capacity = b;
		} else if (strcmp(argv[i], "-P") == 0) {
			p = atoi(argv[i + 1]);
		} else if (strcmp(argv[i], "-n") == 0) {
			num = atoi(argv[i + 1]);
		}
	}

	//Print Emulation Parameters
	PrintParameters(tsfile_true, lambda, mu, r, b, p, num, tsfile);
	if (!LinkedListInit(&completed_packet_queue)) {
		fprintf(stderr, "ERROR: Can't initialize completed_packet_queue. \n");
		exit(1);
	}
	if (!LinkedListInit(&Q1)) {
		fprintf(stderr, "ERROR: Can't initialize Q1. \n");
		exit(1);
	}
	if (!LinkedListInit(&Q2)) {
		fprintf(stderr, "ERROR: Can't initialize Q2. \n");
		exit(1);
	}

	//Load Packet Queue
	if (tsfile_true) { //trace-driven mode

		char buf[80] = { 0 };
		char *start_ptr = buf;

		//get num
		if (fgets(buf, sizeof(buf), fp) == 0) {
			fprintf(stderr, "ERROR: malinformed input. no first line. \n");
			exit(1);
		}

		char* end;
		if (strtol(start_ptr, &end, 0) == 0) {
			fprintf(stderr,
					"ERROR: malformed input - line 1 is not a number. \n");
			exit(1);
		}
		if (end != '\0') {
			fprintf(stderr,
					"ERROR: malformed input - line 1 is not a number. \n");
			exit(1);
		}

		num = atoi(start_ptr);

	}

	//start emulation code
	gettimeofday(&tv1, NULL);
	double start_time = (tv1.tv_sec - tv1.tv_sec) * 1000.0;      // sec to msec
	start_time += (tv1.tv_usec - tv1.tv_usec) / 1000.0;   // usec to msec
	fprintf(stdout, "%012.3fms: emulation begins\n", start_time);

	sigemptyset(&set);
	sigaddset(&set, SIGINT);
	sigprocmask(SIG_BLOCK, &set, 0); //main thread blocks SIGINT

	pthread_create(&arrival_thread, 0, PacketArrival, 0);
	pthread_create(&token_depositing_thread, 0, TokenArrival, 0);
	pthread_create(&s1_thread, 0, ServerOne, 0);
	pthread_create(&s2_thread, 0, ServerTwo, 0);
	pthread_create(&sig_catch_thread, 0, monitor, 0);

	pthread_join(arrival_thread, 0);
	pthread_join(token_depositing_thread, 0);
	pthread_join(s1_thread, 0);
	pthread_join(s2_thread, 0);
	pthread_join(sig_catch_thread, 0);

	struct timeval tv2;
	gettimeofday(&tv2, NULL);
	double curr_time = (tv2.tv_sec - tv1.tv_sec) * 1000.0;      // sec to msec
	curr_time += (tv2.tv_usec - tv1.tv_usec) / 1000.0;   // usec to msec
	double total_emulation_time = curr_time;
	fprintf(stdout, "%012.3fms: emulation ends\n\n", curr_time);

	//statistics (all in ms)

	LinkedListElem *elem = NULL;

	for (elem = LinkedListFirst(&completed_packet_queue); elem != NULL; elem =
			LinkedListNext(&completed_packet_queue, elem)) {

		Packet *data = (Packet *) (elem->obj);

		total_time_spent_q1 += data->time_spent_q1;
		total_time_spent_q2 += data->time_spent_q2;
		total_time_spent_s1 += data->time_spent_s1; //could be 0, depending on which server it was served in
		total_time_spent_s2 += data->time_spent_s2; //could be 0, depending on which server it was served in
		total_time_spent_system += data->time_spent_system;
		total_service_time += data->time_spent_s1 + data->time_spent_s2;
	}


	//convert all msec to sec
	double avg_interarrival_time = (total_interarrival_time / num) / 1000; //all packets
	double avg_service_time = (total_service_time / num) / 1000; //completed packets only
	double avg_num_packets_q1 = (total_time_spent_q1 / total_emulation_time); //completed packets only
	double avg_num_packets_q2 = (total_time_spent_q2 / total_emulation_time); //completed packets only
	double avg_num_packets_s1 = (total_time_spent_s1 / total_emulation_time); //completed packets only
	double avg_num_packets_s2 = (total_time_spent_s2 / total_emulation_time); //completed packets only

	double avg_time_spent_system_msec = (total_time_spent_system / num); //completed packets only
	double avg_time_spent_system = (total_time_spent_system / num) / 1000; //completed packets only

	double summation = 0;
	for (elem = LinkedListFirst(&completed_packet_queue); elem != NULL; elem =
			LinkedListNext(&completed_packet_queue, elem)) {

		Packet *data = (Packet *) (elem->obj);

		summation = summation
				+ pow((data->time_spent_system - avg_time_spent_system_msec),
						2);
	}
	double variance_time_spent_system = summation / num;
	double std_dev_time_spent_system = sqrt(variance_time_spent_system) / 1000; //completed packets only

	double token_drop_probability = tokens_dropped / tokens_produced;
	double packet_drop_probability = packets_dropped / packets_arrived; // all packets

	fprintf(stdout, "average packet inter-arrival time = <%0.6g>\n",
			avg_interarrival_time);
	fprintf(stdout, "average packet service time = <%0.6g>\n\n",
			avg_service_time);

	fprintf(stdout, "average number of packets in Q1 = <%0.6g>\n",
			avg_num_packets_q1);
	fprintf(stdout, "average number of packets in Q2 = <%0.6g>\n",
			avg_num_packets_q2);
	fprintf(stdout, "average number of packets at S1 = <%0.6g>\n",
			avg_num_packets_s1);
	fprintf(stdout, "average number of packets at S2 = <%0.6g>\n\n",
			avg_num_packets_s2);

	fprintf(stdout, "average time a packet spent in system = <%0.6g>\n",
			avg_time_spent_system);
	fprintf(stdout, "standard deviation for time spent in system = <%0.6g>\n\n",
			std_dev_time_spent_system);

	fprintf(stdout, "token drop probability = <%0.6g>\n",
			token_drop_probability);
	fprintf(stdout, "packet drop probability = <%0.6g>\n",
			packet_drop_probability);

	return (0);

}
