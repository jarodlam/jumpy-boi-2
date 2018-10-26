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
#include <avr/pgmspace.h>
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
#include <ram_utils.h>

// Defines
#define STUDENT_NAME "Jarod Lam"
#define STUDENT_NUMBER "n9625607"
#define GAME_NAME "Jumpy Boi 2"
#define DELAY 10

#define PLAYER_WIDTH 9
#define PLAYER_HEIGHT 8
#define PLAYER_START_X 1
#define PLAYER_START_Y 2
#define JUMP_SPEED 1.5
#define MOVE_SPEED 1
#define ACCEL_GRAV 0.1
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

#define NUM_FOOD 5
#define FOOD_WIDTH 3
#define FOOD_HEIGHT 3

#define NUM_ZOMBIES 5
#define ZOMBIE_WIDTH 9
#define ZOMBIE_HEIGHT 8

#define BIT(x) (1 << (x))
#define DB_MASK 0b00000111
#define OVERFLOW_TOP 1023
#define ADC_MAX 1023
#define DAC_MAX 1023
#define F(s) load_rom_string(PSTR(s))

/* ========================================================================== */
/*  Global variables and types                                                */
/* ========================================================================== */

// Enums
enum switches {SW2, SW3, SWU, SWL, SWD, SWR, SWC};
enum leds {LED0, LED1};
enum pots {POT0, POT1};

// Types
typedef struct block_t {
	sprite_id sprite;    // The sprite object for this block
	int row;             // The row number for this block, starting from 0
	int column;          // The column number for this block, starting from 0
	bool safe;           // True if safe '=', false if forbidden 'X'
} block_t;

typedef struct player_t {
	sprite_id sprite;     // The sprite object for the player
	int lives;            // Current number of lives
	int score;            // Current score
	int prev_block;       // Index of the previous block the player stepped on
	int curr_block;       // Index of block the player is on
	int move_dir;         // Direction of movement for player from -1 to 1
	double resid_speed;   // Residual speed from previous block standing
} player_t;

typedef struct treasure_t {
	sprite_id sprite;  // The sprite object for the treasure
	bool moving;       // If this treasure is moving or not
} treasure_t;

typedef struct food_t {
	sprite_id sprite;  // The sprite object for the food
	int curr_block;    // Index of block the food is on
	int x_offset;      // x offset of food from block
} food_t;

typedef struct zombie_t {
	sprite_id sprite;  // The sprite object for the zombie
	int curr_block;    // Index of block the zombie is on
} zombie_t;

// Global variables
player_t player;                     // Player struct
treasure_t treasure;                 // Treasure struct
block_t block_array[MAX_BLOCKS];     // Block struct array
food_t food_array[NUM_FOOD];         // Food struct array
zombie_t zombie_array[NUM_ZOMBIES];  // Zombie struct array

int num_rows;
int num_cols;
int num_blocks;
double block_speed;
uint32_t zombie_reset_time;

volatile uint8_t state_count[7] = {0,0,0,0,0,0,0};
volatile bool switch_closed[7] = {0,0,0,0,0,0,0};
volatile uint32_t overflow_counter = 0;  // For tracking time

/* ========================================================================== */
/*  Sprites                                                                   */
/* ========================================================================== */

