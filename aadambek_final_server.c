/* Aishabibi Adambek - Final Project
	- Fancy Quiz Server
	- Last Modified: April 25, 9:46 pm
*/

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <pthread.h>

#define	QLEN			5
#define	BUFSIZE			4096
#define QNUM			128

// Structure for a quiz question
typedef struct qst_t{
	char q[2048];
	char ans[512];
	char winner[512];
	int first;
	int answers;
} qst_t;

// Structure for a client
typedef struct client_t{
	char name[512];
	int score;
	int ssock;
	int c_num;
	int user_gr;
	int hasParsed;
	bool hasJoined;
	bool isAdmin;
} client_t;

// Structure for a group
typedef struct qroup_t{
	char groupname[512];
	int size;
	int current_size;
	int g_num;
	int counter;
	int q_count;
	char quiz_topic[512];
	qst_t questions[128];
	client_t clients[1010];
	client_t *admin;
	pthread_mutex_t mutex;
} group_t;

// Structure for a quiz parser
typedef struct quiz_p_t{
	char groupname[512];
	char topic[512];
	int user_n; 
	int groupsize;
} quiz_p_t;

client_t 		users[1010];
group_t 		groups[1010];
pthread_t 		*group_threads;
int 			groups_num = 0, groups_count = 0, users_count = 0;
fd_set			global_rfds;
fd_set			global_afds; 
int 			global_nfds = 0;

pthread_mutex_t afds_mutex, group_m, group_unique_m, user_m, groupname_m;

int passivesock(char *service, char *protocol, int qlen, int *rport);

void client_leave(int ssock, client_t *user_to_leave) {
		if(user_to_leave != NULL) {
			int y = user_to_leave->c_num;
			pthread_mutex_lock(&user_m);
			(void) close(user_to_leave->ssock); 
			free(&users[y]);
			pthread_mutex_unlock(&user_m);
		} else {
			close(ssock);
		}
		printf("LOG S: Client is leaving\n"); fflush(stdout);
		return;
}

void end_group(group_t *group_to_end) {
	int y = group_to_end->g_num;
	pthread_mutex_t grmutex = group_to_end->mutex;

	pthread_mutex_lock(&grmutex);
	for(int i = 0; i < group_to_end->counter; i++) {
		int s = group_to_end->clients[i].ssock;
		char end[1024];
		strcpy(end, "END|");
		strcat(end, group_to_end->groupname);
		strcat(end, "\r\n");
		if(write(s, end, strlen(end)) < 0) {
			client_leave(s, &group_to_end->clients[i]);
		}
	}
	pthread_mutex_unlock(&grmutex);

	pthread_mutex_lock(&group_m);
	free(&groups[y]);
	groups_num--;
	pthread_mutex_unlock(&group_m);
	free(group_to_end);

	pthread_mutex_destroy(&(group_to_end->mutex));

	return;
}


char *open_groups(char start[4096]) {
	char x[5], y[5];

	pthread_mutex_lock(&group_m);

	for(int i = 0; i < groups_num; i++) {
		strcat(start, "|");
		strcat(start, groups[i].quiz_topic);
		strcat(start, "|");
		strcat(start, groups[i].groupname);
		
		sprintf(x, "|%d", groups[i].size);
		sprintf(y, "|%d", groups[i].current_size);

		strcat(start, x);
		strcat(start, y);
	}

	pthread_mutex_unlock(&group_m);

	printf("LOG S: %s\n", start); fflush(stdout);

	strcat(start, "\r\n");

	return start;
}

int check_groupname(char *name) {
	for(int i = 0; i < groups_count; i++) {
		if(strcmp(groups[i].groupname, name) == 0) return i;
	}
	return -1;
}

void clients_calc(client_t cls[1010], int counter){
	// bubble sort in descending order
	int c, d;
	for (c = 0; c < counter; c++ ) {
		for (d = 0; d < counter - c - 1; d++) {
			if(strcmp(cls[d].name, "") != 0) {
				if (cls[d].score < cls[d + 1].score) {
					client_t temp = cls[d];
					cls[d] = cls[d+1];
					cls[d+1] = temp;
				}
			}
		}
	}
}

