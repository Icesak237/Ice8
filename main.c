#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <unistd.h>
#include <termios.h>


typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int uint32;


const uint8 fontset[80] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
    0x20, 0x60, 0x20, 0x20, 0x70, // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
    0x90, 0x90, 0xF0, 0x10, 0x10, // 4
    0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
    0xF0, 0x10, 0x20, 0x40, 0x40, // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
    0xF0, 0x90, 0xF0, 0x90, 0x90, // A
    0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
    0xF0, 0x80, 0x80, 0x80, 0xF0, // C
    0xE0, 0x90, 0x90, 0x90, 0xE0, // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
    0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

struct termios original_tio;


struct console 
{
// RAM
uint8 memory[4096];

// Register
uint8 V[16];

// Index-Register
uint16 I;

//Program-Counter
uint32 PC;

// Stack
uint16 stack[16];

// Display
uint8 display [64*32];

// Stack-Pointer
uint8 stack_p;

// Timer
uint8 delay_t;
uint8 sound_t;

// Eingabe
uint8 keypad[16];
};



void disable_raw_mode ()
{
	tcsetattr(STDIN_FILENO, TCSANOW, &original_tio);
}

void enable_raw_mode ()
{
	tcgetattr(STDIN_FILENO, &original_tio);
	atexit(disable_raw_mode);

	struct termios raw = original_tio;
	
	raw.c_lflag &= ~(ICANON | ECHO);
	raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;

	tcsetattr(STDIN_FILENO, TCSANOW, &raw);
}



void init_console (struct console *con)
{
	srand(time(NULL));
	
	memset (con->memory, 0, sizeof(con->memory));
	memset (con->V, 0, sizeof(con->V));
	con->PC = 0x200;
	memset(con->display, 0, sizeof(con->display));
	con->stack_p = 0;
	con->I = 0;

	for (int i = 0; i <= 80; i++) 
	{
		
		con->memory[i] = fontset[i];
	}

	memset (con->keypad, 0, sizeof(con->keypad));
}

int insert_ROM (struct console *con, char *filename)
{
	FILE *fp = fopen(filename, "rb");

	if (!fp)
	{
		return 0;
	}

	fseek(fp, 0, SEEK_END);
	long rom_size = ftell(fp);
	rewind(fp);

	if (rom_size > (4096 - 0x200))
	{
		fclose(fp);
		return 0;
	}

	fread(&con->memory[0x200], 1, rom_size, fp);

	fclose(fp);

	return 1;
}



void cli_display (struct console *con)
{
	printf("\033[H");
	for (int y=0; y < 32; y++)
	{
		for (int x=0; x < 64; x++)
		{
			uint8 pixel = con->display[x + (y*64)];
			if(!pixel)
			{
				printf("  ");
			} else
			{
				printf("██");
			}
		}
		printf("\n");
	}
}



void draw (uint8 x, uint8 y, uint8 N, struct console *con)
{
	con->V[0xF] = 0;

	x = x % 64;
	y = y % 32;
	
	for (int i = 0; i <= N-1; i++)
	{
		uint8 byte = con->memory[con->I + i];

		for (int j = 0; j <= 8-1; j++)
		{
			uint8 pixel = (byte >> (7-j)) & 1;
				
			if ((x + j) < 64 && (y + i) < 32)
			{
				if (con->display[( (y+i) * 64) + x + j] & pixel)
				{
					con->V[0xF] = 1;
				}
				con->display[( (y+i) * 64) + x + j] ^=  pixel;
			}
		}
	}
	cli_display(con);
}



void handle_input (struct console *con)
{
	char c;

	// int bytes_read = read(STDIN_FILENO, &c, 1);

	while (read(STDIN_FILENO, &c, 1) > 0)
	{
		// 1 2 3 4  ->  1 2 3 C
		// Q W E R  ->  4 5 6 D
		// A S D F  ->  7 8 9 E
		// Y X C V  ->  A 0 B F 
        
        switch (c)
        {
            case '1': con->keypad[0x1] = 1; break;
            case '2': con->keypad[0x2] = 1; break;
            case '3': con->keypad[0x3] = 1; break;
            case '4': con->keypad[0xC] = 1; break;

            case 'q': con->keypad[0x4] = 1; break;
            case 'w': con->keypad[0x5] = 1; break;
            case 'e': con->keypad[0x6] = 1; break;
            case 'r': con->keypad[0xD] = 1; break;

            case 'a': con->keypad[0x7] = 1; break;
            case 's': con->keypad[0x8] = 1; break;
            case 'd': con->keypad[0x9] = 1; break;
            case 'f': con->keypad[0xE] = 1; break;

            case 'y': // fuer QWERTZ 
            case 'z': con->keypad[0xA] = 1; break;
            case 'x': con->keypad[0x0] = 1; break;
            case 'c': con->keypad[0xB] = 1; break;
            case 'v': con->keypad[0xF] = 1; break;
            
            case 27: exit(0); break;
        }
	}
}



