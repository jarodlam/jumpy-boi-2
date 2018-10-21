/*
"Jumpy Boi 2"
Jarod Lam
CAB202 project, Semester 2 2018
*/


/* ========================================================================== */
/*  Preprocessor commands                                                     */
/* ========================================================================== */

// Includes
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <cpu_speed.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <graphics.h>
#include <lcd.h>
#include <sprite.h>
#include <lcd_model.h>
#include <macros.h>
#include <usb_serial.h>
#include <cab202_adc.h>

// Parameters
#define STUDENT_NAME "Jarod Lam"
#define STUDENT_NUMBER "n9625607"
#define GAME_NAME "Jumpy Boi 2"
#define DELAY 10
#define MIN_SCREEN_WIDTH 80
#define MIN_SCREEN_HEIGHT 50

#define PLAYER_WIDTH 9
#define PLAYER_HEIGHT 8
#define PLAYER_START_X 1
#define PLAYER_START_Y 1
#define JUMP_SPEED 0.15
#define MOVE_SPEED 1
#define ACCEL_GRAV 0.2
#define TERMINAL_VEL 1
#define INITIAL_LIVES 10
#define ANIM_DURATION 20

#define TREASURE_WIDTH 9
#define TREASURE_HEIGHT 8
#define TREASURE_SPEED 1

#define BLOCK_HEIGHT 2
#define BLOCK_WIDTH 10
#define MAX_BLOCKS 30
#define FORBIDDEN_CHANCE 0.3
#define EMPTY_CHANCE 0.2
#define ROW_HEIGHT 12
#define COL_WIDTH 15
#define ROW_SPEED_MULT 0.002

#define DB_MASK 0b00000111

/* ========================================================================== */
/*  Global variables and types                                                */
/* ========================================================================== */

// Enums
enum switches {SW2, SW3, SWU, SWL, SWD, SWR, SWC};
enum leds {LED0, LED1};
enum pots {POT0, POT1};

// Types
/*
block_t
	Struct for containing data about a block.
*/
typedef struct block_t {
	sprite_id sprite;    // The sprite object for this block
	int row;             // The row number for this block, starting from 0
	int column;          // The column number for this block, starting from 0
	bool safe;           // True if safe '=', false if forbidden 'X'
	bool out_of_bounds;  // True if the block is out of bounds
} block_t;
/*

player_t
	Struct for containing data about the player.
*/
typedef struct player_t {
	sprite_id sprite;     // The sprite object for the player
	int lives;            // Current number of lives
	int score;            // Current score
	bool dead;            // Flag for if the player is dying/dead
	int prev_block;       // Index of the previous block the player stepped on
	int curr_block;       // Index of block the player is on
	int move_dir;         // Direction of movement for player from -1 to 1
} player_t;

/*
treasure_t
	Struct for containing data about the treasure.
*/
typedef struct treasure_t {
	sprite_id sprite;  // The sprite object for the treasure
	bool moving;       // If this treasure is moving or not
} treasure_t;

// Global variables
player_t player;          // Player struct
treasure_t treasure;       // Treasure object
block_t block_array[MAX_BLOCKS];  // Block struct array

int num_rows;
int num_cols;
int num_blocks;
int block_speed;

volatile uint8_t state_count[7] = {0,0,0,0,0,0,0};
volatile bool switch_closed[7] = {0,0,0,0,0,0,0};
volatile uint32_t overflow_counter = 0;  // For tracking time

/* ========================================================================== */
/*  Sprites                                                                   */
/* ========================================================================== */

