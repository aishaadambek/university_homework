// Aishabibi Adambek Homework 2

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>

void check_permissions(char *filename){
	if(access(filename, R_OK) != 0 || access(filename, W_OK) != 0){
		printf("Read or/and Write permissions denied. Check your input.\n");
		exit(-1);
	}
}

bool check_PNG(unsigned char *signature){
	unsigned char correct[] = {137, 80, 78, 71, 13, 10, 26, 10};
	for(int i = 0; i < 8; i++){
			if(*(signature + i) != correct[i]) return false;
	}
	return true;
}


int main(int argc, char *argv[]){
	static char IHDR[] = "IHDR";
	static char IDAT[] = "IDAT";
	static char IEND[] = "IEND";
	int i = 0;

	if(argc != 2) {
		printf("Wrong number of arguments. Check your input. \n");
		exit(-1);
	}
	check_permissions(argv[1]);

	int fdin = open(argv[1], O_RDWR);
	if(fdin < 0) {
		printf("Error opening file. \n");
		exit(-1);
	}

	// check if it is a PNG file
	unsigned char *signature = (unsigned char *) malloc(8);
	read(fdin, signature, 8);
	if(!check_PNG(signature)) {
		printf("PNG file expected. Check your input. \n");
		exit(-1);
	} 

	while(1){
		// read info from length field of chunk to know how many bytes to skip
		// use unsigned char to handle large values
		int val = 0;
		read(fdin, &val, 4);
		int offset = htonl(val);
		printf("LOG: %d - OFFSET: %d \n", i, offset);
		
		// check type of the chunk | char is permissible here as these are ASCII chars 
		char *type = (char *) malloc(5);
		read(fdin, type, 4);
		type[4] = '\0';
		printf("LOG: %d - %s \n", i, type);

		// perform XOR on IDAT OR skip on other chunk types
		if(strcmp(type, IDAT) == 0){
			for(int j = 0; j < offset; j++){
				unsigned char *x = (unsigned char *) malloc(1);
				read(fdin, x, 1);
				lseek(fdin, -1, SEEK_CUR);
				*x = (unsigned char) *(x) ^ 232;
				write(fdin, x, 1);
				free(x);
			}
			lseek(fdin, 4, SEEK_CUR); // skip 4 bytes for CRC field
		} else if(strcmp(type, IEND) == 0){ // exit loop
			free(type);
			break;
		} else {
			lseek(fdin, offset + 4, SEEK_CUR); // skip data field + 4 bytes for CRC field
		}

		// release memory you don't need anymore
		free(type);
		i++;
	}

	close(fdin);
	
	return 0;
}