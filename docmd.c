/*
Copyright 2024 ShaJunXing <shajunxing@hotmail.com>

This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with this program. If not, see <https://www.gnu.org/licenses/>.
*/

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define lua_c
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

int docmd(lua_State *L) {
    const char *cmdline = lua_tostring(L, -1);
    if (strncmp(cmdline, "exit", 4) == 0) {
        exit(0);
    }
    system(cmdline);
    // 参见dotty，"if (status == 0 && lua_gettop(L) > 0)"，要清掉作为输入的字符串，否则每次回车会多一行
    lua_remove(L, 1);
    return 0;
}
