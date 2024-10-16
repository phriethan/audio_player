/*=========================================================================*/
/*  Includes                                                               */
/*=========================================================================*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <system.h>
#include <sys/alt_alarm.h>
#include <io.h>
#include "sys/alt_irq.h"

#include "fatfs.h"
#include "diskio.h"

#include "ff.h"
#include "monitor.h"
#include "uart.h"

#include "alt_types.h"
#include "altera_avalon_pio_regs.h"
#include "altera_avalon_timer_regs.h"
#include <altera_up_avalon_audio.h>
#include <altera_up_avalon_audio_and_video_config.h>


// set up for playing audio
DIR Dir;
FILINFO Finfo;
FRESULT res;
FATFS Fatfs[_VOLUMES];          /* File system object for each logical drive */
FILE *lcd;
uint8_t Buff[1024] __attribute__ ((aligned(4)));  /* Working buffer */
FIL File1, File2;               /* File objects */
long p1, p2, p3;
uint32_t s1, s2, cnt, blen = sizeof(Buff);
unsigned int l_buf;
unsigned int r_buf;
alt_up_audio_dev * audio_dev;


// global variable for song number
int fileIdx = 0;
int closeFile = 0;
// Player state enum
typedef enum {
	PAUSED,
	STOPPED,
	PLAYING
} PlayerState;

// struct to store the player states
typedef struct {
	PlayerState state;
} Player;



// Operation state enum
typedef enum {
	PBACK_NORM_SPD,
	PBACK_HALF_SPD,
	PBACK_DBL_SPD,
	PBACK_MONO
} Operation;

typedef struct {
	Operation op;
} Op;

char filenames[14][20]; // stores the filenames in each row
unsigned long filesizes[14]; // stores the size of each file in the array


// set up for button interrupts
int buttons_val = 0; // values of the buttons (active low)
int isPressed = 0; // flag to check if any button is pressed
int pb_flag = 0;

Operation cur_operation = PBACK_NORM_SPD;
PlayerState cur_state = STOPPED;


int isWav(char *filename);
void songIndexing();
void display_on_lcd();
void changeState();