// Quiz Parser thread
void *quiz_parser(void *ign){
	quiz_p_t qstruct = *((quiz_p_t *) ign);
	pthread_mutex_lock(&user_m);
	int fd = users[qstruct.user_n].ssock;
	pthread_mutex_unlock(&user_m);
	int cc, q_count = 0;
	qst_t questions[128];

	char buf2[BUFSIZE];
	cc = read(fd, buf2, 5);

	char *size = (char *) malloc(7);
	strcpy(size, "");
	int i = 0;

	cc = read(fd, buf2, 1);
	buf2[cc] = '\0';
	while(cc > 0 && strcmp(buf2, "|") != 0) {
		strcat(size, buf2);
		cc = read(fd, buf2, 1);
		buf2[cc] = '\0';
	}
	if(cc <= 0) {
		client_leave(fd, &users[qstruct.user_n]);
		pthread_exit(NULL);
	}

	int quiz_size = atoi(size);
	char quiz_file[quiz_size];
	strcpy(quiz_file, "");

	// Read the whole file in one buffer of size = quiz_size
	int x = 0, cc2;
	while(x < quiz_size) {
		if((cc2 = read(fd, buf2, BUFSIZE)) < 0) {
			client_leave(fd, &users[qstruct.user_n]);
			pthread_exit(NULL);
		}
		x += cc2;
		strncat(quiz_file, buf2, cc2);
	}

	quiz_file[quiz_size] = '\0';
	printf("LOG S: finally, quiz = %s\n", quiz_file); fflush(stdout);

	int j = 0, k = 0;
	// According to the specifications, max size of a single question = 2048
	// max size of the answer = 5123
	char quiz_qst[2048], quiz_ans[512];
	printf("LOG S: finally, quiz_size = %d and %d\n", quiz_size, j); fflush(stdout);
	while(j < quiz_size) {
		if(quiz_file[j] == '\n' && quiz_file[j + 1] == '\n') {
			strcpy(quiz_qst, "");
			for( ; k < j; k++){
				strncat(quiz_qst, &quiz_file[k], 1);
			}
			// To dismiss \n and \n
			k += 2;
			strcpy(quiz_ans, "");
			for(;;k++) {
				if(quiz_file[k] == '\n') {
					break;
				}
				strncat(quiz_ans, &quiz_file[k], 1);
			}
			// To dismiss \n and \n
			k += 2;
			strcpy(questions[q_count].q, quiz_qst);
			strcpy(questions[q_count].ans, quiz_ans);
			strcpy(questions[q_count].winner, "WIN|");
			questions[q_count].first = 0;
			questions[q_count].answers = 0;
			printf("LOG S: q# = %d, quiz_qst = %s, quiz_ans = %s, quiz_winner = %s, answered = %d\n", 
					q_count, questions[q_count].q, questions[q_count].ans, 
					questions[q_count].winner, questions[q_count].answers); fflush(stdout);
			q_count++;
			j = k;
			continue;
		}
		j++;				
	}

	pthread_mutex_lock(&group_m);
	groups[groups_count].size = qstruct.groupsize;
	groups[groups_count].current_size = 0;
	groups[groups_count].g_num = groups_count;
	users[qstruct.user_n].user_gr = groups_count;
	groups[groups_count].q_count = q_count;
	groups[groups_count].counter = 0;
	strcpy(groups[groups_count].groupname, qstruct.groupname);
	strcpy(groups[groups_count].quiz_topic, qstruct.topic);
	groups[groups_count].admin = &users[qstruct.user_n];
	pthread_mutex_init(&groups[groups_count].mutex, NULL);
	printf("LOG S QUIZ PARSER: groups_num = %d, groups_count = %d, groupname = %s, size = %d, current size = %d, q_count = %d, admin_name = %s\n", 
						groups_num, groups_count, groups[groups_count].groupname,
						groups[groups_count].size, groups[groups_count].current_size, 
						groups[groups_count].q_count, groups[groups_count].admin->name); fflush(stdout);
	for(int j = 0; j < q_count; j++) {
		groups[groups_count].questions[j] = questions[j];
	}	
	groups_num++;
	groups_count++;
	pthread_mutex_unlock(&group_m);

	FD_SET(fd, &global_afds);
	if(global_nfds < fd + 1) {
		global_nfds = fd + 1;
	}

	users[qstruct.user_n].hasParsed = 1;

	pthread_exit(NULL);
}

