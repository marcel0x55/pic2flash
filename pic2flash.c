#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>


int main(int argc, char* argv[]){
	if  (argc > 1 && strcmp(argv[1], "h") == 0)
		goto usage;
	
	else if (argc != 4)
		goto errarg;

	unsigned char data;

	int uart = open(argv[3], O_RDWR | O_NOCTTY);
	if (uart < 0){
		printf("Error opening device %s! (maybe try again as superuser?)\n", argv[3]);
		exit(-1);
	}

	struct termios tty;
	
	tcgetattr(uart, &tty);

	cfsetospeed(&tty, B500000);
	cfsetispeed(&tty, B500000);
	
	tty.c_cflag |= (CLOCAL | CREAD);
	tty.c_cflag &= ~PARENB;
	tty.c_cflag &= ~CSTOPB;
	tty.c_cflag &= ~CSIZE;
	tty.c_cflag |= CS8;

	tty.c_lflag = 0;
	tty.c_oflag = 0;
	tty.c_iflag = 0;

	tty.c_cc[VMIN] = 1;
	tty.c_cc[VTIME] = 0;

	tcsetattr(uart, TCSANOW, &tty);

	if (strcmp(argv[1], "w") == 0)
		goto write;

	else if (strcmp(argv[1], "r") == 0)
		goto read;

	else{ 
		close(uart);
		goto errarg;
	}

	write:
		printf("Writing %s to flash using device %s. Please wait. \n", argv[2], argv[3]);
		
		int in = open(argv[2], O_RDONLY);
		if (in < 0){
			printf("Error opening file %s!", argv[2]);
			exit(-1);
		}

		data = 0xFF;
		if ((lseek(in, 0, SEEK_END)) < 32768){
			printf("File is smaller than flash! Will pad with zeros and provide a padded file (%s.padded) for using with diff.\n", argv[2]);
			
			char dest[256];
			snprintf(dest, sizeof(dest), "%s.padded", argv[2]);
			
			char cmd[512];
			snprintf(cmd, sizeof(cmd), "cp \"%s\" \"%s\"", argv[2], dest);
			system(cmd);

			int padded = open(dest, O_WRONLY | O_CREAT, 0666);
			if (padded < 0){
				printf("Error opening file %s!", dest); 
				exit(-1);
			}

			off_t size_dif = lseek(padded, 0, SEEK_END);
			int j;
			for (j = 32768 - size_dif; j > 0; j--)
				write(padded, &data, 1);
			
			close(padded);
			
			in = open(dest, O_RDONLY);
			if (in < 0){
				printf("Error opening file %s!", dest); 
				exit(-1);
			}
			
		}

		lseek(in, 0, SEEK_SET);
		
		printf("Sending write command...\n");
		write(uart, "W", 1);

		printf("Waiting for ACK...\n");
		while(true){
			if ((read(uart, &data, 1)) == -1){
				printf("Error reading from %s!\n", argv[3]);
				exit(-1);
			}
			//printf("While waiting for ack received %x\n", data);
			if (data == 'A')
				break;
		}
		unsigned char prev;
		printf("Writing:");
		while ((read(in, &data, 1)) > 0){

			prev = data;
			//printf("Data after reading: %x\n",data);
			
			if ((write(uart, &data, 1)) == -1){
				printf("Error writing to %s!\n", argv[3]);
				exit(-1);
			}

			while(true){
				if ((read(uart, &data, 1)) == -1){
					printf("Error reading from %s!\n", argv[3]);
					exit(-1);
				}

				if (data == 'W'){
					printf("W");
					break;}

				else if (data == 'G'){
					printf("."); fflush(stdout);

					while(true){
						if((read(uart, &data, 1)) == -1){
							printf("Error reading from %s!\n", argv[3]);
							exit(-1);
						}

						if (data == 'G')
							break;
					}
				break;
				}
			}
		}
		printf("\n");

		close(in);
		close(uart);

		printf("Done writing. Bye.\n");
		return 0;

	read:
		printf("Reading flash to %s using device %s. Please wait. \n", argv[2], argv[3]);
		
		int out = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0666);
		if (out < 0){
			printf("Error opening file %s!", argv[2]);
			exit(-1);
		}
		
		printf("Sending read command...\n");
		write(uart, "R", 1);

		printf("Waiting for ACK...\n");
		while((read(uart, &data, 1)) > 0)
			if (data == 'A')
				break;
		
		printf("Reading:");
		int i;
		for (i = 32768; i > 0; i--){
			read(uart, &data, 1);
			write(out, &data, 1);
			if ((i % 64) == 0){
				printf(".");
				fflush(stdout);
			}
		}
		
		printf("\n");
		
		close(out);
		close(uart);

		printf("Done reading. Bye.\n");
		
		return 0;


	errarg:
		printf("Invalid arguments!\n");
		
	usage:
		printf("Usage:\n");
		printf("\t picflash <command> <path/to/file> <serial device>\n");
		printf("Commands:\n");
		printf("\t r \t : read flash contents to file\n");
		printf("\t w \t : write file to flash\n");
		printf("\t h \t : print help\n");
		printf("Examples:\n");
		printf("\t picflash r file.bin /dev/ttyUSB0\n");
		printf("\t picflash w file.bin /dev/ttyUSB0\n");
		return 0;
}
