/*
 * "Hello World" example.
 *
 * This example prints 'Hello from Nios II' to the STDOUT stream. It runs on
 * the Nios II 'standard', 'full_featured', 'fast', and 'low_cost' example
 * designs. It runs with or without the MicroC/OS-II RTOS and requires a STDOUT
 * device in your system's hardware.
 * The memory footprint of this hosted application is ~69 kbytes by default
 * using the standard reference design.
 *
 * For a reduced footprint version of this template, and an explanation of how
 * to reduce the memory footprint for a given application, see the
 * "small_hello_world" template.
 *
 */

#include <stdio.h>
#include "sys/alt_irq.h"
#include "io.h"
#include "system.h"
#include "altera_avalon_pio_regs.h"
#include "altera_avalon_timer_regs.h"


int buttons_val = 0; // values of the buttons (active low)
int isPressed = 0; // flag to check if any button is pressed
int button_real_val = 0;

static void button_ISR(void* context, alt_u32 id){
	button_real_val = IORD(BUTTON_PIO_BASE, 0);
	IOWR(BUTTON_PIO_BASE, 2, 0); // clears the button interrupt
	IOWR(TIMER_0_BASE, 1, 0x5); // set the start and ITO bits to 1; ITO triggers the Timer interrupt
	printf("buttons pressed \n");
	IOWR(BUTTON_PIO_BASE, 3, 0); // also clears the button interrupt
}

static void timer_ISR(void* context, alt_u32 id){

	int buttons_cur = button_real_val; // read the current state of the buttons
	printf("timer entered buttons val %d\n", buttons_cur);
	// if no button has been pressed but the current value indicates so
	if (isPressed == 0 && buttons_cur != 0xF){
		isPressed = 1; // set the pressed flag to 1
		buttons_val = buttons_cur;
	}

	// if the buttons have been released after a button press
	else if (isPressed == 1 && buttons_cur == 0xF){
		isPressed = 0;
		buttons_val = buttons_cur;
	}
	else {
		IOWR(TIMER_0_BASE, 1, 0x5); // we restart the timer
	}


	// stop the timer
//	IOWR(TIMER_0_IRQ, 1, 0x8);
	// clears the timeout bit
	IOWR(TIMER_0_IRQ, 0, 0);
	// re-enables the interrupt for buttons port
	IOWR(BUTTON_PIO_BASE, 2, 0xF);
	button_real_val = 0;
}

int main()
{
	// button registers setup
	IOWR(BUTTON_PIO_BASE, 3, 0); // clears the button interrupt
	IOWR(BUTTON_PIO_BASE, 2, 0xF); // enables the interrupt for buttons port

	alt_irq_register(BUTTON_PIO_IRQ, (void *)0, button_ISR); // registering the interrupt for buttons
	alt_irq_register(TIMER_0_IRQ, (void *)0, timer_ISR); // registering the interrupt for timers
	// timer registers setup
	IOWR(TIMER_0_BASE, 0, 0); // make sure the status register is cleared
	IOWR(TIMER_0_BASE, 2, 0xFFFF); // write FFFF to periodl
	IOWR(TIMER_0_BASE, 3, 0x000F); // write F to periodh , making the countdown period FFFFF
	IOWR(TIMER_0_BASE, 1, 0x8); // precaution to ensure that the timer is stopped initially



	while(1){
		if(buttons_val == 7){
			printf("pb 3 is pressed.\n");
			buttons_val = 0; // resetting the button value to 0
		}
		else if(buttons_val == 11){
			printf("pb 2 is pressed.\n");
			buttons_val = 0; // resetting the button value to 0
		}
		else if(buttons_val == 13){
			printf("pb 1 is pressed.\n");
			buttons_val = 0; // resetting the button value to 0
		}
		else if(buttons_val == 14){
			printf("pb 0 is pressed.\n");
			buttons_val = 0; // resetting the button value to 0
		}
		else if(buttons_val == 15){
			printf("button released\n");
			buttons_val = 0; // resetting the button value to 0
		}
	}
  return 0;
}
