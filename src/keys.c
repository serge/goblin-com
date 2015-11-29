#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "keys.h"
#include "device.h"
#include "rand.h"

bool
is_exit_key(keys_t* keytable, int key)
{
    enum action a = get_action_by_key(keytable, key);
    return a == QUIT || a == QUITSAVE || key == 27;
}

static int
parse_hotkey(char* key, char hotkey[MAXKEYLEN])
{
   strncpy(hotkey, key, MAXKEYLEN - 1);
   if (strcmp(key, "up") == 0)
      return ARROW_U;
   if (strcmp(key, "down") == 0)
      return ARROW_D;
   if (strcmp(key, "right") == 0)
      return ARROW_R;
   if (strcmp(key, "left") == 0)
      return ARROW_L;
   if (strcmp(key, "enter") == 0)
      return 13;
   hotkey[0] = key[0];
   hotkey[1] = 0;
   return key[0];
}

void
add_key(enum action a, char* key, keys_t* table)
{
   int tkey = parse_hotkey(key, table->actions[a]);
   table->keys[tkey] = a;
}

void
recognise_action(char* action, char* key, keys_t* table)
{
#define ACTION_PARSE(name, strname, hotkey) \
      if(strcmp(strname, action) == 0) {\
          add_key(name, key, table); \
      }
      GEN_ACTION(ACTION_PARSE);
}

keys_t*
create_empty_table()
{
   keys_t* table;
   size_t i;

   table = (keys_t*)malloc(sizeof(keys_t));
   for(i = 0; i < countof(table->keys); i++)
      table->keys[i] = NONE;
#define ACTION_CREATE(name, strname, hotkey) \
      add_key(name, hotkey, table);
   GEN_ACTION(ACTION_CREATE);

   return table;
}

void
write_key_file(const char* keyfile)
{
   FILE* keyfp = fopen(keyfile, "w");
#define WRITE_ACTION(name, strname, hotkey) \
      if(name != NONE) \
         fprintf(keyfp, "%s:%s\n", strname, hotkey);
   GEN_ACTION(WRITE_ACTION);
   fclose(keyfp);
}

keys_t* 
parse_key_file(const char* keyfile)
{
   char action[20], key[10] = {0};
   char* rcv;
   keys_t* table;
   FILE* keyfp;

   table = create_empty_table();
   keyfp = fopen(keyfile, "r"); 
   if (keyfp == NULL) {
       write_key_file(keyfile);
       return table;
   }
   rcv = action;
   while(1) {
      int ch = fgetc(keyfp);
      if (ch == EOF)
          break;
      switch(ch) {
         case ':':
            *rcv = 0;
            rcv = key;
            break;
         case '\n':
         case '\r':
            *rcv = 0;
            rcv = action;
            if (key[0] != 0)
               recognise_action(action, key, table);
            action[0] = 0;
            key[0] = 0;
            break;
         default:
            *rcv++ = ch;
            break;
      }
   }

   fclose(keyfp);
   return table;
}

enum action
get_action_by_key(keys_t *keys, int key) 
{
   return keys->keys[key];
}

char*
get_key_by_action(keys_t *keys, enum action a)
{
   return keys->actions[a];
}

