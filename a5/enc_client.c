#include <stdio.h> // fopen
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>  // ssize_t
#include <sys/socket.h> // send(),recv()
#include <sys/stat.h>
#include <fcntl.h>
#include <netdb.h>      // gethostbyname()

/**
* Client code
* 1. Create a socket and connect to the server specified in the command arugments.
* 2. Prompt the user for input and send that input as a message to the server.
* 3. Print the message received from the server and exit the program.
*/

int checkCharacter(int ch) {
	int valid = 0;
	if (ch == 32 || ch == 10 || ch == EOF) {
		valid = 1;
	}
	if (ch >= 65 && ch <= 90) {
		valid = 1;
	}
	return valid;
}

// Error function used for reporting issues
void error(const char *msg) { 
  perror(msg); 
  exit(0); 
} 

// Set up the address struct
void setupAddressStruct(struct sockaddr_in* address, 
                        int portNumber, 
                        char* hostname){
 
  // Clear out the address struct
  memset((char*) address, '\0', sizeof(*address)); 

  // The address should be network capable
  address->sin_family = AF_INET;
  // Store the port number
  address->sin_port = htons(portNumber);

  // Get the DNS entry for this host name
  struct hostent* hostInfo = gethostbyname(hostname); 
  if (hostInfo == NULL) { 
    fprintf(stderr, "CLIENT: ERROR, no such host\n"); 
    exit(0); 
  }
  // Copy the first IP address from the DNS entry to sin_addr.s_addr
  memcpy((char*) &address->sin_addr.s_addr, 
        hostInfo->h_addr_list[0],
        hostInfo->h_length);
}

// used to count the length of the message to be sent
int digitCounter(int number) {
	int count = 0;
	while (number !=0) {
		number /= 10;
		++count;
	}
	return count;
}

// sends all the data given
int sendall(int s, char *buf, int *len) {
	int total = 0;
	int bytesleft = *len;
	int n;
	
	//printf("total: %d\n", len);
	while (total < *len) {
		n = send(s, buf+total, bytesleft, 0);
		if (n == -1) { break; }
		total +=n;
		bytesleft -= n;
	}
	*len = total;
	return n == -1 ? -1 : 0;
}

