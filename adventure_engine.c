#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <conio.h> 
#include <cbm.h>
#include <time.h>

#include "ui_data.h" 

// =========================================================
// SYSTEM COORDINATES - EDIT THESE DIRECTLY HERE!
// =========================================================
#if defined(__C64__) || defined(__C128__)
    #define UI_VRAM ((unsigned char*)0x0400)
    #define UI_CRAM ((unsigned char*)0xD800)
    
    // Nudge these to align text inside your green boxes
    #define UI_NAME_X 1
    #define UI_NAME_Y 2
    #define UI_HP_X   11
    #define UI_HP_Y   2
    #define UI_GP_X   16
    #define UI_GP_Y   2
    #define UI_EXP_X  21
    #define UI_EXP_Y  2
	#define UI_EQ1_X  27 // equiped has 4 char on top
	#define UI_EQ1_SZ 4 // equiped has 4 char on top
    #define UI_EQ1_Y  1 // top line position    
    #define UI_EQ_X   26 //need two rows.. 5 charaters in the next row
	#define UI_EQ_SZ  6 //need two rows.. 5 charaters in the next row
    #define UI_EQ_Y   2
	#define UI_Q_X    34  // quest 
    #define UI_Q_Y    2
#elif defined(__VIC20__)
    #define UI_VRAM ((unsigned char*)0x1E00)
    #define UI_CRAM ((unsigned char*)0x9600)
#endif

// --- CONSTANTS ---
#define NO_EXIT -99
#define INVENTORY -1
#define DESTROYED -99 
#define MAX_ROOMS 30    
#define MAX_ITEMS 50 
#define MAX_ACTORS 20 
#define INPUT_LEN 40

#define TYPE_TOOL 0
#define TYPE_WEAPON 1
#define TYPE_TREASURE 2
#define TYPE_HEAL 3   

#define MIN_X 2
#define MAX_X 39
#define MIN_Y 6
#define MAX_Y 23

// --- STRUCTURES ---
typedef struct {
    char name[32];
    char description[200]; 
    int n, s, e, w, u, d;
} Room;

typedef struct {
    char name[32];
    char description[128];
    int location;
    int type;
    int stat;
	int is_quest;
} Item;

typedef struct {
    char name[32];
    char description[128];
    int location;
    int hp;
    int damage;
	int is_quest;
} Actor;

// --- GLOBALS ---
FILE *boot_file = NULL; 
Room current_room_data;
Room dummy_room_data;  
Item items[MAX_ITEMS];   
Actor actors[MAX_ACTORS];

int total_rooms = 0;
int total_items = 0;
int total_actors = 0; 
int current_room_id = 0;
int playing = 1;
int equipped_weapon = -1;

char player_name[9] = "hero";
char intro_text[512]; 
char last_command[INPUT_LEN] = "";

int player_max_hp = 100;
int player_hp = 100;
int player_gold = 0;
int player_exp = 0;
int quests_completed = 0, total_quests = 0;

char message_buffer[512] = ""; 
char global_inv_text[256]; 

int text_x = MIN_X; 
int text_y = MIN_Y;

// --- CORE UTILITIES ---
int is_match(const char *input, const char *command) {
    if (input == NULL || command == NULL) return 0;
    while (*input && *command) {
        if (*input != *command) return 0;
        input++; command++;
    }
    return (*input == *command);
}

int match_item(const char *disk_name, const char *input_noun) {
    int i = 0;
    while (input_noun[i] != '\0') {
        if (disk_name[i] == '\0') return 0; 
        if (disk_name[i] != input_noun[i]) return 0;
        i++;
    }
    return (disk_name[i] == '\0' || disk_name[i] == ' ');
}

void append_num(char *dest, int num) {
    char buf[10];
    int i = 0, j;
    char temp;
    if (num == 0) { strcat(dest, "0"); return; }
    while (num > 0) {
        buf[i++] = (num % 10) + '0';
        num /= 10;
    }
    buf[i] = '\0';
    for(j=0; j<i/2; j++) {
        temp = buf[j];
        buf[j] = buf[i-1-j];
        buf[i-1-j] = temp;
    }
    strcat(dest, buf);
}

