.RECIPEPREFIX := $() $()

CC = gcc -s -O3 -std=gnu2x -Wl,--exclude-all-symbols

all: lish.exe loop.exe

lish.exe: lish.c docmd.c
    $(CC) -o lish.exe lish.c docmd.c -l:lua51.dll

loop.exe: loop.c
    $(CC) -o loop.exe loop.c

clean:
    rm -f *.exe *.dll