const char PROGMEM safe_sprite[] = {
	0b11111111, 0b11000000,
	0b11111111, 0b11000000
};
const char PROGMEM forbidden_sprite[] = {
	0b10101010, 0b10000000,
	0b01010101, 0b01000000
};
const char PROGMEM player_sprite[] = {
	0b11110111, 0b10000000,
	0b11011101, 0b10000000,
	0b01000001, 0b00000000,
	0b10100010, 0b10000000,
	0b10100010, 0b10000000,
	0b10001000, 0b10000000,
	0b01000001, 0b00000000,
	0b00111110, 0b00000000 
};
const char PROGMEM treasure_sprite[] = {
	0b00000111, 0b10000000,
	0b00001111, 0b10000000,
	0b00010011, 0b10000000,
	0b00100101, 0b00000000,
	0b01010110, 0b00000000,
	0b01001000, 0b00000000,
	0b10110000, 0b00000000,
	0b11000000, 0b00000000
};
const char PROGMEM food_sprite[] = {
	0b11100000,
	0b10100000,
	0b11100000
};
const char PROGMEM zombie_sprite[] = {
	0b11110111, 0b10000000,
	0b11111111, 0b10000000,
	0b01111111, 0b00000000,
	0b11011101, 0b10000000,
	0b11011101, 0b10000000,
	0b11110111, 0b10000000,
	0b01111111, 0b00000000,
	0b00111110, 0b00000000 
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
void screen_intro();
void screen_pause();
void screen_game_over(bool *game_running);
void screen_end();
void screen_fade(int dir);

// Player
void player_setup();
void reset_player();
void player_update();
void player_physics();
void player_standing();
void player_block_motion();
void player_block_collision();
void player_block_forbidden();
void player_block_score();
void player_control();
void player_die();
void player_jump();
void player_boundaries();

// Blocks
void block_setup();
block_t *block_create(int curr_row, int curr_col, bool safe, int num_blocks);
block_t *block_create_starting();
bool block_safe(int curr_col, int num_cols, int *safe_count);
void block_update();

// Treasure
void treasure_setup();
void treasure_update();
void treasure_collect();

// Food
void food_setup();
void food_update();
void food_block_wrap(food_t *f);
void food_place();
int food_search();
int food_inventory();

// Zombies
void zombie_setup();
void zombie_update();
void zombie_physics();

// Collision
bool collision_box (sprite_id s1, sprite_id s2);
//bool pixel_level_collision (sprite_id s1, sprite_id s2);
//int get_coord_list(sprite_id s, int (*sx)[], int (*sy)[], int size);
int collision_block(sprite_id s);
int collision_on_block(sprite_id s);

// Timers and interrupts
void setup_timers();

// Switches
void switch_setup();
bool switch_read_raw(int switch_id);
bool switch_read(int switch_id);
void switch_set(int switch_id);

// LEDs
void led_setup();
void led_set(int led_id, bool state);

// Serial
void usb_serial_setup(void);
void usb_serial_update();
void usb_serial_send(char * message);
void usb_serial_sendf(const char *format, ...);

// Backlight
void backlight_setup();
void backlight_set(int duty_cycle);

// Helper functions
void draw_centref(int y_offset, const char * format, ...);
void draw_centref_P(int y_offset, const char * format, ...);
int screen_width();
int screen_height();
double time_elapsed();
char time_printable(char *time_string);
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
	screen_intro();
	bool game_running = true;
	while (game_running) {
		reset();
		while (player.lives > 0) {
			// Loop until dead, then reset
			clear_screen();
			process();
			show_screen();
			_delay_ms(DELAY);
		}
		screen_game_over(&game_running);
	}
	screen_end();
}

/*
setup (void)
	Run once when the game starts to set up variables and other things.
*/
void setup() {
	set_clock_speed(CPU_8MHz);
	lcd_init(LCD_DEFAULT_CONTRAST);
	adc_init();
	switch_setup();
	led_setup();
	setup_timers();
	backlight_setup();
	usb_serial_setup();
	_delay_ms(500);
}

void reset() {
	player.lives = INITIAL_LIVES;
	player.score = 0;
	overflow_counter = 0;
	block_setup();
	player_setup();
	treasure_setup();
	food_setup();
	clear_screen();
	show_screen();
	usb_serial_sendf(F("Game started. player x=%0.1f, player y=%0.1f\n"),
	                          sprite_x(player.sprite), sprite_y(player.sprite));
}

/*
process (void)
	Run repeatedly until player dies. Activates update functions for
	blocks, treasure and player.
*/
void process() {
	clear_screen();
	screen_pause();
	usb_serial_update();
	player_update();
	food_update();
	block_update();
	treasure_update();
	show_screen();
}

/* ========================================================================== */
/*  User interface                                                            */
/* ========================================================================== */

/*
screen_intro
	Intro screen showing name and student number when the program starts. Exits
	when SW2 is pressed.
*/
void screen_intro() {
	clear_screen();
	draw_centref(-16, F(GAME_NAME));
	draw_centref(-8, F(STUDENT_NAME));
	draw_centref(0, F(STUDENT_NUMBER));
	draw_centref(8, F("SW2 TO START"));
	show_screen();
	while (!(switch_read(SW2) || usb_serial_getchar() == 's'));
	srand(TCNT3);  // Use the timer to seed the RNG
}