int roll_d20() { return (rand() % 20) + 1; }

void set_msg(const char *text) { strcpy(message_buffer, text); }

void delay_seconds(int seconds) {
    clock_t goal = clock() + (seconds * CLOCKS_PER_SEC);
    while (clock() < goal) {} 
}

// --- HUD HELPERS ---
unsigned char petscii_to_screencode(unsigned char c) {
    if (c >= 64 && c <= 95) return c - 64;
    if (c >= 96 && c <= 127) return c - 32;
    if (c >= 128 && c <= 159) return c + 64;
    if (c >= 160 && c <= 191) return c - 64;
    if (c >= 192 && c <= 254) return c - 128;
    if (c == 255) return 94;
    return c; 
}

void poke_hud_str(int x, int y, const char* str, unsigned char color, int width) {
    int offset = (y * 40) + x;
    int i = 0;
    for (i = 0; i < width; i++) {
        if (str[i] == '\0') break;
        UI_VRAM[offset + i] = petscii_to_screencode(str[i]);
        UI_CRAM[offset + i] = color;
    }
    while (i < width) {
        UI_VRAM[offset + i] = 32; 
        UI_CRAM[offset + i] = color;
        i++;
    }
}

void poke_hud_num(int x, int y, int num, unsigned char color, int width) {
    char buf[8];
    sprintf(buf, "%d", num); 
    poke_hud_str(x, y, buf, color, width);
}

// --- UI LOGIC ---
void hal_init_ui() {
    cbm_k_bsout(142);
    memcpy(UI_VRAM, ui_screen_data, 1000);
    memcpy(UI_CRAM, ui_color_data, 1000);
}

void update_status_bar() {
    unsigned char hp_color = COLOR_LIGHTGREEN;
    int heart_offset = ((UI_HP_Y - 1) * 40) + (UI_HP_X + 2);
    
    if ((player_hp * 4) <= player_max_hp) hp_color = COLOR_LIGHTRED;
    else if ((player_hp * 2) <= player_max_hp) hp_color = COLOR_YELLOW;

    UI_CRAM[heart_offset] = hp_color;

    poke_hud_str(UI_NAME_X, UI_NAME_Y, player_name, COLOR_WHITE, 8);
    poke_hud_num(UI_HP_X,   UI_HP_Y,   player_hp,   hp_color, 3);
    poke_hud_num(UI_GP_X,   UI_GP_Y,   player_gold, COLOR_LIGHTBLUE, 3);
    poke_hud_num(UI_EXP_X,  UI_EXP_Y,  player_exp,  COLOR_YELLOW, 3);
    
	// --- QUEST DISPLAY LOGIC ---
    {
        char q_buf[8];
        int quests_remaining = total_quests - quests_completed;
        if (quests_remaining < 0) quests_remaining = 0; // Sanity check
        
        // Format it like "4/0" 
        sprintf(q_buf, "%02d/%02d", quests_remaining, quests_completed);
        
        // Poke it to the screen (Width 3 should be enough for "X/Y")
        poke_hud_str(UI_Q_X, UI_Q_Y, q_buf, COLOR_WHITE, 5);
    }
	
    if (equipped_weapon != -1) {
        char w1[5] = "", w2[7] = "";
        char *space = strchr(items[equipped_weapon].name, ' ');
        if (space != NULL) {
            strncpy(w1, items[equipped_weapon].name, space - items[equipped_weapon].name);
            strncpy(w2, space + 1, 6);
        } else {
            strncpy(w2, items[equipped_weapon].name, 6);
        }
        poke_hud_str(UI_EQ1_X, UI_EQ1_Y, w1, COLOR_WHITE, UI_EQ1_SZ);
		poke_hud_str(UI_EQ_X,     UI_EQ_Y,     w2, COLOR_WHITE, UI_EQ_SZ); 
    } else {
        poke_hud_str(UI_EQ1_X, UI_EQ1_Y, "    ", COLOR_GRAY2, UI_EQ1_SZ);
        poke_hud_str(UI_EQ_X,     UI_EQ_Y,     "none ", COLOR_GRAY2, UI_EQ_SZ);
    }
}