uint8_t safe_sprite[] = {
	0b11111111, 0b11000000,
	0b11111111, 0b11000000
};
uint8_t forbidden_sprite[] = {
	0b10101010, 0b10000000,
	0b01010101, 0b01000000
};
uint8_t player_sprite[] = {
	0b11110111, 0b10000000,
	0b11011101, 0b10000000,
	0b01000001, 0b00000000,
	0b10100010, 0b10000000,
	0b10100010, 0b10000000,
	0b10001000, 0b10000000,
	0b01000001, 0b00000000,
	0b00111110, 0b00000000 
};
uint8_t treasure_sprite[] = {
	0b00000111, 0b10000000,
	0b00001111, 0b10000000,
	0b00010011, 0b10000000,
	0b00100101, 0b00000000,
	0b01010110, 0b00000000,
	0b01001000, 0b00000000,
	0b10110000, 0b00000000,
	0b11000000, 0b00000000
};

/* ========================================================================== */
/*  Function declarations                                                     */
/* ========================================================================== */

// Main functions
int main(void);
void setup();
void reset();
void process();

// User interface
void intro_screen();
void pause_screen();

// Player
void setup_player();
void update_player();
void player_physics();
void player_standing();
void player_block_motion();
void player_block_collision();
void player_control();
void player_die();

// Blocks
void setup_blocks();
block_t *create_block(int curr_row, int curr_col, bool safe, int num_blocks);
block_t *create_starting_block();
bool generate_safe(int curr_col, int num_cols, int *safe_count);
void update_blocks();

// Treasure
void setup_treasure();
void update_treasure();
void treasure_collect();

// Collision
bool bounding_box_collision (sprite_id s1, sprite_id s2);
//bool pixel_level_collision (sprite_id s1, sprite_id s2);
//int get_coord_list(sprite_id s, int (*sx)[], int (*sy)[], int size);
int check_block_collision(sprite_id s);
int get_current_block(sprite_id s);

// Timers and interrupts
void setup_timers();

// Input/output
void setup_io();
bool read_switch_raw(int switch_id);
bool read_switch(int switch_id);
void set_switch(int switch_id);
void set_led(int led_id, bool state);

// Serial
void setup_usb_serial(void);
void update_usb_serial();
void usb_serial_send(char * message);
void usb_serial_sendf(const char *format, ...);

// Helper functions
void draw_string_centre(int y_offset, char *string);
void draw_formatted_centre(int y_offset, const char * format, ...);
int screen_width();
int screen_height();
double get_elapsed_time();
char get_printable_time(char *time_string);
void sprite_turn_to(sprite_id sprite, double dx, double dy);
void sprite_move_to(sprite_id sprite, int x, int y);
void sprite_step(sprite_id sprite);
double sprite_x(sprite_id sprite);
double sprite_y(sprite_id sprite);
double sprite_dx(sprite_id sprite);
double sprite_dy(sprite_id sprite);
double sprite_width(sprite_id sprite);
double sprite_height(sprite_id sprite);
sprite_id sprite_create(double x,double y,int width,int height,uint8_t image[]);

/* ========================================================================== */
/*  Main functions                                                            */
/* ========================================================================== */

/*
main (void)
	The main C function. Contains essential loops for the game.
*/
int main(void) {
	setup();
	intro_screen();
	bool game_running = true;
	while (game_running) {
		reset();
		while (player.lives > 0) {
			// Loop until dead, then reset
			player.dead = false;
			while (sprite_y(player.sprite) < screen_height()) {
				clear_screen();
				process();
				show_screen();
				_delay_ms(DELAY);
			}
			player.lives--;
			setup_player();
		}
		//game_over(&game_running);
	}
}

/*
setup (void)
	Run once when the game starts to set up variables and other things.
*/
void setup() {
	set_clock_speed(CPU_8MHz);
	lcd_init(LCD_DEFAULT_CONTRAST);
	adc_init();
	setup_io();
	setup_timers();
	setup_usb_serial();
	_delay_ms(500);
}

void reset() {
	player.lives = INITIAL_LIVES;
	player.score = 0;
	overflow_counter = 0;
	setup_blocks();
	setup_player();
	setup_treasure();
	clear_screen();
	show_screen();
	usb_serial_sendf("Game started. player x=%0.1f, player y=%0.1f\n",
		sprite_x(player.sprite), sprite_y(player.sprite));
}

