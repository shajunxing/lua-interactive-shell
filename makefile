.RECIPEPREFIX := $() $()

CC = gcc -s -O3 -std=gnu2x -Wl,--exclude-all-symbols

all: lsh.exe loop.exe

lsh.exe: lshmain.c lshcmd.c
    $(CC) -o lsh.exe lshmain.c lshcmd.c -l:lua51.dll

loop.exe: loop.c
    $(CC) -o loop.exe loop.c

clean:
    rm -f *.exe *.dll