void update_banner(const char* text) {
    poke_hud_str(MIN_X, 5, text, COLOR_YELLOW, 37);
}

// --- PRINTING LOGIC ---
void clear_lower_screen() {
    int i;
    for(i = MIN_Y; i <= MAX_Y; i++) {
        gotoxy(MIN_X, i); 
        cprintf("%37s", ""); 
    }
    text_x = MIN_X; text_y = MIN_Y;
}

void check_page_break() {
    if (text_y >= MAX_Y) {
        gotoxy(MIN_X, MAX_Y); textcolor(COLOR_YELLOW);
        cputs("--- press enter ---");
        while(1) {
            char c = cgetc();
            if (c == '\n' || c == '\r') break;
        }
        clear_lower_screen();
        textcolor(COLOR_WHITE);
    }
}

void win_newline() {
    text_y++; text_x = MIN_X;
    check_page_break();
}

void win_print(const char *text) {
    int i = 0, w_len;
    char word[40];
    if (text == NULL || text[0] == '\0') return;
    while (text[i] != '\0') {
        while (text[i] == ' ') {
            if (text_x < MAX_X) { gotoxy(text_x, text_y); cputc(' '); text_x++; }
            i++;
        }
        if (text[i] == '\0') break;
        w_len = 0;
        while (text[i] != ' ' && text[i] != '\0' && text[i] != '\n' && text[i] != '\r' && w_len < 37) {
            word[w_len++] = text[i++];
        }
        word[w_len] = '\0';
        if (text_x > MIN_X && text_x + w_len > MAX_X) win_newline();
        gotoxy(text_x, text_y); cputs(word); text_x += w_len;
        if (text[i] == '\n' || text[i] == '\r') { win_newline(); i++; }
    }
}

// --- INPUT LOGIC ---
void get_input(char *buffer, int max_len) {
    int pos = 0; char c;
    cursor(1);
    while (1) {
        c = cgetc();
        if (c == '\n' || c == '\r') { buffer[pos] = '\0'; break; }
        else if (c == 0x14 || c == 0x08) { 
            if (pos > 0) {
                pos--;
                gotoxy(text_x - 1, text_y); cputc(' '); gotoxy(text_x - 1, text_y);
                text_x--;
            }
        }
        else if (pos < max_len - 1 && text_x < MAX_X && c >= 32 && c <= 126) {
            buffer[pos++] = c; cputc(c); text_x++;
        }
    }
    cursor(0);
}

// --- DISK LOGIC ---
void load_room_from_disk(int room_id) {
    int i; FILE *f = fopen("rooms.dat", "rb");
    if (f == NULL) { set_msg("error: rooms.dat missing!"); return; }
    for (i = 0; i < room_id; i++) fread(&dummy_room_data, 244, 1, f);
    fread(&current_room_data, 244, 1, f);
    fclose(f);
    current_room_data.name[31] = '\0';
    current_room_data.description[199] = '\0';
    current_room_id = room_id;
}

