#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int deed_id;
    char deed_name[55];
    int points_for_deed;
    int total_times_done;
    int total_points_earned;
}deed;

typedef struct _deedc {
	int deedID;
	int totaldone;
	struct _deedc *link;
}deedc;

void insertion(deed *x[], int size) {
	int i;
	for(i = 1; i < size; i++) {
		deed *toInsert = x[i];
		int j;
		for (j = i; j>=0; j--) {
			if(j == 0 || x[j-1]->total_times_done <= toInsert->total_times_done) {
				x[j] = toInsert;
				break;
			}
			else {
				x[j] = x[j-1];
			}
		}
	}
}


void bubbleSort(deed *x[], int size) {
	int i, j;
	for (i = 0; i < size - 1; i++) {
		for (j = 0; j < size - 1 - i; j++) {
			if (x[j]->total_points_earned > x[j + 1]->total_points_earned) {
				deed *temp = x[j + 1];
				x[j + 1] = x[j];
				x[j] = temp;
			}
		}
	}
}

_Bool strcomp(char str1[], char str2[]) {
	int i = 0;
	do {
		if (str1[i] < str2[i]) {
			return 1;
		} else if (str1[i] > str2[i]) {
			return 0;
		}
		i++;
	} while (str1[i] != '\0' || str2[i] != '\0');

	return 0;
}

void selectionSort(deed *x[], int size) {
	int i;
	for (i = 0; i < size - 1; i++) {
	      int j, mindex = i;
	      for (j = i + 1; j < size; j++) {
	           if (strcomp(x[j]->deed_name, x[mindex]->deed_name) == 1) {
	                mindex = j;
	           	   }
	          }
	      deed *temp = x[i];
	      x[i] = x[mindex];
	      x[mindex] = temp;
	     }
}

int binarysearch(deed *x[], int size, char word[]) {
	int min = 0;
	int max = size - 1;
	while (min <= max) {
		int guess = (min+max)/2;
		if (strcmp(x[guess]->deed_name, word) == 0) {
			return guess;
		}
		else if(strcmp(x[guess]->deed_name, word) > 0) {
			max = guess - 1;
		}
		else min = guess + 1;
	}
	return -1;
}

void freeAll(deedc *x) {
	if (x != NULL) {
		freeAll(x->link);
		free(x);
	}
}

int main() {
	setvbuf(stdout, NULL, _IONBF, 0);

	FILE *input;
		input = fopen("deed_list.txt", "r");
		if (input == NULL) {
			printf("Error opening file");
			return 1;
		}
		int i, totald;

		for(i = 0; i < 1; i++) {
			fscanf(input, "%i", &totald);
		}

		deed **deed_list=malloc(totald*sizeof(deed));
		do {
			deed_list[i]=malloc(sizeof(deed));
			fscanf(input, "%i %s %i", &deed_list[i]->deed_id, deed_list[i]->deed_name, &deed_list[i]->points_for_deed);
			i++;
		} while(!feof(input));
		fclose(input);

		//reading from file 2 using linked list (as we do not know how many entries there are in the file)
	FILE *input2;
			input2 = fopen("daylog.txt", "r");
			if (input2 == NULL) {
				printf("Error opening file");
				return 1;
			}
		deedc *first = NULL;
		deedc *prev = NULL;
		do {
			deedc *new = (deedc*)malloc(sizeof(deedc));
			fscanf(input2, "%i %i", &new->deedID, &new->totaldone);
			new->link = NULL;
			if(first == NULL) {
				first = new;
			}
			else {
				prev->link = new;
			}
			prev = new;
		}while(!feof(input2));
		fclose(input2);

	//tasks 1 and 2
	int k;
	for (k = 1; k <= totald; k++) { //setting the entries to 0 (as currently they contain garbage)
		deed_list[k]->total_times_done = 0;
		deed_list[k]->total_points_earned = 0;
	}
	while(first != NULL) { //loop through all nodes of linked lists
		for(k = 1; k <= totald; k++) {//loop through all structs to check needed conditions
					if(deed_list[k]->deed_id == first->deedID) {
							deed_list[k]->total_times_done += first->totaldone; //total times done for individual struct
							deed_list[k]->total_points_earned += first->totaldone * deed_list[k]->points_for_deed;//total points earned for ind. struct
							}
				}
			first = first->link;
		}

	//tasks 3, 4
	int totDeeds = 0, totPts = 0;
	for(k = 1; k <= totald; k++) {
		totDeeds += deed_list[k]->total_times_done;
		totPts += deed_list[k]->total_points_earned;
	}

	insertion(deed_list, totald + 1); //task 1 sorting
	printf("\n- TASK 1: sorted by the total times done\n\n");
	printf("Total |      Deed name\n");
	printf("Times |\n");
	for (k = 1; k <= totald; k++) {
		if(deed_list[k]->total_times_done > 0) {
			printf("%3i    %s", deed_list[k]->total_times_done, deed_list[k]->deed_name);
			printf("\n");
		}
	}

	bubbleSort(deed_list, totald + 1); //task 2 sorting
	printf("\n- TASK 2: sorted by the total points earned\n\n");
	printf("Total  |      Deed name\n");
	printf("Points |\n");
	for (k = 1; k <= totald; k++) {
		if(deed_list[k]->total_times_done > 0) {
			printf("%3i    %s", deed_list[k]->total_points_earned, deed_list[k]->deed_name);
			printf("\n");
		}
	}

	printf("\n- TASKS 3 and 4:\n\n");
	printf("Total number of deeds accomplished during the week: %i.\n", totDeeds);
	printf("Total number of \"deed points\" accumulated during the week: %i.\n", totPts);

	selectionSort(deed_list, totald + 1); //task 5 sorting
	printf("\n- TASK 5: sorted by an alphabetical order\n\n");
	printf("Deed ID  |      Deed name\n");
	for (k = 1; k <= totald; k++) {
			if(deed_list[k]->total_times_done > 0) {
				printf("%5i    %s", deed_list[k]->deed_id, deed_list[k]->deed_name);
				printf("\n");
			}
		}

	printf("\n- TASK 6: searching for the occurrence of the user's input\n\n");
	char userin[55]; //initializing a string for placing the input
	while(1) {
		printf("Type in your deed name (enter '0' if you want to terminate the program): ");
		scanf("%55s", userin); //telling the program to read only first 55 characters
		if(strcmp("0", userin) == 0) { //checking program termination condition
			printf("You have terminated the program.\n");
			return 1;
		}
		else {
			binarysearch(deed_list, totald + 1, userin); //function to look for the input string's occurrence
			if(binarysearch(deed_list, totald + 1, userin) != -1) {
				printf("\nDeed ID | Total times done | Total points earned\n");
				printf("%5i %13i %18i\n\n", deed_list[binarysearch(deed_list, totald + 1, userin)]->deed_id, deed_list[binarysearch(deed_list, totald + 1, userin)]->total_times_done, deed_list[binarysearch(deed_list, totald + 1, userin)]->total_points_earned);
			}
			else printf("No such deed.\n\n");
		}
	}

	freeAll(first);
	return 0;
}