// cli format: enc_client plaintext mykey port
int main(int argc, char *argv[]) {
  int socketFD, charsWritten, charsRead;
	int c;
	char* enc = calloc(5, sizeof(char));
	strcpy(enc, "enc_\0");
	char* plainText;
	char* keyText;
	// concatenation of plaintext and key plus enc prefix
	char* plain_key;
	// the message to be sent
	char* message;
	char* len;
	int digitSize;
	int messageSize;
	// used for checking if the input is valid
	int checkCH;
	size_t i, len_plain, len_key;
	struct stat plainTextSTAT;
	struct stat keyTextSTAT;
	FILE* plainTextFD;
	FILE* keyTextFD;
	int port = atoi(argv[3]);
  struct sockaddr_in serverAddress;
  char buffer[69334];
	char* host = "localhost\0"; 

  // Check usage & args
  if (argc < 4) { 
    fprintf(stderr,"USAGE: %s hostname port\n", argv[0]); 
    exit(0); 
  } 

	// open the plaintext file
	plainTextFD = fopen(argv[1], "r");
	if (plainTextFD == NULL) {
		fprintf(stderr,"PlainTextFile");
		exit(1);
	}

	// open the keytext file
	keyTextFD = fopen(argv[2], "r");	
	if (keyTextFD == NULL) {
		fprintf(stderr, "KeyTextFile");
		exit(1);
	}

	// get the length from each file and create space
	// for file data
	stat(argv[1], &plainTextSTAT);
	len_plain = plainTextSTAT.st_size;
	// space for # at the end of plain text included
	plainText = calloc(len_plain, sizeof(char));
	// keyfile stat
	stat(argv[2], &keyTextSTAT);
	len_key = keyTextSTAT.st_size;
	keyText = calloc(len_key, sizeof(char));

	// the length of the plaintext file cannot be
	// greater than the provided key
	if (len_plain > len_key) {
		fprintf(stderr, "Length of plain text file is greater than the key\n");
		exit(1);	
	}

	// read the plaintext file
	i = 0;
	while (c != EOF) {
		c = fgetc(plainTextFD);	
		if (checkCharacter(c)) {
			plainText[i] = c;	
			i++;
		} else {
			fprintf(stderr, "BAD INPUT\n");
			exit(1);
		}	
	}
	plainText[i-2] = '#';
	plainText[i-1] = '\0';
	plainText[i] = '\0';
	fclose(plainTextFD);

	// reset and read key file
	i = 0;
	c = 0;
	while (c != EOF) {
		c = fgetc(keyTextFD);	
		keyText[i] = c;	
		i++;
	}
	keyText[i-1] = '\0';
	keyText[i] = '\0';
	fclose(keyTextFD);

	// create the message to be sent
	messageSize = strlen(enc) + strlen(plainText) + strlen(keyText);
	digitSize = digitCounter(messageSize);
	len = calloc(digitSize + 1, sizeof(char));

	//stackoverflow.com/questions/8257714/how-to-convert-an-int-to-string-in-c/
	// len is the length that will be appended to the message, after
	// the enc_ prefix
	snprintf(len, digitSize + 1, "%d", messageSize);
	//len[strlen(len) - 1] = '\0';

	message = calloc(messageSize, sizeof(char));
	/*
	printf("len enc: %d, len plain: %d, len key: %d\n, ", 
					strlen(enc), strlen(plainText), strlen(keyText));
	*/

	// concat the plain and key text for the message
	plain_key = calloc(strlen(enc)
														+ strlen(plainText) 
														+ strlen(keyText), sizeof(char));

	// prepend a message that indicates this message is sent from
	// enc_client. idea provided by:
	// https://piazza.com/class/kjc3320l16c2f1?cid=516
	strcpy(plain_key, enc);
	strcat(plain_key, len);
	strcat(plain_key, plainText);
	strcat(plain_key, keyText); 
	strcpy(message, plain_key);
	//printf("plain key message length: %i\n", messageSize);
	//printf("plain key message: %s\n", message);
	
  // Create a socket
  socketFD = socket(AF_INET, SOCK_STREAM, 0); 
  if (socketFD < 0){
    fprintf(stderr, "CLIENT: ERROR opening socket");
  }

  // Set up the server address struct
  setupAddressStruct(&serverAddress, port, host);

  // Connect to server
  if (connect(socketFD, 
							(struct sockaddr*)&serverAddress, 
							sizeof(serverAddress)) < 0){
    fprintf(stderr,"CLIENT: ERROR connecting");
  }

  // Get input message from user
  //printf("CLIENT: Enter text to send to the server, and then hit enter: ");

  // Clear out the buffer array
  memset(buffer, '\0', sizeof(buffer));

  // Get input from the user, trunc to buffer - 1 chars, leaving \0
  //fgets(buffer, sizeof(buffer) - 1, stdin);
  // Remove the trailing \n that fgets adds
  //buffer[strcspn(buffer, "\n")] = '\0'; 

  // Send the plain_key_message to server
  /*
  charsWritten = send(socketFD, 
											plain_key, 
											strlen(plain_key), 0); 
	*/	
	///*
	int size = strlen(message);
	//printf("message: %s\n", message);
	charsWritten = sendall(socketFD, message, &size);
	//*/
  if (charsWritten < 0){
    fprintf(stderr,"CLIENT: ERROR writing to socket");
  }
	
  if (charsWritten < strlen(buffer)) {
    fprintf(stderr,"CLIENT: WARNING: Not all data written to socket!");
  }
	//printf("\n");
  // Get return message from server
  // Clear out the buffer again for reuse
  memset(buffer, '\0', sizeof(buffer));
  // Read data from the socket, leaving \0 at end
  charsRead = recv(socketFD, buffer, sizeof(buffer), 0); 
  if (charsRead < 0){
    fprintf(stderr,"CLIENT: ERROR reading from socket");
  }
	if ((strcmp(buffer, "400") == 0)) {
		//printf("buffer: %s, len: %d, isTrue: %d\n", 
		//				buffer, strlen(buffer), (strcmp(buffer, "400") == 0));
		memset(buffer, '\0', sizeof(buffer));	
		fprintf(stderr, "Wrong Server\n");
		close(socketFD);
		exit(2);
	} else {
		fprintf(stdout, buffer);
		printf("\n");
  	// Close the socket
  	close(socketFD); 
		exit(0);
	}
	///*
	free(plain_key);
	free(message);
	free(len);
	free(keyText);
	free(plainText);
	free(enc);
	//*/
  return 0;
}