/*
process (void)
	Run repeatedly until player dies. Activates update functions for
	blocks, treasure and player.
*/
void process() {
	clear_screen();
	pause_screen();
	update_usb_serial();
	update_blocks();
	update_player();
	update_treasure();
	show_screen();
}

/* ========================================================================== */
/*  User interface                                                            */
/* ========================================================================== */

/*
intro_screen
	Intro screen showing name and student number when the program starts. Exits
	when SW2 is pressed.
*/
void intro_screen() {
	clear_screen();
	draw_string_centre(-16, GAME_NAME);
	draw_string_centre(-8, STUDENT_NAME);
	draw_string_centre(0, STUDENT_NUMBER);
	draw_string_centre(8, "SW2 TO START");
	show_screen();
	while (!(read_switch(SW2) || usb_serial_getchar() == 's'));
	srand(TCNT3);  // Use the timer to seed the RNG
}

/*
pause_screen
	Pause screen when the joystick centre is pressed once
*/
void pause_screen() {
	if (!read_switch(SWC)) {return;}
	uint32_t pause_overflow = overflow_counter; // Record pause time
	char time_string[10];
	get_printable_time(time_string);
	
	clear_screen();
	draw_string_centre(-16, "PAUSED");
	draw_formatted_centre(-8, "Lives: %d", player.lives);
	draw_formatted_centre(0, "Score: %d", player.score);
	draw_string_centre(8, time_string);
	show_screen();
	
	_delay_ms(500);
	while (!read_switch(SWC));   // Wait for joystick to be pressed
	_delay_ms(200);
	overflow_counter -= overflow_counter - pause_overflow; // Subtract pause
}

/* ========================================================================== */
/*  Player                                                                    */
/* ========================================================================== */

/*
setup_player (void)
	Creates the player sprite with initial values
*/
void setup_player() {
	// Defaults
	player.prev_block = -1;
	player.dead = false;
	// Create the player sprite
	player.sprite = sprite_create(PLAYER_START_X, PLAYER_START_Y,
	                              PLAYER_WIDTH, PLAYER_HEIGHT, player_sprite);
}

/*
update_player
	Controls player movement
*/
void update_player() {
	sprite_step(player.sprite);
	
	player.sprite->dx = 0;

	// Player control
	player_standing();
	player_physics();
	player_block_motion();
	//player_block_collision();
	player_control();
	
	sprite_draw(player.sprite);
}

/*
player_physics
	Controls the physics of the player
*/
void player_physics() {
	double dx = sprite_dx(player.sprite);
	double dy = sprite_dy(player.sprite);
	if (player.curr_block >= 0) {
		dy = 0;
	} else {
		dy += ACCEL_GRAV;
	}
	sprite_turn_to(player.sprite, dx, dy);
}

/*
player_standing
	Handles the player standing on blocks
*/
void player_standing() {
	// Get the current standing block index
	player.curr_block = get_current_block(player.sprite);
	//usb_serial_sendf("Player current block %d\n", player.curr_block);
	// If just landed on a block, set the player's move speed to 0
	if (player.prev_block < 0 && player.curr_block >= 0) {
		player.move_dir = 0;
	}
	// Update the previous block with the current block
	player.prev_block = player.curr_block;
}

/*
player_block_motion
	Handles the player motion due to standing on blocks
*/
void player_block_motion() {
	// Exit the function if not on a block
	if (player.curr_block < 0) return;
	// Set dx to the motion of the block
	double dx = sprite_dx(player.sprite);
	double dy = sprite_dy(player.sprite);
	dx = sprite_dx(block_array[player.curr_block].sprite);
	sprite_turn_to(player.sprite, dx, dy);
}

/*
	Handles the player colliding with a block
*/
void player_block_collision() {
	// Exit the function if standing on a block
	if (player.curr_block >= 0) return;
	// Check for a collided block
	int b = check_block_collision(player.sprite);
	usb_serial_sendf("Player collided with block %d\n", b);
	if (b < 0) return;  // Exit if no collision
	else if (!block_array[b].safe) player_die();  // Die if block is forbidden
	else if (sprite_y(player.sprite) == sprite_y(block_array[b].sprite)+1);
	else {  // Set the horizontal motion to 0
		player.sprite->x -= player.sprite->dx;
		player.move_dir = 0;
	}
}