// Game Handler thread
void *game_handler(void *ign) {
	fd_set 			afds, readfds;
	int 			nfds = 0, go = 0, fd, adminssock, quiz_read, cc, ww;
	struct timeval 	timeout, timeoutadmin, t0, t1;
	char 			buf[BUFSIZE];
	char 			*z, *start_response;
	char 			get_ogroups[] = "GETOPENGROUPS";
	char 			start[BUFSIZE];
	
	start_response = (char *) malloc(4096);	
	int g_num = *((int *) ign);

	group_t *gr = &groups[g_num];

	printf("GAME HANDLER[169]: GROUP - name = %s, size = %d, current_size = %d, g_num = %d, counter = %d, q_count = %d, quiz_topic = %s, admin = %s\n",
		gr->groupname, gr->size, gr->current_size, gr->g_num, gr->counter, 
		gr->q_count, gr->quiz_topic, gr->admin->name);
	fflush(stdout);

	pthread_mutex_lock(&gr->mutex);
	adminssock = gr->admin->ssock;
	nfds = adminssock + 1;
	int current_size = gr->current_size;
	int size = gr->size;
	int counter = gr->counter;
	pthread_mutex_unlock(&gr->mutex);

	printf("LOG S[168] Game Handler: Adminssock = %d \n", adminssock);
	int prev_value = current_size;
	int prev_counter = counter;

	FD_ZERO(&afds);
	FD_ZERO(&readfds);

	FD_SET(adminssock, &afds);

	// When admin is alone, to make the select() responsive and 
	// a thread to notice arriving clients, a timeout choice is arbitrary
	timeoutadmin.tv_sec = 5; 

	while(1) {
		memcpy((char *)&readfds, (char *)&afds, sizeof(readfds));
		
		gettimeofday(&t0, NULL);
		if(select(nfds, &readfds, (fd_set *)0, (fd_set *)0, &timeoutadmin) < 0) {
			fprintf( stderr, "[329] server select: %s\n", strerror(errno) );
			exit(-1);
		}
		gettimeofday(&t1, NULL);
		timeoutadmin.tv_sec = timeoutadmin.tv_sec - (t1.tv_sec - t0.tv_sec);

		if(go) break;
		for (fd = 0; fd < nfds; fd++) {
			if(go) break;
			if (fd == adminssock && FD_ISSET(fd, &readfds)) {
				printf("FD = %d is set (adminssock = %d) \n", fd, adminssock);
				if ((cc = read( fd, buf, BUFSIZE)) <= 0) {
						end_group(gr);
						FD_ZERO(&afds);
						FD_ZERO(&readfds);
						pthread_exit(NULL);
				} else {
					buf[cc] = '\0';
					buf[strlen(buf) - 2] = '\0';
					
					pthread_mutex_lock(&gr->mutex);
					int current_size = gr->current_size;
					//printf("\nGAME HANDLER[205]: checking current_size - %d, previous_value = %d\n", current_size, prev_value); fflush(stdout);
					if(prev_value < current_size) {
						int counter = gr->counter;
						// printf("GAME HANDLER[208]: counter = %d, previous_counter = %d\n", 
						// 		counter, prev_counter); fflush(stdout);
						for(int i = prev_counter; i < counter; i++){
							int s = gr->clients[i].ssock;
							FD_SET(s, &afds);
							printf("GAME HANDLER[213]: new client, fd = %d, name = %s AND curr_size = %d", 
									s, gr->clients[i].name, current_size); fflush(stdout);
							if(nfds < s + 1) {
								nfds = s + 1;
							}
						}
						prev_counter = counter;
						prev_value = current_size;
					}
					pthread_mutex_unlock(&gr->mutex);

					if(current_size == size) {
						go = 1;
					}

					if(go) break;

					printf("LOG S - GAME HANDLER RECEIVED at fd = %d (adminssock = %d): %s\n", 
						fd, adminssock, buf); fflush(stdout);

					if(strcmp(buf, get_ogroups) == 0){
						strcpy(start, "OPENGROUPS");
						start_response = open_groups(start);
						if(write(fd, start_response, strlen(start_response)) < 0) {
							end_group(gr);
							FD_ZERO(&afds);
							FD_ZERO(&readfds);
							pthread_exit(NULL);
						}

					} else if(strncmp(buf, "CANCEL|", 7) == 0){
						end_group(gr);
						FD_ZERO(&afds);
						FD_ZERO(&readfds);
						pthread_exit(NULL);
					} 
				}
			}
		}

		if(go) break;
		if(timeoutadmin.tv_sec <= 0) {
			timeoutadmin.tv_sec = 5; 
			continue;
		}
	}

	printf("\n[279] GAMEHANDLER: about to start a quiz...\n"); fflush(stdout);

	pthread_mutex_lock(&afds_mutex);
	for(fd = 0; fd < nfds; fd++){
		FD_CLR(fd, &global_afds);
		if (global_nfds == fd + 1) global_nfds--;	
	}
	pthread_mutex_unlock(&afds_mutex);

	int k = 0;
	while(k < gr->q_count) {

		char qsz[5]; // Quiz size placeholder = sizeof(int) + sizeof('\0')
		char qstring[strlen(gr->questions[k].q) + 6 + strlen("QUES|")];
		int qsize = strlen(gr->questions[k].q);
		sprintf(qsz, "%d|", qsize);
		strcpy(qstring, "QUES|");
		strcat(qstring, qsz);
		strcat(qstring, gr->questions[k].q);

		for(fd = 0; fd < nfds; fd++) {
			if((ww = write(fd, qstring, strlen(qstring))) < 0) {
				if(fd == adminssock) {
					FD_CLR(fd, &afds);
				} else {
					FD_CLR(fd, &afds);
					for(int i = 0; i < counter; i++) {
						if(gr->clients[i].ssock == fd) {
							pthread_mutex_lock(&gr->mutex);
							gr->current_size--;
							pthread_mutex_unlock(&gr->mutex);
							client_leave(fd, &gr->clients[i]);
							free(&gr->clients[i]);
						}
					}
					if (nfds == fd + 1) nfds--;	
				}
			}
		}

	// Timeout for the players' responses to the quiz questions
	// - timeout choice according to the specifications
	timeout.tv_sec = 60;
	while(gr->questions[k].answers < gr->current_size) {

		pthread_mutex_lock(&gr->mutex);
		if(gr->current_size == 0) {
			printf("Quiz is finished, bye.\n"); fflush(stdout);
			pthread_exit(NULL);
		}
		pthread_mutex_unlock(&gr->mutex);

		memcpy((char *)&readfds, (char *)&afds, sizeof(readfds));

		gettimeofday(&t0, NULL);
		if(select(nfds, &readfds, (fd_set *)0, (fd_set *)0, &timeout) < 0) {
			fprintf( stderr, "S: Quiz_read select failed: %s\n", strerror(errno) );
			exit(-1);
		}
		gettimeofday(&t1, NULL);
		timeout.tv_sec = timeout.tv_sec - (t1.tv_sec - t0.tv_sec);

		for(fd = 0; fd < nfds; fd++) {
			if(fd != adminssock && FD_ISSET(fd, &readfds)) {
				if ((cc = read( fd, buf, BUFSIZE )) <= 0 ) {
					FD_CLR(fd, &afds);
					for(int i = 0; i < counter; i++) {
						if(gr->clients[i].ssock == fd) {
							pthread_mutex_lock(&gr->mutex);
							gr->current_size--;
							pthread_mutex_unlock(&gr->mutex);
							client_leave(fd, &gr->clients[i]);
							free(&gr->clients[i]);
						}
					}
					if (nfds == fd + 1) nfds--;
				} else {
					pthread_mutex_lock(&gr->mutex);
					gr->questions[k].answers++;
					pthread_mutex_unlock(&gr->mutex);

					buf[cc] = '\0';
					buf[strlen(buf) - 2] = '\0';

					char *foo, *bar;
					foo = strtok_r(buf, "|", &z);
					if(foo != NULL) bar = strtok_r(NULL, "|", &z);

					if(strcmp(gr->questions[k].ans, bar) == 0) {
						pthread_mutex_lock(&gr->mutex);
						gr->questions[k].first++;
						if(gr->questions[k].first == 1) {
							for(int i = 0; i < gr->counter; i++) {
								if(gr->clients[i].ssock == fd) {
									gr->clients[i].score = gr->clients[i].score + 2;
									strcat(gr->questions[k].winner, gr->clients[i].name);
									strcat(gr->questions[k].winner, "\r\n"); 
									break;
								}
							}
						} else {
							for(int i = 0; i < gr->counter; i++) {
								if(gr->clients[i].ssock == fd) {
									gr->clients[i].score++;
									break;
								}
							}
						}
						pthread_mutex_unlock(&gr->mutex);
					} else if(strcmp("NOANS", bar) == 0){
						break;
					} else if(strcmp("LEAVE\r\n", bar) == 0) {
						pthread_mutex_lock(&gr->mutex);

						FD_CLR(fd, &afds);
						if (nfds == fd + 1) nfds--;	

						pthread_mutex_lock(&afds_mutex);
						FD_SET(fd, &global_afds);
						if (global_nfds < fd + 1) global_nfds = fd + 1;
						pthread_mutex_unlock(&afds_mutex);

						gr->current_size--;
						for(int j = 0; j < gr->counter; j++) {
							if(gr->clients[j].ssock == fd) {
								free(&gr->clients[j]);
							}
						}
						pthread_mutex_unlock(&gr->mutex);
					} else {
						for(int i = 0; i < gr->counter; i++) {
							if(gr->clients[i].ssock == fd) {
								gr->clients[i].score--;
								break;
							}
						}
					}
					printf("LOG S GAME HANDLER: question #%d, Winner is - %s\n", 
								k, gr->questions[k].winner); fflush(stdout);
				}
			} else if (!FD_ISSET(fd, &readfds) && timeout.tv_sec <= 0) {
				printf("The timeout for fd = %d \n", fd);

				char end[1024];
				strcpy(end, "END|");
				strcat(end, gr->groupname);
				strcat(end, "\r\n");
				if(write(fd, end, strlen(end)) < 0) {
					for(int i = 0; i < gr->counter; i++) {
						if(gr->clients[i].ssock == fd) {
							client_leave(fd, &gr->clients[i]);
							break;
						}
					}
				}
			}
		}
	}

		if(strcmp("WIN|", gr->questions[k].winner) == 0) {
			strcat(gr->questions[k].winner, "\r\n");			
		} 
		 
		for(fd = 0; fd < nfds; fd++) {
			if((ww = write(fd, gr->questions[k].winner, strlen(gr->questions[k].winner))) < 0) {
				if(fd == adminssock) {
					FD_CLR(fd, &afds);
				} else {
					FD_CLR(fd, &afds);
					for(int i = 0; i < counter; i++) {
						if(gr->clients[i].ssock == fd) {
							pthread_mutex_lock(&gr->mutex);
							gr->current_size--;
							pthread_mutex_unlock(&gr->mutex);
							client_leave(fd, &gr->clients[i]);
							free(&gr->clients[i]);
						}
					}
					if (nfds == fd + 1) nfds--;
				}
			}
		}
		
		k++;
		timeout.tv_sec = 60;	
		printf("[595] Current size is %d\n", gr->current_size); fflush(stdout);
	}

	char final_response[4096];
	strcpy(final_response, "RESULT|");
	for(int i = 0; i < gr->counter; i++) {
		strcat(final_response, gr->clients[i].name);
		strcat(final_response, "|");
		char score[5];
		sprintf(score, "%d|", gr->clients[i].score);
		strcat(final_response, score);
	}
	strcat(final_response, "\r\n");

	for(fd = 0; fd < nfds; fd++) {
			if((ww = write(fd, final_response, strlen(final_response))) < 0) {
				if(fd == adminssock) {
					FD_CLR(fd, &afds);
				} else {
					FD_CLR(fd, &afds);
					for(int i = 0; i < counter; i++) {
						if(gr->clients[i].ssock == fd) {
							pthread_mutex_lock(&gr->mutex);
							gr->current_size--;
							current_size = gr->current_size;
							pthread_mutex_unlock(&gr->mutex);
							client_leave(fd, &gr->clients[i]);
							free(&gr->clients[i]);
						}
					}
					if (nfds == fd + 1) nfds--;
				}
			}
	}

	for(fd = 0; fd < nfds; fd++){
		FD_SET(fd, &global_afds);
		if(global_nfds < fd + 1) {
			global_nfds = fd + 1;
		}
	}

	printf("Quiz is finished, bye.\n"); fflush(stdout);
	pthread_exit(NULL);
}