/*
screen_pause
	Pause screen when the joystick centre is pressed once
*/
void screen_pause() {
	if (!switch_read(SWC)) {return;}
	uint32_t pause_overflow = overflow_counter; // Record pause time
	char time_string[10];
	time_printable(time_string);
	
	clear_screen();
	draw_centref(-24, "PAUSED");
	draw_centref(-16, "Lives: %d", player.lives);
	draw_centref(-8, "Score: %d", player.score);
	draw_centref(0, "Time: %s", time_string);
	draw_centref(8, "Zombies: ");
	draw_centref(16, "Food: %d", food_inventory());
	show_screen();
	
	_delay_ms(500);
	while (!switch_read(SWC));   // Wait for joystick to be pressed
	_delay_ms(200);
	overflow_counter -= overflow_counter - pause_overflow; // Subtract pause
}

/*
screen_game_over
	Game over screen when the player loses all lives
*/
void screen_game_over(bool *game_running) {
	char time_string[10];
	time_printable(time_string);
	clear_screen();
	draw_centref(-24, F("YOU DIED :("));
	draw_centref(-16, F("Total score: %d"), player.score);
	draw_centref(-8, F("Play time: %s"), time_string);
	draw_centref(0, F("SW3 to reset"));
	draw_centref(8, F("SW2 to end"));
	show_screen();
	while (1) {
		if (switch_read(SW2)) {
			*game_running = false;
			return;
		} else if (switch_read(SW3)) {
			return;
		}
	}
}

/*
screen_end
	Just displays the student number on screen.
*/
void screen_end() {
	clear_screen();
	draw_centref(-8, F(STUDENT_NUMBER));
	show_screen();
}

/*
screen_fade()
	Fades screen backlight and contrast in and out 
*/
void screen_fade(int dir) {
	int backlight_step = (DAC_MAX+1) / 16;
	int contrast_step = (LCD_DEFAULT_CONTRAST+1) / 16;
	int i1, i2;
	if (dir > 0) {i1 =   0; i2 = 15;}
	else         {i1 = -15; i2 =  0;}
	LCD_CMD(lcd_set_function, lcd_instr_extended);
	for (int i = i1; i <= i2; i++) {
		backlight_set(backlight_step * i * dir);
		LCD_CMD(lcd_set_contrast, contrast_step * i * dir);
		_delay_ms(10);
	}
	LCD_CMD(lcd_set_function, lcd_instr_basic);
}

/* ========================================================================== */
/*  Player                                                                    */
/* ========================================================================== */

/*
player_setup (void)
	Creates the player sprite with initial values
*/
void player_setup() {
	// Defaults
	player.prev_block = 0;
	player.move_dir = 0;
	player.resid_speed = 0;
	// Create the player sprite
	char *sprite = load_rom_bitmap(player_sprite, 16);
	player.sprite = sprite_create(PLAYER_START_X, PLAYER_START_Y,
	                                       PLAYER_WIDTH, PLAYER_HEIGHT, sprite);
}

/*
player_update
	Controls player movement
*/
void player_update() {
	sprite_step(player.sprite);
	player.sprite->dx = player.resid_speed;
	player.curr_block = collision_on_block(player.sprite);

	player_standing();
	player_physics();
	player_block_motion();
	//player_block_collision();
	player_block_forbidden();
	player_block_score();
	player_jump();
	player_control();
	player_boundaries();
	
	player.prev_block = player.curr_block;
	sprite_draw(player.sprite);
}