/*
player_control
	Controls the movement of the player through buttons
*/
void player_control() {
	double dx = sprite_dx(player.sprite);
	double dy = sprite_dy(player.sprite);
	dx += player.move_dir * MOVE_SPEED;
	sprite_turn_to(player.sprite, dx, dy);
	// Exit the function if not standing on a block
	if (player.curr_block < 0) return;
	if (read_switch(SWL) && player.move_dir > -1) {
		player.move_dir -= 1;
	} else if (read_switch(SWR) && player.move_dir < 1) {
		player.move_dir += 1;
	}
}

/*
player_die
	Animates the player dying and then resets the player position
*/
void player_die() {
	
}

/* ========================================================================== */
/*  Blocks                                                                    */
/* ========================================================================== */

/*
setup_blocks (void)
	Sets up blocks randomly with no observable pattern. All blocks are added to
	the block_t array block_array
*/
void setup_blocks() {
	// Row and column parameters
	num_rows = floor(screen_height() / ROW_HEIGHT);
	num_cols = floor(screen_width() / COL_WIDTH);
	// Populate block_array, stopping at max block count
	create_starting_block();
	num_blocks = 1;
	while (num_blocks < MAX_BLOCKS) {
		// Loop over all the rows
		for (int curr_row = 1; curr_row < num_rows; curr_row++) {
			int safe_count = 0;
			// Loop over columns
			for (int curr_col = 0; curr_col < num_cols; curr_col++) {
				// Randomly make this block empty
				if ((double) rand() / (RAND_MAX) < EMPTY_CHANCE) {continue;}
				bool safe = curr_row==0 ? true
			                   : generate_safe(curr_col, num_cols, &safe_count);
				block_t *created_block = create_block(curr_row, curr_col, 
				                                      safe, num_blocks);
				num_blocks++;
			}
		}
		break;
	}
}

/*
create_starting_block
	Creates the starting block in the first row
*/
block_t *create_starting_block() {
	block_array[0].row = 0;
	block_array[0].column = 0;
	block_array[0].safe = true;
	block_array[0].sprite = sprite_create(0, ROW_HEIGHT-2,
		                                BLOCK_WIDTH, BLOCK_HEIGHT, safe_sprite);
	return &block_array[0];
}

/*
create_block
	Creates a block in block_array based on parameters
*/
block_t *create_block(int curr_row, int curr_col, bool safe, int curr_block) {
	// Calculate some parameters
	int x = curr_col * COL_WIDTH;
	int y = (curr_row+1) * ROW_HEIGHT - 2;
	uint8_t *sprite = safe ? safe_sprite : forbidden_sprite;
	// Apply these parameters to the block in block_array
	block_array[curr_block].row = curr_row;
	block_array[curr_block].column = curr_col;
	block_array[curr_block].safe = safe;
	block_array[curr_block].sprite = sprite_create(x, y,
		                                     BLOCK_WIDTH, BLOCK_HEIGHT, sprite);
	return &block_array[curr_block];
}

/*
generate_safe
	Generates a block based on parameters and adds it to block_array
Parameters:
	int curr_col      The current column in the block generation algorithm
	int num_cols      Total number of columns
	int safe_count    Number of safe blocks in this row
Returns:
	block_t*
*/
bool generate_safe(int curr_col, int num_cols, int *safe_count) {
	// Force the block to be safe if there are none in the row
	bool safe = true;
	if (curr_col == num_cols-1 && safe_count <= 0) {
		safe = true;
	} else {
		// Randomly make this block forbidden
		safe = ((double) rand() / (RAND_MAX) > FORBIDDEN_CHANCE);
		safe_count += safe;
	}
	return safe;
}

