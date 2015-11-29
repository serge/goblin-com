#include <stdbool.h>
#define MAXKEYLEN 10

#define GEN_ACTION(T) \
   T(BUILD,"build", "B") \
   T(HEROES,"heroes", "H") \
   T(SQUADS,"squads", "S") \
   T(STORY,"story", "r") \
   T(HELP,"help", "p") \
   T(YES,"yes", "y") \
   T(NO,"no", "n") \
   T(FARM,"farm", "f") \
   T(STABLE,"stable", "s") \
   T(MINE,"mine","m") \
   T(HAMLET,"hamlet", "h") \
   T(ROAD,"road", "r") \
   T(UP,"up", "up") \
   T(DOWN,"down", "down") \
   T(LEFT,"left", "left") \
   T(RIGHT,"right", "right") \
   T(HIRE,"hire", "enter") \
   T(QUIT, "quit", "q") \
   T(QUITSAVE, "quitsave", "Q") \
   T(SPEEDUP, "speedup", ">") \
   T(REFRESH, "refresh", "R") \
   T(LUMBERYARD, "lumberyard", "w") \
   T(SPEEDDOWN, "speeddown", "<") \
   T(SQUADUP, "squadup", "+") \
   T(SQUADDOWN, "squaddown", "-") \
   T(NONE,"none", "")

#define ACTION_ENUM(name, strname, hotkey) name,
enum action {
   GEN_ACTION(ACTION_ENUM)
};

typedef struct keys {
   enum action keys[1000];
   char actions[NONE][MAXKEYLEN];
} keys_t;

enum action
get_action_by_key(keys_t *keys, int key);

char*
get_key_by_action(keys_t *keys, enum action a);

keys_t* 
parse_key_file(const char* keyfilename);

keys_t*
create_empty_table();

bool
is_exit_key(keys_t* keytable, int key);