/*
player_physics
	Controls the physics of the player
*/
void player_physics() {
	double dx = sprite_dx(player.sprite);
	double dy = sprite_dy(player.sprite);
	if (player.curr_block >= 0 && player.sprite->dy >= 0) {
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
	// If just landed on a block, set the player's move speed to 0
	if (player.prev_block < 0 && player.curr_block >= 0) {
		player.move_dir = 0;
	}
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
	player.resid_speed = dx;
	sprite_turn_to(player.sprite, dx, dy);
}

/*
player_block_collision
	Handles the player colliding with a block
*/
void player_block_collision() {
	// Exit the function if standing on a block
	if (player.curr_block >= 0) return;
	// Check for a collided block
	int b = collision_block(player.sprite);
	usb_serial_sendf(F("Player collided with block %d\n"), b);
	if (b < 0) return;  // Exit if no collision
	else if (!block_array[b].safe) player_die("forbidden block");
	else if (sprite_y(player.sprite) == sprite_y(block_array[b].sprite)+1);
	else {  // Set the horizontal motion to 0
		//player.sprite->x -= player.sprite->dx;
		player.move_dir = 0;
	}
}

/*
player_block_forbidden
	Kills the player if stepping on a forbidden block
*/
void player_block_forbidden() {
	if (player.curr_block < 0) return;
	if (!block_array[player.curr_block].safe) {
		player_die("forbidden block");
	}
}

/*
player_block_score
	Gives the player a point if stepping on a new safe block
*/
void player_block_score() {
	if (player.curr_block < 0) return;
	if (player.prev_block != player.curr_block &&
	    block_array[player.curr_block].safe) {
		player.score++;
	}
}

/*
player_control
	Controls the movement of the player through buttons
*/
void player_control() {
	double dx = sprite_dx(player.sprite);
	double dy = sprite_dy(player.sprite);
	if (block_speed >= MOVE_SPEED) {
		dx = block_speed * 0.1;
	} else {
		dx += player.move_dir * MOVE_SPEED;
	}
	sprite_turn_to(player.sprite, dx, dy);
	// Exit the function if not standing on a block
	if (player.curr_block < 0) return;
	if (switch_read(SWL) && player.move_dir > -1) {
		player.move_dir -= 1;
	} else if (switch_read(SWR) && player.move_dir < 1) {
		player.move_dir += 1;
	}
}

/*
player_die
	Animates the player dying and then resets the player position
Parameters:
	char *death_reason    Text reason for death to be send over USB serial
*/
void player_die(char *death_reason) {
	player.lives--;
	
	char time_string[10];
	time_printable(time_string);
	usb_serial_sendf("Player died due to %s, lives=%d, score=%d, time=%s\n",
		death_reason, player.lives, player.score, time_string);
	
	screen_fade(-1);
	player_setup();
	process();
	screen_fade(1);
	backlight_set(DAC_MAX);
}

/*
player_jump
	Controls player jumping
*/
void player_jump() {
	if (player.curr_block < 0) return;  // Exit if not grounded
	double dx = sprite_dx(player.sprite);
	double dy = sprite_dy(player.sprite);
	if (switch_read(SWU)) {
		dy = -JUMP_SPEED;
	}
	sprite_turn_to(player.sprite, dx, dy);
}

/*
player_boundaries
	Kills the player if it's out of bounds
*/
void player_boundaries() {
	int pw = sprite_width(player.sprite);
	int ph = sprite_height(player.sprite);
	int pl = round(sprite_x(player.sprite));
	int pt = round(sprite_y(player.sprite));
	int pr = pl + pw;
	int pb = pt + ph;
	// Check collision with sides of screen
	if (pl < 1 || pt < 1 || pb > screen_height() || pr > screen_width()) {
		player_die("boundary collision");
	}
}

/* ========================================================================== */
/*  Blocks                                                                    */
/* ========================================================================== */

/*
block_setup (void)
	Sets up blocks randomly with no observable pattern. All blocks are added to
	the block_t array block_array
*/
void block_setup() {
	// Row and column parameters
	num_rows = floor(screen_height() / ROW_HEIGHT);
	num_cols = floor(screen_width() / COL_WIDTH);
	// Populate block_array, stopping at max block count
	block_create_starting();
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
			                   : block_safe(curr_col, num_cols, &safe_count);
				block_t *created_block = block_create(curr_row, curr_col, 
				                                      safe, num_blocks);
				num_blocks++;
			}
		}
		break;
	}
}

/*
block_create_starting
	Creates the starting block in the first row
*/
block_t *block_create_starting() {
	block_array[0].row = 0;
	block_array[0].column = 0;
	block_array[0].safe = true;
	char *sprite = load_rom_bitmap(safe_sprite, 4);
	block_array[0].sprite = sprite_create(0, ROW_HEIGHT-2,
		                                     BLOCK_WIDTH, BLOCK_HEIGHT, sprite);
	return &block_array[0];
}

/*
block_create
	Creates a block in block_array based on parameters
*/
block_t *block_create(int curr_row, int curr_col, bool safe, int curr_block) {
	// Calculate some parameters
	int x = curr_col * COL_WIDTH;
	int y = (curr_row+1) * ROW_HEIGHT - 2;
	uint8_t *sprite = safe
		          ? load_rom_bitmap(safe_sprite, 4)
	              : load_rom_bitmap(forbidden_sprite, 4);
	// Apply these parameters to the block in block_array
	block_array[curr_block].row = curr_row;
	block_array[curr_block].column = curr_col;
	block_array[curr_block].safe = safe;
	block_array[curr_block].sprite = sprite_create(x, y,
		                                     BLOCK_WIDTH, BLOCK_HEIGHT, sprite);
	return &block_array[curr_block];
}

