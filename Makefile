CFLAGS = -std=c99 -Wall -Wextra -g3 -O3
LDLIBS = -lm

sources := main.c display.c map.c game.c rand.c device_unix.c keys.c
headers := display.h map.h game.h rand.h device.h keys.h
texts   := story.txt help.txt game-over.txt halfway.txt win.txt apology.txt

gcom : text.o $(addprefix src/,$(sources))
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

text.o : $(addprefix doc/,$(texts))
	$(LD) -r -b binary -o $@ $^

clean :
	$(RM) persist.gcom gcom gcom.exe text.o