void loop (struct console *con)
{
	// Opcode lesen
	uint16 op = 0;
	op = (con->memory[con->PC] << 8) | con->memory[con->PC+1];
	con->PC += 2;

	// Opcode interpretieren
	uint16 op_type = 0;
	op_type = (op & 0xF000) >> 12;

	uint16 NNN = (op & 0x0FFF);
	uint8 NN = (op & 0x00FF);
	uint8 N = (op & 0x000F);

	uint8 X = (op & 0x0F00) >> 8;
	uint8 Y = (op & 0x00F0) >> 4;

	// Instruktionen
	switch (op_type)
	{
	case 0:
		if (NN == 0xE0)
		{
			memset(con->display, 0, sizeof(con->display));
		} else if (NN == 0xEE) 
		{
			con->stack_p -= 1;
			con->PC = con->stack[con->stack_p];
		}
		break;
	case 1:
		con->PC = NNN; //Sprung auf NNN
		break;

	case 2:
		con->stack[con->stack_p] = con->PC;
		con->stack_p ++;
		con->PC = NNN;
		break;
		
	case 3:
		if (con->V[X] == NN)
		{
			con->PC += 2;
		}
		break;
		
	case 4:
		if (con->V[X] != NN)
		{
			con->PC += 2;
		}
		break;
		
	case 5:
		if (con->V[X] == con->V[Y])
		{
			con->PC += 2;
		}
		break;
		
	case 6:
		con->V[X] = NN; //Setze VX auf NN
		break;

	case 7:
		con->V[X] += NN; //Addiere NN auf VX
		break;
		
	case 8:
		switch (N)
		{
			case 0:
				con->V[X] = con->V[Y];
			break;
			
			case 1:
				con->V[X] |= con->V[Y];
			break;

			case 2:
				con->V[X] &= con->V[Y];
			break; 

			case 3:
				con->V[X] ^= con->V[Y];
			break; 

			case 4:
				if (con->V[X] + con->V[Y] > 255)
				{
					con->V[X] = con->V[X] + con->V[Y] - 256;
					con->V[0xF] = 1;
				} else {
					con->V[X] = con->V[X] + con->V[Y];
					con->V[0xF] = 0;
				}
			break; 

			case 5:
				if (con->V[X] <  con->V[Y])
				{
					con->V[X] = con->V[X] - con->V[Y] + 256;
					con->V[0xF] = 0;
				} else {
					con->V[X] = con->V[X] - con->V[Y];
					con->V[0xF] = 1;
				}
			break; 

			case 6:
				con->V[0xF] = con->V[X] & 0x01;
				con->V[X] >>= 1;
			break; 

			case 7:
				if (con->V[Y] - con->V[X] < 0)
				{
					con->V[X] = con->V[Y] - con->V[X] + 256;
					con->V[0xF] = 0;
				} else {
					con->V[X] = con->V[Y] - con->V[X];
					con->V[0xF] = 1;
				}

			break; 

			case 0xE:
				con->V[0xF] = con->V[X] & 0x80;
				con->V[X] <<= 1;
			break; 
		}
		break;
	case 9:
		if (con->V[X] != con->V[Y])
		{
			con->PC += 2;
		}
		
		break;
	case 0xA:
		con->I = NNN;
		break;
		
	case 0xB:
		con->PC = con->V[0] + NNN;
		break;

	case 0xC:
		con->V[X] = rand() & NN;
		break;

	case 0xD:
		draw(con->V[X], con->V[Y], N, con);
		break;

	case 0xE:
		switch (NN)
		{
			case 0x9E:
				if (con->keypad[con->V[X]] != 0)
				{
					con->PC += 2;
				}
			break;
			case 0xA1:
				if (con->keypad[con->V[X]] == 0)
				{
					con->PC += 2;
				}
			break;
		}
		break;
	
	case 0xF:
		switch (NN)
		{
			case 0x07:
				con->V[X] = con->delay_t;
			break;
			case 0x0A:
			{
				int key_pressed = 0;
				for (int i = 0; i < 16; i++)
				{
					if (con->keypad[i] != 0)
					{
						con->V[X] = i;
						key_pressed = 1;
						break;
					}
				}
				if(!key_pressed)
				{
					con->PC -= 2;
				}
			break;
			}
			case 0x15:
				con->delay_t = con->V[X];
			break;
			case 0x18:
				con->sound_t = con->V[X];
			break;
			case 0x1E:
				con->I += con->V[X];
			break;
			case 0x29:
				con->I = con->V[X]*5;
			break;
			case 0x33:
				con->memory[con->I]   = (con->V[X] / 100) % 10;
				con->memory[con->I+1] = (con->V[X] / 10) % 10;
				con->memory[con->I+2] = con->V[X] % 10;
			break;
			case 0x55:
				for (int i=0; i <= X; i++)
				{
					con->memory[con->I+i] = con->V[0+i];
				}
			break;
			case 0x65:
				for (int i=0; i <= X; i++)
				{
					con->V[0+i] = con->memory[con->I+i];
				}
			break;
		}
		break;
	} 
}





void update_timers (struct console *con)
{
	if (con->delay_t > 0)
	{
		con->delay_t--;
	}
	if (con->sound_t > 0)
	{
		con->sound_t--;
		// Hier Sound TODO
	}
}



int main (int argc, char *argv[])
{
	struct console con;
	struct timespec start_time;
	clock_gettime(CLOCK_MONOTONIC, &start_time);

	struct timespec current_time;

	init_console(&con);

	// TODO ROM im Aufruf spezifizieren
	if (!insert_ROM(&con, argv[1])) 
	{
		return 1;
	}

	enable_raw_mode();
	
	while(1)
	{
		handle_input(&con);
		loop(&con);
		// cli_display(&con);
		
		clock_gettime(CLOCK_MONOTONIC, &current_time);
		// long long delta_sec = current_time.sec - start_time.sec;
		// long long delta_nsec = current_time.nsec - start_time.nsec;

		long long delta_time = (current_time.tv_sec - start_time.tv_sec) * 1000000000LL + current_time.tv_nsec - start_time.tv_nsec;

		if (delta_time > 16666667)
		{
			update_timers(&con);
			clock_gettime(CLOCK_MONOTONIC, &start_time);

			memset(con.keypad, 0, sizeof(con.keypad));
		}		
		
		usleep(1000);
	}

	return 0;
}