/*
update_blocks
	Draws all blocks and updates their positions
*/
void update_blocks() {
	sprite_draw(block_array[0].sprite);
	for (int i = 1; i < num_blocks; i++) {
		sprite_id curr_sprite = block_array[i].sprite;
		sprite_step(curr_sprite);
		// Set block speed according to potentiometer
		curr_sprite->dx = pow(-1,block_array[i].row) 
			                                  * adc_read(POT0) * ROW_SPEED_MULT;
		// Wrap around the screen
		int x = sprite_x(curr_sprite);
		int y = sprite_y(curr_sprite);
		if (x < -BLOCK_WIDTH) {
			sprite_move_to(curr_sprite, screen_width(), y);
		} else if (x > screen_width()) {
			sprite_move_to(curr_sprite, -BLOCK_WIDTH, y);
		}
		// Draw block
		sprite_draw(curr_sprite);
	}
}

/* ========================================================================== */
/*  Treasure                                                                  */
/* ========================================================================== */

/*
setup_treasure
	Sets up treasure location and velocity
*/
void setup_treasure() {
	treasure.sprite = sprite_create(1, screen_height()-TREASURE_HEIGHT-3,
		                      TREASURE_WIDTH, TREASURE_HEIGHT, treasure_sprite);
	sprite_turn_to(treasure.sprite, TREASURE_SPEED, 0);
	treasure.moving = true;
}

/*
update_treasure
	Draws and updates the treasure sprite
*/
void update_treasure() {
	if (!treasure.sprite->is_visible) {return;}
	if (read_switch(SW3)) {    // Start and stop the treasure sprite
		treasure.moving = !treasure.moving;
		_delay_ms(10);
	}
	if (treasure.moving) {      // Move the treasure if enabled
		sprite_step(treasure.sprite);
		int x = sprite_x(treasure.sprite);
		double dx = sprite_dx(treasure.sprite);
		if (x <= 0 || x+TREASURE_WIDTH >= screen_width()) {
			dx = -dx;        // Bounce off the walls
		}
		sprite_turn_to(treasure.sprite, dx, sprite_dy(treasure.sprite));
	}
	treasure_collect();
	sprite_draw(treasure.sprite);
}

/*
treasure_collect
	Checks for treasure collision with the player
*/
void treasure_collect() {
	if (bounding_box_collision(player.sprite, treasure.sprite)){
		treasure.sprite->is_visible = false;
		player.lives += 2;
		setup_player();
		char time_string[10];
		get_printable_time(time_string);
		usb_serial_sendf("Player collided with treasure. score=%d, lives=%d, \
game time=%s, player x=%0.1f, player y=%0.1f\n", player.score, player.lives,
			time_string, sprite_x(player.sprite), sprite_y(player.sprite));
	}
}

/* ========================================================================== */
/*  Collision                                                                 */
/* ========================================================================== */

/*
bounding_box_collision
	Checks whether two sprites s1 and s2 collide at bounding box level
Parameters:
	sprite_id s1    First sprite to be compared
	sprite_id s2    Second sprite to be compared
Returns:
	bool          True if there is a collision
*/
bool bounding_box_collision (sprite_id s1, sprite_id s2) {
	// Get some parameters
	int s1w = s1->width; int s1h = s1->height;
	int s2w = s2->width; int s2h = s2->height;
	int s1l = s1->x; int s1t = s1->y;
	int s2l = s2->x; int s2t = s2->y;
	int s1b = s1t+s1h; int s1r = s1l+s1w;
	int s2b = s2t+s2h; int s2r = s2l+s2w;
	
	// Check for collision
	if (s1r > s2l && s1l < s2r && s1b > s2t && s1t < s2b) {
		return 1;
	} else {
		return 0;    // No collision
	}
}