static void button_ISR(void* context, alt_u32 id);
static void timer_ISR(void* context, alt_u32 id);
int main()
{
	// Initialize disk, initialize files, load the files
	disk_initialize((uint8_t)0);
	f_mount((uint8_t)0, &Fatfs[0]);
	songIndexing();
	display_on_lcd();
	audio_dev = alt_up_audio_open_dev ("/dev/Audio");

	// opening the first file
//	f_open(&File1, filenames[0], (uint8_t) 1);
//	p1 = filesizes[0]; // stores the size of the current file

	// button registers setup
	IOWR(BUTTON_PIO_BASE, 3, 0); // clears the button interrupt
	IOWR(BUTTON_PIO_BASE, 2, 0xF); // enables the interrupt for buttons port

	alt_irq_register(BUTTON_PIO_IRQ, (void *)0, button_ISR); // registering the interrupt for buttons
	alt_irq_register(TIMER_0_IRQ, (void *)0, timer_ISR); // registering the interrupt for timers

	// timer registers setup
	IOWR(TIMER_0_BASE, 0, 0); // make sure the status register is cleared
	IOWR(TIMER_0_BASE, 2, 0xFFFF); // write FFFF to periodl
	IOWR(TIMER_0_BASE, 3, 0x000F); // write F to periodh , making the countdown period FFFFF
	// IOWR(TIMER_0_BASE, 1, 0x8); // precaution to ensure that the timer is stopped initially

	while(1){
		if (cur_state != PAUSED) {
			f_open(&File1, filenames[fileIdx], (uint8_t) 1);
			p1 = filesizes[fileIdx];
		}
		/* AUDIO PART */

		int switch0 = 0;
		int switch1= 0;
		int speed = 1;

		switch0 = IORD(SWITCH_PIO_BASE, 0) & 0x1;
		switch1 = IORD(SWITCH_PIO_BASE, 0) & 0x2;

		// mono
		if(switch0 && switch1){
			speed = 1;
			cur_operation = PBACK_MONO;
		}
		// double speed
		else if(!switch0 && switch1){
			speed = 2;
			cur_operation = PBACK_DBL_SPD;
		}
		// half speed
		else if(switch0 && !switch1){
			speed = 1;
			cur_operation = PBACK_HALF_SPD;
		}
		// normal
		else if(!switch0 && !switch1){
			speed = 1;
			cur_operation = PBACK_NORM_SPD;
		}

		// Checking whether to close the file or not
		if(closeFile == 1){
			f_close(&File1);
			closeFile = 0;
			f_open(&File1, filenames[fileIdx], (uint8_t) 1);
			p1 = filesizes[fileIdx];
		}

		// ascertain the number of bytes left in the file to be read
		while ( p1 > 0 ){
			while(cur_state == PAUSED);

			if(cur_state == STOPPED){
				closeFile = 1;
				break;
			}
			if(pb_flag == 1){
				pb_flag = 0;
				break;
			}

			if(p1 >= blen){
				cnt = blen; // usually 1024
				p1 -= blen;
			}
			else{
				cnt = (uint8_t) p1; // on last iteration: if its less than 1024
				p1 = 0;
			}
			res = f_read(&File1, Buff, cnt, &s2);
			// Error handling
			if (res != FR_OK){
				printf("Error reading the file for Audio.\n");
				return -1;
			}

			// number of bytes remaining to be read
			for(int i = 0; i < s2; i += 4 * speed){

				// maintain byte order; mask higher bits of Buff[i] and Buff[i+2]
				l_buf = Buff[i+1] << 8 | Buff[i];
				r_buf = Buff[i+3] << 8 | Buff[i+2];

				// check space in left channel
				int fifospace = alt_up_audio_write_fifo_space(audio_dev, ALT_UP_AUDIO_LEFT);
				if(cur_operation == PBACK_HALF_SPD){
					if ( fifospace > 0 ){
						alt_up_audio_write_fifo (audio_dev, &(r_buf), 1, ALT_UP_AUDIO_RIGHT);
						alt_up_audio_write_fifo (audio_dev, &(l_buf), 1, ALT_UP_AUDIO_LEFT);
					}
				}
				if ( fifospace > 0 ){
					if(cur_operation == PBACK_MONO){
						alt_up_audio_write_fifo (audio_dev, &(l_buf), 1, ALT_UP_AUDIO_RIGHT);
					}
					else{
						alt_up_audio_write_fifo (audio_dev, &(r_buf), 1, ALT_UP_AUDIO_RIGHT);
					}
					alt_up_audio_write_fifo (audio_dev, &(l_buf), 1, ALT_UP_AUDIO_LEFT);
				}
				else{
					i -= 4 * speed; // retry playing those bytes of data
				}
			}
		}
		if (p1 == 0) {
			cur_state = STOPPED;
			display_on_lcd();
			closeFile = 1;
		}
	}
  return 0;
}