void boot_game() {
    int i; 
    unsigned char buf[168]; // <--- INCREASED TO 168 BYTES
    
    cbm_k_bsout(142); clrscr();
    textcolor(COLOR_LIGHTBLUE); cputs("=== welcome to the adventure ===\r\n\r\n");
    
    boot_file = fopen("boot.dat", "rb");
    if (boot_file == NULL) while(1);
    
    fread(&total_rooms, 2, 1, boot_file);
    fread(&total_items, 2, 1, boot_file);
    fread(&total_actors, 2, 1, boot_file);
    fread(intro_text, 512, 1, boot_file);
    
    total_quests = 1; // <--- START AT 1 FOR THE "DROP GEM IN FOYER" WIN CONDITION
    
    for (i = 0; i < total_items; i++) {
        fread(buf, 168, 1, boot_file); // <--- READ 168 BYTES
        memcpy(items[i].name, buf, 32); 
        memcpy(items[i].description, buf+32, 128);
        items[i].location = buf[160] | (buf[161] << 8);
        items[i].type = buf[162] | (buf[163] << 8); 
        items[i].stat = buf[164] | (buf[165] << 8);
        items[i].is_quest = buf[166] | (buf[167] << 8); // <--- GRAB QUEST FLAG
        
        if (items[i].is_quest == 1) total_quests++; // <--- TALLY QUEST
    }
    
    for (i = 0; i < total_actors; i++) {
        fread(buf, 168, 1, boot_file); // <--- READ 168 BYTES
        memcpy(actors[i].name, buf, 32); 
        memcpy(actors[i].description, buf+32, 128);
        actors[i].location = buf[160] | (buf[161] << 8);
        actors[i].hp = buf[162] | (buf[163] << 8); 
        actors[i].damage = buf[164] | (buf[165] << 8);
        actors[i].is_quest = buf[166] | (buf[167] << 8); // <--- GRAB QUEST FLAG
        
        if (actors[i].is_quest == 1) total_quests++; // <--- TALLY QUEST
    }
    fclose(boot_file);
    
    load_room_from_disk(0);
    
    textcolor(COLOR_WHITE); cputs(intro_text); cputs("\r\n\r\n");
    textcolor(COLOR_YELLOW); cputs("what is your name, hero?\r\n> "); textcolor(COLOR_WHITE);
    {
        int p = 0; char c; cursor(1);
        while(1) {
            c = cgetc();
            if(c == '\n' || c == '\r') { player_name[p] = '\0'; break; }
            if((c == 0x14 || c == 0x08) && p > 0) {
                p--; cputc(0x14); cputc(' '); cputc(0x14);
            } else if (p < 8 && c >= 32 && c <= 126) {
                player_name[p++] = c; cputc(c);
            }
        }
        cursor(0);
    }
    srand(clock()); 
}

// --- GAME ACTIONS ---
void look() {
    int i, found_item = 0;
    update_banner(current_room_data.name);
    textcolor(COLOR_WHITE); win_print(current_room_data.description); win_newline();
    for (i = 0; i < total_items; i++) {
        if (items[i].location == current_room_id) {
            if (!found_item) { win_newline(); win_print("you see here:\n"); found_item = 1; }
            win_print("- "); win_print(items[i].name); win_print("\n");
        }
    }
    for (i = 0; i < total_actors; i++) {
        if (actors[i].location == current_room_id) {
            win_newline();
            if (actors[i].hp > 0) { textcolor(COLOR_LIGHTRED); win_print(actors[i].name); win_print(" is here.\n"); } 
            else { textcolor(COLOR_GRAY2); win_print("the body of a "); win_print(actors[i].name); win_print(" lies here.\n"); }
            textcolor(COLOR_WHITE);
        }
    }
}

void go(const char *direction) {
    int next_room = NO_EXIT;
    if (is_match(direction, "north") || is_match(direction, "n")) next_room = current_room_data.n;
    else if (is_match(direction, "south") || is_match(direction, "s")) next_room = current_room_data.s;
    else if (is_match(direction, "east") || is_match(direction, "e")) next_room = current_room_data.e;
    else if (is_match(direction, "west") || is_match(direction, "w")) next_room = current_room_data.w;
    else if (is_match(direction, "up") || is_match(direction, "u")) next_room = current_room_data.u;
    else if (is_match(direction, "down") || is_match(direction, "d")) next_room = current_room_data.d;
    if (next_room != NO_EXIT) { current_room_id = next_room; load_room_from_disk(next_room); } 
    else { set_msg("you can't go that way."); }
}

void take(const char *noun) {
    int i;
    for (i = 0; i < total_items; i++) {
        if (match_item(items[i].name, noun) && items[i].location == current_room_id) {
            items[i].location = INVENTORY;
            strcpy(message_buffer, "you pick up the "); strcat(message_buffer, items[i].name); strcat(message_buffer, ".");
            if (items[i].type == TYPE_TREASURE) player_gold += items[i].stat;
			if (items[i].is_quest == 1) {
                quests_completed++;
                items[i].is_quest = 0; // Set to 0 so they can't drop/take it repeatedly to cheat
            }
            return;
        }
    }
    set_msg("i don't see that here.");
}