/*
pixel_level_collsion
	Checks whether two sprites s1 and s2 collide at pixel level
Parameters:
	sprite_id s1    First sprite to be compared
	sprite_id s2    Second sprite to be compared
Returns:
	bool          True if there is a collision
*/
/*bool pixel_level_collision (sprite_id s1, sprite_id s2 )
{
    // Generate x and y arrays
	int s1size = sprite_width(s1) * sprite_height(s1);
    int s2size = sprite_width(s2) * sprite_height(s2);
	int s1x[s1size], s1y[s1size], s2x[s2size], s2y[s2size];
	// Populate the arrays
	int ctr1 = get_coord_list(s1, &s1x, &s1y, s1size);
	int ctr2 = get_coord_list(s2, &s2x, &s2y, s2size);
	// Compare the two
	for (int i = 0; i < ctr1; i++) {
		for (int j = 0; j < ctr2; j++) {
			if (s1x[i] == s2x[j] && s1y[i] == s2y[j]) {
				return true;
			}
		}
	}
	return false;
}*/

/*
get_coord_list
	Supporting function for pixel_level_collision
*/
/*int get_coord_list(sprite_id s, int size, int (*sx)[size], int (*sy)[size]) {
	int ctr = 0;
	int width_bits = 8 * ceil(sprite_width(s) / 8);
	for (int y = 0; y < sprite_height(s); y++) {
		for (int x = 0; x < sprite_width(s); x++) {
			// Find the bit and byte index for the current x and y pos
			int pixel_byte = (y * width_bits + x) / 8;
			int pixel_bit  = (y * width_bits + x) % 8;
			// Check if they match, and add to the arrays if they do
			if (BIT_IS_SET(s->bitmap[pixel_byte], pixel_bit)) {
				*sx[ctr] = round(x + sprite_x(s));
				*sy[ctr] = round(y + sprite_y(s));
				ctr++;
			}
		}
	}
	return ctr;
}*/

/*
get_current_block
	Gets the index of the block the sprite is standing on
Returns:
	int    Index of current block, or -1 if falling
*/
int get_current_block(sprite_id s) {
	int sl = round(sprite_x(s));
	int sr = sl + sprite_width(s) - 1;
	int sb = round(sprite_y(s)) + sprite_height(s);
	for (int i = 0; i <= num_blocks; i++) {
		sprite_id b = block_array[i].sprite;
		int bl = round(sprite_x(b));
		int br = bl + sprite_width(b) - 1;
		int bt = round(sprite_y(b));
		if (sr >= bl && sl <= br && sb == bt) {
			return i;
		}
	}
	return -1;
}

/*
check_block_collision
	Checks collision of the sprite with every block on screen
Returns:
	int    Index of collided block, or -1 if none
*/
int check_block_collision(sprite_id s) {
	int collided_ind = -1;
	for (int i = 0; i <= num_blocks; i++) {
		if (bounding_box_collision(s, block_array[i].sprite)) {
			if (!block_array[i].safe) return i; // Prioritise forbidden
			collided_ind = i;
		}
	}
	return collided_ind;
}

/* ========================================================================== */
/*  Timers and interrupts                                                     */
/* ========================================================================== */

/*
setup_timers
	Sets the required values to set up timers
*/
void setup_timers() {
	// Timer 0 (debouncing)
	TCCR0A = 0b00000000;
	TCCR0B = 0b00000100;
	TIMSK0 = 0b00000001;
	// Timer 3 (game time)
	TCCR3A = 0b00000000;
	TCCR3B = 0b00000011;
	TIMSK3 = 0b00000001;
	// Turn on interrupts
	sei();
}

/*
Timer 0
	Timer for debouncing switches
*/
ISR(TIMER0_OVF_vect) {
	for (int i = 0; i <= 6; i++) {
		state_count[i] = ((state_count[i]<<1) & DB_MASK) | read_switch_raw(i);
		if (state_count[i] & DB_MASK == DB_MASK) {
			switch_closed[i] = true;
		} else if (state_count[i] == 0b00000000){
			switch_closed[i] = false;
		}
	}

}

/*
Timer 3
	Timer for game time, gets reset at start of game
*/
ISR (TIMER3_OVF_vect) {
	overflow_counter++;
}