int isWav(char *filename){
	char *extension = ".WAV";
	size_t len = strlen(filename);
	if(len >= 4 && strcmp(filename + len - 4 , extension) == 0){
		return 1;
	}
	return 0;
}

 void songIndexing(){
	 res = f_opendir(&Dir, 0);
	 if (res) // if res in non-zero there is an error; print the error.
	 {
		printf("Root directory not found.\n");
	 }
	 int rowNum = 0;
	 for (;;)
	 {
		 res = f_readdir(&Dir, &Finfo);
		 if ((res != FR_OK) || !Finfo.fname[0]){
			 break;
		 }

		 // Is a .WAV
		 if(isWav(Finfo.fname)){
			 strcpy(filenames[rowNum], Finfo.fname);
			 filesizes[rowNum] =  Finfo.fsize;
			 rowNum++;
		 }
	 }
 }

  void display_on_lcd( )
 {
   lcd = fopen("/dev/lcd_display", "w");

   /* Write some simple text to the LCD. */
	   if (lcd != NULL )
	  {
	   // printing the correct track
	   fprintf(lcd, "%d. %s \n", fileIdx+1, filenames[fileIdx]);
	   switch(cur_state){
		   case PAUSED:
			   fprintf(lcd, "%s \n", "PAUSED");
			   break;
		   case STOPPED:
			   fprintf(lcd, "%s \n", "STOPPED");
			   break;
		   case PLAYING:
			   switch(cur_operation){
				   case(PBACK_NORM_SPD):
						fprintf(lcd, "%s \n", "PBACK-NORM-SPDD");
						break;
				case(PBACK_HALF_SPD):
						fprintf(lcd, "%s \n", "PBACK-HALF-SPDD");
						break;
				case(PBACK_DBL_SPD):
						fprintf(lcd, "%s \n", "PBACK-DBL-SPDD");
						break;
				case(PBACK_MONO):
						fprintf(lcd, "%s \n", "PBACK-MONO");
						break;
			   }
	   }
   }

   fclose( lcd );

   return;
 }

  static void button_ISR(void* context, alt_u32 id){
  	IOWR(BUTTON_PIO_BASE, 2, 0); // clears the button interrupt
  	IOWR(TIMER_0_BASE, 1, 0x5); // set the start and ITO bits to 1; ITO triggers the Timer interrupt
  	IOWR(BUTTON_PIO_BASE, 3, 0); // also clears the button interrupt
  }

  static void timer_ISR(void* context, alt_u32 id){
  	int buttons_cur = IORD(BUTTON_PIO_BASE, 0); // read the current state of the buttons
  	// if no button has been pressed but the current value indicates so
  	if (isPressed == 0 && buttons_cur != 0xF){
  		isPressed = 1; // set the pressed flag to 1
  		buttons_val = buttons_cur;
  	}

  	// if the buttons have been released after a button press
  	else if (isPressed == 1 && buttons_cur == 0xF){
  		isPressed = 0;
  		changeState();
  		buttons_val = buttons_cur;
  		pb_flag = 1;
  	}
  	else {
  		IOWR(TIMER_0_BASE, 1, 0x5); // we restart the timer
  	}
  	// stop the timer
  	IOWR(TIMER_0_BASE, 1, 0x8);

  	display_on_lcd(); // updating the display

  	IOWR(BUTTON_PIO_BASE, 2, 0xF);

  	// clears the timeout bit
  	IOWR(TIMER_0_BASE, 0, 0);
  	// re-enables the interrupt for buttons port

  }

  void changeState(){
	  // PREVIOUS SONG
	if(buttons_val == 7){
		// close the current file
		closeFile = 1;
		fileIdx -= 1;
		if (fileIdx < 0){
			fileIdx = 13;
		}
		if(cur_state == PAUSED || cur_state == STOPPED){
			cur_state = STOPPED;
		}
		else{
			cur_state = PLAYING;
		}
		buttons_val = 0; // resetting the button value to 0
	}

	// STOPPED
	else if(buttons_val == 11){
		cur_state = STOPPED;
		buttons_val = 0; // resetting the button value to 0
	}

	// PAUSE / PLAY
	else if(buttons_val == 13){
		if(cur_state == PAUSED || cur_state == STOPPED){
			cur_state = PLAYING;
		}
		else if (cur_state == PLAYING) {
			cur_state = PAUSED;
		}
		buttons_val = 0; // resetting the button value to 0
	}

	// NEXT SONG
	else if(buttons_val == 14){
		// close the current file
		closeFile = 1;
		fileIdx += 1;
		if(fileIdx >= 14){
			fileIdx = 0;
		}
		if(cur_state == PAUSED || cur_state == STOPPED){
			cur_state = STOPPED;
		}
		else{
			cur_state = PLAYING;
		}
		buttons_val = 0; // resetting the button value to 0
	}

	// RELEASED STATE
	else if(buttons_val == 15){
		buttons_val = 0; // resetting the button value to 0
	}
  }