// Main Thread
int main( int argc, char *argv[] ) {
	char			*service;
	unsigned int	alen;
	int				fd = 0, msock, ssock, *i, rport = 0;
	struct 			sockaddr_in	fsin;
	// Client's requests
	char get_ogroups[] = "GETOPENGROUPS";
	char leave[] = "LEAVE\r\n";
	char sendqz[] = "SENDQUIZ\r\n";
	char group[] = "GROUP";
	char quiz[] = "QUIZ";
	char cancel[] = "CANCEL";
	char join[] = "JOIN"; 
	char end[] = "ENDGROUP";
	struct timeval 	main_timeout, t0, t1;
	// Server's responses
	char start[4096];
	char *start_response = (char *) malloc(4096);
	int ww, cc;
	char *y;
	
	switch (argc) {
		case	1:
			// No args? let the OS choose a port and tell the user
			rport = 1;
			break;
		case	2:
			// User provides a port? then use it
			service = argv[1];
			break;
		default:
			fprintf( stderr, "usage: server [port]\n" );
			exit(-1);
	}

	group_threads = (pthread_t *) malloc(32 * sizeof(pthread_t));

	msock = passivesock( service, "tcp", QLEN, &rport );
	if (rport) {
		//	Tell the user the selected port
		printf( "S: server: port %d\n", rport );	
		fflush( stdout );
	}

	pthread_mutex_init(&group_m, NULL);
	pthread_mutex_init(&group_unique_m, NULL);
	pthread_mutex_init(&user_m, NULL);
	pthread_mutex_init(&groupname_m, NULL);
	pthread_mutex_init(&afds_mutex, NULL);
	
	global_nfds = msock + 1;

	FD_ZERO(&global_afds);
	FD_SET(msock, &global_afds);

	main_timeout.tv_sec = 5; 
	for (;;) {
		// Reset the file descriptors you are interested in
		memcpy((char *)&global_rfds, (char *)&global_afds, sizeof(global_rfds));

		// Only waiting for sockets who are ready to read
		//  - this also includes the close event
		gettimeofday(&t0, NULL);
		if (select(global_nfds, &global_rfds, (fd_set *)0, (fd_set *)0, &main_timeout) < 0) {
			fprintf( stderr, "[710] server select: %s\n", strerror(errno) );
			exit(-1);
		}
		gettimeofday(&t1, NULL);
		main_timeout.tv_sec = main_timeout.tv_sec - (t1.tv_sec - t0.tv_sec);

		// The main socket is ready - it means a new client has arrived
		if (FD_ISSET(msock, &global_rfds)) {
			int	ssock;

			// we can call accept with no fear of blocking
			alen = sizeof(fsin);
			ssock = accept(msock, (struct sockaddr *)&fsin, &alen);
			printf("ACCEPTED ssock = %d \n", ssock); fflush(stdout);
			if (ssock < 0){
				fprintf( stderr, "accept: %s\n", strerror(errno) );
				exit(-1);
			}

			/* start listening to this guy */
			FD_SET( ssock, &global_afds );
			strcpy(start, "OPENGROUPS");
			start_response = open_groups(start);
			ww = write(ssock, start_response, strlen(start_response));
			// increase the maximum
			if ( ssock + 1 > global_nfds ) {
				global_nfds = ssock + 1;
			}
		}

		/*	Handle the participants requests  */
		for(fd = 0; fd < global_nfds; fd++) {
			// check every socket to see if it's in the ready set
			for(int i = 0; i < users_count; i++) {
				if(users[i].ssock == fd && users[i].hasParsed == 1) {
					printf("So, the game begins... adminssock = %d \n", users[i].ssock ); fflush(stdout);
					pthread_mutex_lock(&afds_mutex);
					FD_CLR(fd, &global_afds);
					pthread_mutex_unlock(&afds_mutex);

					int x = users[i].user_gr;
					int *j = (int *) malloc(sizeof(int));
					*j = x;

					users[i].hasParsed = 0;	
					
					if ((pthread_create(&group_threads[x], NULL, game_handler, (void *) j) ) < 0){
						perror( "S: pthread_create error.\n");
						client_leave(fd, &users[x]);
						end_group(&groups[x]);
						continue;
					}
					break;
				}
			}

			if (fd != msock && FD_ISSET(fd, &global_rfds)){
				// read without blocking because data is there
				char buf[BUFSIZE];
				if ((cc = read( fd, buf, BUFSIZE )) <= 0){
					printf( "The client has gone.\n" );
					(void) close(fd);
					FD_CLR( fd, &global_afds );
					// lower the max socket number if needed
					if ( global_nfds == fd + 1 )
						global_nfds--;
				} else {
					buf[cc] = '\0';					
					buf[strlen(buf) - 2] = '\0';
					printf("Main thread has received: %s\n", buf);
					if(strcmp(buf, get_ogroups) == 0){
						strcpy(start, "OPENGROUPS");
						start_response = open_groups(start);
						if(write(fd, start_response, strlen(start_response)) < 0) {
							client_leave(fd, NULL);
						}
					} else if(strcmp(buf, leave) == 0) {
						for(int i = 0; i < users_count; i++) {
							if(users[i].ssock == fd && users[i].hasJoined) {
								int x;
								x = users[i].user_gr;
								pthread_mutex_lock(&groups[x].mutex);
								groups[x].current_size--;
								for(int j = 0; j < groups[x].counter; j++) {
									if(groups[x].clients[j].ssock == fd) {
										free(&groups[x].clients[i]);
									}
								}
								pthread_mutex_unlock(&groups[x].mutex);
							}
						}
					} else if (strncmp(buf, join, 4) == 0) {
						char *foo, *groupname, *username, *in = buf;
						int this_user;

						foo = strtok_r(in, "|", &y);
						groupname = strtok_r(NULL, "|", &y);
						username = strtok_r(NULL, "|", &y);

						printf("LOG S [564]: command - %s, groupname - %s, username - %s\n", 
							foo, groupname, username); fflush(stdout);

						pthread_mutex_lock(&group_unique_m);
						int result = check_groupname(groupname);
						pthread_mutex_unlock(&group_unique_m);

						if(result == - 1) {
							if(write(fd, "NOGROUP\r\n", strlen("NOGROUP\r\n")) < 0) {
								client_leave(fd, &users[this_user]);
								pthread_exit(NULL);
							}
						} else {
							pthread_mutex_lock(&group_m);
							pthread_mutex_lock(&groups[result].mutex);
							if(groups[result].current_size == groups[result].size) {
								if(write(fd, "FULL\r\n", strlen("FULL\r\n")) < 0) {
									client_leave(fd, NULL);
									pthread_exit(NULL);
								}
							} else {
								if(write(fd, "OK\r\n", strlen("OK\r\n")) < 0) {
									client_leave(fd, NULL);
									pthread_exit(NULL);
								}
								pthread_mutex_lock(&user_m);
								users[users_count].score = 0;
								users[users_count].ssock = fd;
								users[users_count].c_num = users_count;
								users[users_count].user_gr = result;
								users[users_count].hasJoined = false;
								users[users_count].isAdmin = false;	
								strcpy(users[users_count].name, username);
								this_user = users_count;
								users_count++;
								pthread_mutex_unlock(&user_m);

								printf("LOG S: name = %s, ssock = %d, score = %d, c_num = %d\n", 
									users[this_user].name, users[this_user].ssock, 
									users[this_user].score, users[this_user].c_num); fflush(stdout);

								int entry = groups[result].counter;
								groups[result].clients[entry] = users[this_user];
								groups[result].current_size++;
								groups[result].counter++;
								printf("User has joined and name = %s, fd = %d at %d\n", groups[result].clients[entry].name,
									groups[result].clients[entry].ssock, entry); fflush(stdout);

								pthread_mutex_lock(&afds_mutex);
								FD_CLR(fd, &global_afds);
								pthread_mutex_unlock(&afds_mutex);

							}
							pthread_mutex_unlock(&groups[result].mutex);
							pthread_mutex_unlock(&group_m);
						}

					} else if (strncmp(buf, group, 5) == 0) {
						char *foo, *topic, *groupname, *adminname, *size, *in = buf;
						int this_admin;

						foo = strtok_r(in, "|", &y);
						topic = strtok_r(NULL, "|", &y);
						groupname = strtok_r(NULL, "|", &y);
						size = strtok_r(NULL, "|", &y);

						int groupsize = atoi(size);

						pthread_mutex_lock(&groupname_m);
						int result = check_groupname(groupname);
						pthread_mutex_unlock(&groupname_m);

						printf("LOG S: command - %s, topic - %s, groupname - %s, size - %d, result - %d\n", 
								foo, topic, groupname, groupsize, result); fflush(stdout);

						if(result >= 0) {
							char response[] = "BAD|Groupname is not unique\r\n";
							write(fd, response, strlen(response));
							continue;
						}
						
						pthread_mutex_lock(&user_m);
						users[users_count].score = 0;
						users[users_count].ssock = fd;
						users[users_count].c_num = users_count;
						users[users_count].hasJoined = false;
						users[users_count].isAdmin = true;	
						users[users_count].hasParsed = 0;	
						strcpy(users[users_count].name, "admin");
						this_admin = users_count;
						users_count++;
						pthread_mutex_unlock(&user_m);

						if(write(fd, sendqz, strlen(sendqz)) < 0) {
							client_leave(fd, &users[this_admin]);
							continue;
						} 

						quiz_p_t quiz_prsr = {
							.user_n = users_count - 1,
							.groupsize = groupsize
						};
						strcpy(quiz_prsr.groupname, groupname);
						strcpy(quiz_prsr.topic, topic);

						pthread_t quiz_parser_t;
						quiz_p_t *i = (quiz_p_t *) malloc(sizeof(quiz_p_t));
						*i = quiz_prsr;	

						pthread_mutex_lock(&afds_mutex);
						FD_CLR(fd, &global_afds);
						if (global_nfds == fd + 1)
							global_nfds--;
						pthread_mutex_unlock(&afds_mutex);

						if ((pthread_create(&quiz_parser_t, NULL, quiz_parser, (void *) i) ) < 0){
							perror( "S: pthread_create error.\n");
							client_leave(fd, &users[this_admin]);
							end_group(&groups[groups_count]);
							continue;
						}
					}

				}
			}
		}

		if(main_timeout.tv_sec <= 0) {
			main_timeout.tv_sec = 5;
			continue;
		}
	}

	pthread_mutex_destroy(&group_m);
	pthread_mutex_destroy(&group_unique_m);
	pthread_mutex_destroy(&user_m);
	pthread_mutex_destroy(&groupname_m);
	pthread_mutex_destroy(&afds_mutex);

	close(msock);

	return 0;
}