/* ========================================================================== */
/*  Input/output                                                              */
/* ========================================================================== */

/*
setup_io
	Sets up input/output for switches and leds
*/
void setup_io() {
	CLEAR_BIT(DDRF, 6);  // SW2
	CLEAR_BIT(DDRF, 5);  // SW3
	CLEAR_BIT(DDRD, 1);  // SWU
	CLEAR_BIT(DDRB, 1);  // SWL
	CLEAR_BIT(DDRB, 7);  // SWD
	CLEAR_BIT(DDRD, 0);  // SWR
	CLEAR_BIT(DDRB, 0);  // SWC
	SET_BIT(DDRB, 2);    // LED0
	SET_BIT(DDRB, 3);    // LED1
}

/*
read_switch_raw
	Reads the raw value of a switch specified by the enum
Parameters:
	int switch_id    Switch number/enum from 0-6
Returns:
	bool             Switch state true/false
*/
bool read_switch_raw(int switch_id) {
	switch (switch_id) {
		case 0: return BIT_IS_SET(PINF, 6);  // SW2
		case 1: return BIT_IS_SET(PINF, 5);  // SW3
		case 2: return BIT_IS_SET(PIND, 1);  // SWU
		case 3: return BIT_IS_SET(PINB, 1);  // SWL
		case 4: return BIT_IS_SET(PINB, 7);  // SWD
		case 5: return BIT_IS_SET(PIND, 0);  // SWR
		case 6: return BIT_IS_SET(PINB, 0);  // SWC
		default: return false;
	}
}

/*
read_switch
	Reads the value of a switch from the debounced value in switch_closed[]
*/
bool read_switch(int switch_id) {
	if (switch_id > 6 || switch_id < 0) {
		return false;
	} else {
		return switch_closed[switch_id];
	}
}

/*
set_switch
	Artificially sets the value of a switch
*/
void set_switch(int switch_id) {
	if (switch_id > 6 || switch_id < 0) {
		return false;
	} else {
		state_count[switch_id] = BIT_MASK;
		switch_closed[switch_id] = true;
	}
}

/*
set_led
	Set an LED to on or off
Parameters:
	int led_id    LED number 0 or 1
	bool state    Turn on or off
*/
void set_led(int led_id, bool state) {
	switch (led_id) {
		case 0:  // LED0
			if (state) {SET_BIT(PORTB, 2);}
			else       {CLEAR_BIT(PORTB, 2);}
		case 1:  // LED1
			if (state) {SET_BIT(PORTB, 3);}
			else       {CLEAR_BIT(PORTB, 3);}
	}
}

/* ========================================================================== */
/*  Serial                                                                    */
/* ========================================================================== */

/*
setup_usb_serial
	From Topic 10 lecture.
*/
void setup_usb_serial(void) {
	// Set up LCD and display message
	lcd_init(LCD_DEFAULT_CONTRAST);
	draw_string(10, 10, "Connect USB...", FG_COLOUR);
	show_screen();

	usb_init();

	while ( !usb_configured() ) {
		// Block until USB is ready.
	}

	clear_screen();
	draw_string(10, 10, "USB connected", FG_COLOUR);
	show_screen();
}

/*
update_usb_serial
	Checks for serial input and maps characters to buttons
*/
void update_usb_serial() {
	int16_t char_code = usb_serial_getchar();
	if (char_code <= 0) return;
	switch (char_code) {
		case 'a': set_switch(SWL);
		case 'd': set_switch(SWR);
		case 'w': set_switch(SWU);
		case 't': set_switch(SW3);
		case 's': set_switch(SWD);
		case 'p': set_switch(SWC);
	}
}

/*
usb_serial_send
	From Topic 10 lecture.
*/
void usb_serial_send(char * message) {
	usb_serial_write((uint8_t *) message, strlen(message));
}

/*
usb_serial_sendf
	Sends formatted string over serial
Parameters:
	const char *format...    vsprintf arguments
*/
void usb_serial_sendf(const char *format, ...) {
	va_list args;
	va_start(args, format);
	char string[100];
	vsprintf(string, format, args);
	usb_serial_write((uint8_t *) string, strlen(string));
}