/*
block_safe
	Generates a block based on parameters and adds it to block_array
Parameters:
	int curr_col      The current column in the block generation algorithm
	int num_cols      Total number of columns
	int safe_count    Number of safe blocks in this row
Returns:
	block_t*
*/
bool block_safe(int curr_col, int num_cols, int *safe_count) {
	// Force the block to be safe if there are none in the row
	bool safe;
	if (curr_col == num_cols-1 && *safe_count <= 0) {
		safe = true;
	// Force the block to be forbidden if there are none in the row
	} else if (curr_col == num_cols-1 && *safe_count >= num_cols-1) {
		safe = false;
	} else {
		// Randomly make this block forbidden
		safe = ((double) rand() / (RAND_MAX) > FORBIDDEN_CHANCE);
		safe_count += safe;
	}
	return safe;
}

/*
block_update
	Draws all blocks and updates their positions
*/
void block_update() {
	sprite_draw(block_array[0].sprite);
	for (int i = 1; i < num_blocks; i++) {
		sprite_id curr_sprite = block_array[i].sprite;
		sprite_step(curr_sprite);
		// Set block speed according to potentiometer
		block_speed = pow(-1,block_array[i].row) 
			                                  * adc_read(POT0) * ROW_SPEED_MULT;
		curr_sprite->dx = block_speed;
		// Wrap around the screen
		int x = round(sprite_x(curr_sprite));
		int y = round(sprite_y(curr_sprite));
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
treasure_setup
	Sets up treasure location and velocity
*/
void treasure_setup() {
	char* sprite = load_rom_bitmap(treasure_sprite, 16);
	treasure.sprite = sprite_create(1, screen_height()-TREASURE_HEIGHT-3,
		                               TREASURE_WIDTH, TREASURE_HEIGHT, sprite);
	sprite_turn_to(treasure.sprite, TREASURE_SPEED, 0);
	treasure.moving = true;
}

/*
treasure_update
	Draws and updates the treasure sprite
*/
void treasure_update() {
	if (!treasure.sprite->is_visible) {return;}
	if (switch_read(SW3)) {    // Start and stop the treasure sprite
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
	if (collision_box(player.sprite, treasure.sprite)){
		treasure.sprite->is_visible = false;
		player.lives += 2;
		player_setup();
		char time_string[10];
		time_printable(time_string);
		usb_serial_sendf(F("Player collided with treasure. score=%d, lives=%d, \
time=%s, player x=%0.1f, player y=%0.1f\n"), player.score, player.lives,
			time_string, sprite_x(player.sprite), sprite_y(player.sprite));
	}
}

/* ========================================================================== */
/*  Food                                                                      */
/* ========================================================================== */

/*
food_setup
	Set up Food sprites and place them on screen
*/
void food_setup() {
	char *sprite = load_rom_bitmap(food_sprite, 3);
	for (int i = 0; i < NUM_FOOD; i++) {
		food_t *f = &food_array[i];
		f->curr_block = -1;
		f->sprite = sprite_create(1, 1, FOOD_WIDTH, FOOD_HEIGHT, sprite);
		f->sprite->is_visible = false;
	}
}

/*
food_update
	Draws and does other things to Food sprites
*/
void food_update() {
	food_place();
	for (int i = 0; i < NUM_FOOD; i++) {
		food_t *f = &food_array[i];
		if (!f->sprite->is_visible) return;
		f->sprite->dx = sprite_dx(block_array[f->curr_block].sprite);
		food_block_wrap(f);
		sprite_step(f->sprite);
		sprite_draw(f->sprite);
	}
}

/*
food_block_wrap
	Makes Food match motion of the block underneath
*/
void food_block_wrap(food_t *f) {
	if (f->curr_block < 0) {
		f->curr_block = collision_on_block(f->sprite);
		if (f->curr_block < 0) return;
	}
	
	int x = round(sprite_x(f->sprite));
	int y = round(sprite_y(f->sprite));
	if (x < -BLOCK_WIDTH+f->x_offset) {
		sprite_move_to(f->sprite, screen_width()+f->x_offset, y);
	} else if (x > screen_width()) {
		sprite_move_to(f->sprite, -BLOCK_WIDTH+f->x_offset, y);
	}
}

/*
food_place
	Places a Food at the player location if SWD and there's any Food left
*/
void food_place() {
	if (!switch_read(SWD) || player.curr_block < 0) return;
	int i = food_search();
	if (i < 0) return;
	int px = round(sprite_x(player.sprite));
	int py = round(sprite_y(player.sprite));
	sprite_move_to(food_array[i].sprite, px+3, py+PLAYER_HEIGHT-3);
	food_array[i].curr_block = collision_on_block(food_array[i].sprite);
	food_array[i].x_offset = px-block_array[food_array[i].curr_block].sprite->x;
	food_array[i].sprite->is_visible = true;
}

/*
food_search
	Finds the index of first available Food in the array, or -1 if none
*/
int food_search() {
	for (int i = 0; i < NUM_FOOD; i++) {
		if (!food_array[i].sprite->is_visible) {
			return i;
		}
	}
	return -1;
}

/*
food_inventory
	Count of number of Food in inventory
*/
int food_inventory() {
	int count = 0;
	for (int i = 0; i < NUM_FOOD; i++) {
		if (!food_array[i].sprite->is_visible) {
			count++;
		}
	}
	return count;
}

/* ========================================================================== */
/*  Zombies                                                                   */
/* ========================================================================== */

/*
zombie_setup
	Sets up Zombie sprites in Zombie array
*/
void zombie_setup() {
	zombie_reset_time = time_elapsed();
	char *sprite = load_rom_bitmap(zombie_sprite, 16);
	for (int i = 0; i < NUM_FOOD; i++) {
		zombie_t *z = &zombie_array[i];
		z->curr_block = -1;
		z->sprite = sprite_create(1, 1, ZOMBIE_WIDTH, ZOMBIE_HEIGHT, sprite);
		z->sprite->is_visible = false;
	}
}

/*
zombie_update
	Draws and does other things to Zombie sprites
*/
void zombie_update() {
	for (int i = 0; i < NUM_ZOMBIES; i++) {
		zombie_t *z = &zombie_array[i];
		if (!z->sprite->is_visible) return;
		
		z->curr_block = collision_on_block(z->sprite);
		zombie_physics();
		
		sprite_step(z->sprite);
		sprite_draw(z->sprite);
	}
}

/*
zombie_physics
	Applies gravity to zombies
*/
void zombie_physics(zombie_t z) {
	double dx = sprite_dx(z.sprite);
	double dy = sprite_dy(z.sprite);
	if (z.curr_block >= 0) {
		dy = 0;
	} else {
		dy += ACCEL_GRAV;
	}
	sprite_turn_to(z.sprite, dx, dy);
}

/* ========================================================================== */
/*  Collision                                                                 */
/* ========================================================================== */

/*
collision_box
	Checks whether two sprites s1 and s2 collide at bounding box level
Parameters:
	sprite_id s1    First sprite to be compared
	sprite_id s2    Second sprite to be compared
Returns:
	bool          True if there is a collision
*/
bool collision_box (sprite_id s1, sprite_id s2) {
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
collision_on_block
	Gets the index of the block the sprite is standing on
Returns:
	int    Index of current block, or -1 if falling
*/
int collision_on_block(sprite_id s) {
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
collision_block
	Checks collision of the sprite with every block on screen
Returns:
	int    Index of collided block, or -1 if none
*/
int collision_block(sprite_id s) {
	int collided_ind = -1;
	for (int i = 0; i <= num_blocks; i++) {
		if (collision_box(s, block_array[i].sprite)) {
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
		state_count[i] = ((state_count[i]<<1) & DB_MASK) | switch_read_raw(i);
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
/*  Switches                                                                  */
/* ========================================================================== */

/*
switch_setup
	Sets up input/output for switches
*/
void switch_setup() {
	CLEAR_BIT(DDRF, 6);  // SW2
	CLEAR_BIT(DDRF, 5);  // SW3
	CLEAR_BIT(DDRD, 1);  // SWU
	CLEAR_BIT(DDRB, 1);  // SWL
	CLEAR_BIT(DDRB, 7);  // SWD
	CLEAR_BIT(DDRD, 0);  // SWR
	CLEAR_BIT(DDRB, 0);  // SWC
}

/*
switch_read_raw
	Reads the raw value of a switch specified by the enum
Parameters:
	int switch_id    Switch number/enum from 0-6
Returns:
	bool             Switch state true/false
*/
bool switch_read_raw(int switch_id) {
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
switch_read
	Reads the value of a switch from the debounced value in switch_closed[]
*/
bool switch_read(int switch_id) {
	if (switch_id > 6 || switch_id < 0) {
		return false;
	} else {
		bool state = switch_closed[switch_id];
		//usb_serial_sendf("%d %d\n",switch_id, state_count[switch_id]);
		state_count[switch_id] = 0b00000000;
		switch_closed[switch_id] = false;
		//usb_serial_sendf("%d %d\n",switch_id, state_count[switch_id]);
		return state;
	}
}

/*
switch_set
	Artificially sets the value of a switch
*/
void switch_set(int switch_id) {
	if (switch_id > 6 || switch_id < 0) {
		return;
	} else {
		state_count[switch_id] = DB_MASK;
		switch_closed[switch_id] = true;
	}
}

/* ========================================================================== */
/*  LEDs                                                                      */
/* ========================================================================== */

/*
led_setup
	Sets up LED pins
*/
void led_setup() {
	SET_BIT(DDRB, 2);    // LED0
	SET_BIT(DDRB, 3);    // LED1
}

/*
led_set
	Set an LED to on or off
Parameters:
	int led_id    LED number 0 or 1
	bool state    Turn on or off
*/
void led_set(int led_id, bool state) {
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
usb_serial_setup
	From Topic 10 lecture.
*/
void usb_serial_setup(void) {
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
usb_serial_update
	Checks for serial input and maps characters to buttons
*/
void usb_serial_update() {
	int16_t char_code = usb_serial_getchar();
	if (char_code <= 0) return;
	switch (char_code) {
		case 'a': switch_set(SWL);
		case 'd': switch_set(SWR);
		case 'w': switch_set(SWU);
		case 't': switch_set(SW3);
		case 's': switch_set(SWD);
		case 'p': switch_set(SWC);
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
/*  Backlight                                                                 */
/* ========================================================================== */

/*
setup_pwm
	Initialise PWM for LCD backlight, adapted from Topic 11 example code
*/
void backlight_setup() {
	// Set to OVERFLOW_TOP ticks per overflow
	TC4H = OVERFLOW_TOP >> 8;
	OCR4C = OVERFLOW_TOP & 0xff;
	// Use OC4A for PWM
	TCCR4A = BIT(COM4A1) | BIT(PWM4A);
	SET_BIT(DDRC, 7);
	// Set pre-scale to "no pre-scale" 
	TCCR4B = BIT(CS42) | BIT(CS41) | BIT(CS40);
	// Select fast PWM
	TCCR4D = 0;
	// Turn on the backlight
	backlight_set(DAC_MAX);
}

/*
backlight_set
	Set PWM for LCD backlight, adapted from Topic 11 example code
*/
void backlight_set(int duty_cycle) {
	// Set bits 8 and 9 of Output Compare Register 4A.
	TC4H = duty_cycle >> 8;
	// Set bits 0..7 of Output Compare Register 4A.
	OCR4A = duty_cycle & 0xff;
}

/* ========================================================================== */
/*  Helper functions                                                          */
/* ========================================================================== */

/*
draw_centref
	Draws string in the centre of the screen formatted using snprintf
Parameters:
	int y_offset            The height at which to draw the string
	const char *format...    vsprintf arguments
*/
void draw_centref(int y_offset, const char * format, ...) {
	va_list args;
	va_start(args, format);
	char string[100];
	vsprintf(string, format, args);
	int length = strlen(string) * CHAR_WIDTH;
	int x = screen_width()/2 - length / 2;
	int y = screen_height()/2 + y_offset;
	draw_string(x, y, (char *) string, FG_COLOUR);
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
time_elapsed()
	Get the current game time from timer 3
Returns:
	double    Game time in seconds
*/
double time_elapsed() {
	return ( overflow_counter * 65536.0 + TCNT3 ) * 64  / 8000000;
}

/*
time_printable()
	Get the current game time in mm:ss format
Returns:
	char    Game time in mm:ss format
*/
char time_printable(char *time_string) {
	double curr_time = time_elapsed();
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