void drop(const char *noun) {
    int i;
    for (i = 0; i < total_items; i++) {
        if (match_item(items[i].name, noun) && items[i].location == INVENTORY) {
            if (match_item(items[i].name, "gem") && current_room_id == 0) {
                clear_lower_screen(); textcolor(COLOR_LIGHTGREEN);
                win_print("\n=== victory ===\n\nyou place the ruby gem on the pedestal.\nyou escaped!\n");
                playing = 0; return;
            }
            items[i].location = current_room_id;
            if (equipped_weapon == i) equipped_weapon = -1;
            if (items[i].type == TYPE_TREASURE) player_gold -= items[i].stat;
            strcpy(message_buffer, "you drop the "); strcat(message_buffer, items[i].name); strcat(message_buffer, ".");
            return;
        }
    }
    set_msg("you don't have that to drop.");
}

void equip(const char *noun) {
    int i;
    for (i = 0; i < total_items; i++) {
        if (match_item(items[i].name, noun) && items[i].location == INVENTORY) {
            if (items[i].type == TYPE_WEAPON) {
                equipped_weapon = i;
                strcpy(message_buffer, "you heft the "); strcat(message_buffer, items[i].name); strcat(message_buffer, ".");
            } else { set_msg("you can't equip that!"); }
            return;
        }
    }
    set_msg("you don't have that in your inventory.");
}

void use_item(const char *noun) {
    int i;
    for (i = 0; i < total_items; i++) {
        if (match_item(items[i].name, noun) && items[i].location == INVENTORY) {
            if (items[i].type == TYPE_HEAL) {
                player_hp += items[i].stat;
                if (player_hp > player_max_hp) player_hp = player_max_hp;
                strcpy(message_buffer, "you consume the "); strcat(message_buffer, items[i].name); strcat(message_buffer, ".");
                items[i].location = DESTROYED; 
            } else { set_msg("you can't use that right now."); }
            return;
        }
    }
    set_msg("you don't have that in your inventory.");
}

void attack(const char *noun) {
    int i, p_roll, m_roll, dmg_dealt; char temp[32]; message_buffer[0] = '\0'; 
    for (i = 0; i < total_actors; i++) {
        if (match_item(actors[i].name, noun) && actors[i].location == current_room_id) {
            if (actors[i].hp <= 0) { set_msg("it is already dead!"); return; }
            p_roll = roll_d20(); strcpy(temp, "you roll a "); append_num(temp, p_roll); strcat(temp, ". "); strcat(message_buffer, temp);
            if (p_roll >= 5) { 
                dmg_dealt = (equipped_weapon != -1) ? items[equipped_weapon].stat : 1;
                strcpy(temp, "you hit it for "); append_num(temp, dmg_dealt); strcat(temp, " damage!\n"); strcat(message_buffer, temp);
                actors[i].hp -= dmg_dealt; player_exp += 5; 
            } else { strcat(message_buffer, "you miss!\n"); }
            if (actors[i].hp <= 0) { strcat(message_buffer, "you killed it!");
			player_exp += 20;
			if (actors[i].is_quest == 1) {
                    quests_completed++;
                    actors[i].is_quest = 0; // Ensure it only counts once
                }
			return;
			}
            m_roll = roll_d20(); strcpy(temp, "it rolls a "); append_num(temp, m_roll); strcat(temp, ". "); strcat(message_buffer, temp);
            if (m_roll >= 8) { 
                dmg_dealt = actors[i].damage;
                strcpy(temp, "it hits you for "); append_num(temp, dmg_dealt); strcat(temp, " damage!"); strcat(message_buffer, temp);
                player_hp -= dmg_dealt;
            } else { strcat(message_buffer, "it misses!"); }
            if (player_hp <= 0) { clear_lower_screen(); textcolor(COLOR_LIGHTRED); win_print("\nyou have been slain.\n"); playing = 0; }
            return;
        }
    }
    set_msg("you don't see them here.");
}