/* ========================================================================== */
/*  Helper functions                                                          */
/* ========================================================================== */

/*
draw_string_centre
	Draws string in the centre of the screen at a y offsett
Parameters:
	int y_offset    The height at which to draw the string
*/
void draw_string_centre(int y_offset, char *string) {
	int length = strlen(string) * CHAR_WIDTH;
	int x = screen_width()/2 - length / 2;
	int y = screen_height()/2 + y_offset;
	draw_string(x, y, string, FG_COLOUR);
}

/*
draw_formatted_centre
	Draws string in the centre of the screen formatted using snprintf
Parameters:
	int y_offset            The height at which to draw the string
	const char *format...    vsprintf arguments
*/
void draw_formatted_centre(int y_offset, const char * format, ...) {
	va_list args;
	va_start(args, format);
	char string[100];
	vsprintf(string, format, args);
	draw_string_centre(y_offset, string);
}

/*
screen_width
	Returns width of screen in pixels, ported from ZDK
Returns:
	int    Width of screen in pixels
*/
int screen_width() {
	return LCD_X;
}

/*
screen_height
	Returns height of screen in pixels, ported from ZDK
Returns:
	int    Height of screen in pixels
*/
int screen_height() {
	return LCD_Y;
}

/*
get_elapsed_time()
	Get the current game time from timer 3
Returns:
	double    Game time in seconds
*/
double get_elapsed_time() {
	return ( overflow_counter * 65536.0 + TCNT3 ) * 64  / 8000000;
}

/*
get_printable_time()
	Get the current game time in mm:ss format
Returns:
	char    Game time in mm:ss format
*/
char get_printable_time(char *time_string) {
	double curr_time = get_elapsed_time();
	int minutes = curr_time / 60;
	int seconds = (int) curr_time % 60;
	snprintf(time_string, 10, "%02d:%02d", minutes, seconds);
	return *time_string;
}

/*
sprite_turn_to
	Sets player dx and dy, ported from ZDK
*/
void sprite_turn_to(sprite_id sprite, double dx, double dy) {
	if (dy > TERMINAL_VEL) {
		dy = TERMINAL_VEL; // Stop the sprite from moving too fast
	}
	sprite->dx = dx;
	sprite->dy = dy;
}

/*
sprite_move_to
	Sets player x and y, ported from ZDK
*/
void sprite_move_to(sprite_id sprite, int x, int y) {
	sprite->x = x;
	sprite->y = y;
}

/*
sprite_step
	Steps player forward by dx and dy, ported from ZDK
*/
void sprite_step(sprite_id sprite) {
	sprite->x += sprite->dx;
	sprite->y += sprite->dy;
}

/*
sprite_x
	Returns x position of sprite, ported from ZDK
*/
double sprite_x(sprite_id sprite) {
	return sprite->x;
}

/*
sprite_y
	Returns y position of sprite, ported from ZDK
*/
double sprite_y(sprite_id sprite) {
	return sprite->y;
}

/*
sprite_dx
	Returns dx of sprite, ported from ZDK
*/
double sprite_dx(sprite_id sprite) {
	return sprite->dx;
}

/*
sprite_dy
	Returns dy of sprite, ported from ZDK
*/
double sprite_dy(sprite_id sprite) {
	return sprite->dy;
}

/*
sprite_width
	Returns width of sprite, ported from ZDK
*/
double sprite_width(sprite_id sprite) {
	return sprite->width;
}
/*
sprite_width
	Returns height of sprite, ported from ZDK
*/
double sprite_height(sprite_id sprite) {
	return sprite->height;
}

/*
	Shameless adaptation of the ZDK function
*/
sprite_id sprite_create(double x,double y,int width,int height,uint8_t image[]){
	sprite_id sprite = malloc(sizeof(Sprite));
	sprite_init(sprite, x, y, width, height, image);
	return sprite;
}