// --- ENGINE ---
void show_action_message() {
    if (message_buffer[0] != '\0') {
        clear_lower_screen(); update_status_bar(); update_banner("action");
        text_x = MIN_X +2;
		text_y =  12;
		win_print(message_buffer);
		//gotoxy(MIN_X + 2, 12); textcolor(COLOR_LIGHTGREEN); cputs(message_buffer);
        delay_seconds(1); message_buffer[0] = '\0'; clear_lower_screen(); 
    }
}

void process_input() {
    char input[INPUT_LEN]; 
	char *verb, *noun; 
	int action_taken = 0;
	int i, found;
    get_input(input, sizeof(input));
	
	//Repeat intercept logic
	if (is_match(input, "r") || is_match(input, "R")) {
        if (last_command[0] != '\0') {
            // Overwrite the 'r' with the saved command
            strcpy(input, last_command); 
        } else {
            set_msg("nothing to repeat.");
            show_action_message();
            return;
        }
    } else if (input[0] != '\0') {
        // If they typed a normal command, save it for next time
        strcpy(last_command, input); 
    }
	
    verb = strtok(input, " \n\r"); noun = strtok(NULL, " \n\r");
    if (verb == NULL) return; 
    if (is_match(verb, "look") || is_match(verb, "l")) { /* Auto */ } 
    else if (is_match(verb, "go") || is_match(verb, "n") || is_match(verb, "s") || is_match(verb, "e") || is_match(verb, "w") || is_match(verb, "u") || is_match(verb, "d")) {
        if (is_match(verb, "go")) { if (noun != NULL) go(noun); else set_msg("go where?"); } else go(verb);
        action_taken = 1;
    }
    else if (is_match(verb, "take") || is_match(verb, "t" )|| is_match(verb, "get")) { if (noun != NULL) take(noun); else set_msg("take what?"); action_taken = 1; }
    else if (is_match(verb, "drop") || is_match(verb, "d")) { if (noun != NULL) drop(noun); else set_msg("drop what?"); action_taken = 1; }
    else if (is_match(verb, "equip") || is_match(verb, "eq")) { if (noun != NULL) equip(noun); else set_msg("equip what?"); action_taken = 1; }
    else if (is_match(verb, "attack") || is_match(verb, "hit") || is_match(verb, "a")) { if (noun != NULL) attack(noun); else set_msg("attack who?"); action_taken = 1; }
    else if (is_match(verb, "inventory") || is_match(verb, "i")) {
        found = 0; message_buffer[0] = '\0';
        strcat(message_buffer, "you are carrying:\n");
        for (i = 0; i < total_items; i++) {
            if (items[i].location == INVENTORY) {
                strcat(message_buffer, "- "); strcat(message_buffer, items[i].name); 
                if (equipped_weapon == i) strcat(message_buffer, " [eq]");
                strcat(message_buffer, "\n"); found = 1;
            }
        }
        if (!found) set_msg("you are carrying nothing.");
        action_taken = 1;
    }
    else if (is_match(verb, "use") || is_match(verb, "drink") || is_match(verb, "eat")) {
        if (noun != NULL) use_item(noun); else set_msg("use what?");
        action_taken = 1;
    }
	else if (is_match(verb, "help") || is_match(verb, "h")) {
        set_msg("commands:\nn, s, e, w, u, d\ntake, drop, equip\nuse, attack, i, l, quit");
        action_taken = 1;
    }
	else if (is_match(verb, "quit")) playing = 0;
    else { set_msg("i don't understand that."); action_taken = 1; }
    if (action_taken) { show_action_message(); }
}

int main() {
    bordercolor(COLOR_BLACK); bgcolor(COLOR_BLACK); 
    boot_game();
    hal_init_ui(); 
    while (playing) { 
        clear_lower_screen(); update_status_bar(); look(); 
        win_newline(); textcolor(COLOR_YELLOW); win_print("> "); textcolor(COLOR_WHITE);
        process_input(); 
    }
    return EXIT_SUCCESS;
}