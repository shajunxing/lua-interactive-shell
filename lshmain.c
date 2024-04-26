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

/// 调试：打印当前位置
#define printloc() printf("%s,%d %s\n", __FILE__, __LINE__, __func__);

static lua_State *globalL = NULL;

static const char *progname = LUA_PROGNAME;

static void lstop(lua_State *L, lua_Debug *ar) {
    (void)ar; /* unused arg. */
    lua_sethook(L, NULL, 0, 0);
    luaL_error(L, "interrupted!");
}

static void laction(int i) {
    /// 阻止ctrl-c
    signal(i, SIG_IGN); /* if another SIGINT happens before lstop,
                                terminate process (default action) */
    lua_sethook(globalL, lstop, LUA_MASKCALL | LUA_MASKRET | LUA_MASKCOUNT, 1);
}

static void print_usage(void) {
    fprintf(
        stderr,
        /// 去掉LUA_QL，因为clang-format会ugly indent
        "usage: %s [options] [script [args]].\n"
        "Available options are:\n"
        "  -e stat  execute string 'stat'\n"
        "  -l name  require library 'name'\n"
        "  -i       enter interactive mode after executing 'script'\n"
        "  -v       show version information\n"
        "  --       stop handling options\n"
        "  -        execute stdin and stop handling options\n",
        progname);
    fflush(stderr);
}

static void l_message(const char *pname, const char *msg) {
    if (pname)
        fprintf(stderr, "%s: ", pname);
    fprintf(stderr, "%s\n", msg);
    fflush(stderr);
}

static int report(lua_State *L, int status) {
    if (status && !lua_isnil(L, -1)) {
        const char *msg = lua_tostring(L, -1);
        if (msg == NULL)
            msg = "(error object is not a string)";
        l_message(progname, msg);
        lua_pop(L, 1);
    }
    return status;
}

static int traceback(lua_State *L) {
    if (!lua_isstring(L, 1)) /* 'message' not a string? */
        return 1; /* keep it intact */
    lua_getfield(L, LUA_GLOBALSINDEX, "debug");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return 1;
    }
    lua_getfield(L, -1, "traceback");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        return 1;
    }
    lua_pushvalue(L, 1); /* pass error message */
    lua_pushinteger(L, 2); /* skip this function and traceback */
    lua_call(L, 2, 1); /* call debug.traceback */
    return 1;
}

static int docall(lua_State *L, int narg, int clear) {
    int status;
    int base = lua_gettop(L) - narg; /* function index */
    lua_pushcfunction(L, traceback); /* push traceback function */
    lua_insert(L, base); /* put it under chunk and args */
    signal(SIGINT, laction);
    status = lua_pcall(L, narg, (clear ? 0 : LUA_MULTRET), base);
    /// 阻止ctrl-c
    signal(SIGINT, SIG_IGN);
    lua_remove(L, base); /* remove traceback function */
    /* force a complete garbage collection in case of errors */
    if (status != 0)
        lua_gc(L, LUA_GCCOLLECT, 0);
    return status;
}

/// 增加信息
static void print_version(void) {
    l_message(NULL, "Lua Interactive Shell (Powered By " LUA_RELEASE ")");
}

static int getargs(lua_State *L, char **argv, int n) {
    int narg;
    int i;
    int argc = 0;
    while (argv[argc])
        argc++; /* count total number of arguments */
    narg = argc - (n + 1); /* number of arguments to the script */
    luaL_checkstack(L, narg + 3, "too many arguments to script");
    for (i = n + 1; i < argc; i++)
        lua_pushstring(L, argv[i]);
    lua_createtable(L, narg, n + 1);
    for (i = 0; i < argc; i++) {
        lua_pushstring(L, argv[i]);
        lua_rawseti(L, -2, i - n);
    }
    return narg;
}

static int dofile(lua_State *L, const char *name) {
    int status = luaL_loadfile(L, name) || docall(L, 0, 1);
    return report(L, status);
}

static int dostring(lua_State *L, const char *s, const char *name) {
    int status = luaL_loadbuffer(L, s, strlen(s), name) || docall(L, 0, 1);
    return report(L, status);
}

static int dolibrary(lua_State *L, const char *name) {
    lua_getglobal(L, "require");
    lua_pushstring(L, name);
    return report(L, docall(L, 1, 1));
}

/// 支持自定义提示符，_PROMPT全局变量扩充为类似bash的转义序列
static const char *get_prompt(lua_State *L, int firstline) {
    lua_getfield(L, LUA_GLOBALSINDEX, "string");
    lua_getfield(L, -1, "gsub");
    lua_getfield(L, LUA_GLOBALSINDEX, firstline ? "_PROMPT" : "_PROMPT2");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_pushstring(L, firstline ? LUA_PROMPT : LUA_PROMPT2);
    }
    lua_pushstring(L, "\\(%w)");
    // gsub的第三个参数是替换表
    lua_newtable(L);
    lua_pushstring(L, "e"); // 转义符
    lua_pushstring(L, "\e"); // 注意这里是C语言的转义不是Lua的
    lua_rawset(L, -3);
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof cwd)) {
        lua_pushstring(L, "w");
        lua_pushstring(L, cwd);
        lua_rawset(L, -3);
    }
    lua_call(L, 3, 2);
    const char *p = lua_tostring(L, -2);
    // 清理堆栈
    lua_pop(L, 3);
    return p;
}

static int incomplete(lua_State *L, int status) {
    if (status == LUA_ERRSYNTAX) {
        size_t lmsg;
        const char *msg = lua_tolstring(L, -1, &lmsg);
        const char *tp = msg + lmsg - (sizeof(LUA_QL("<eof>")) - 1);
        if (strstr(msg, LUA_QL("<eof>")) == tp) {
            lua_pop(L, 1);
            return 1;
        }
    }
    return 0; /* else... */
}

/// 此处patch，如果第一行第一个字符是`，那么依照以前的处理，否则执行命令
static int pushline(lua_State *L, int firstline) {
    char buffer[LUA_MAXINPUT];
    char *b = buffer;
    size_t l;
    const char *prmt = get_prompt(L, firstline);
    if (lua_readline(L, b, prmt) == 0)
        return 0; /* no input */
    l = strlen(b);
    if (l > 0 && b[l - 1] == '\n') /* line ends with newline? */
        b[l - 1] = '\0'; /* remove it */
    /// 执行命令
    if (firstline && b[0] != '`') {
        // printloc();
        lua_pushstring(L, b);
        lua_freeline(L, b);
        return 2; // 返回值2表示是命令
    }
    /// 执行Lua指令
    if (firstline) {
        if (l > 1 && b[1] == '=') {
            // printloc();
            lua_pushfstring(L, "return %s", b + 2);
        } else {
            // printloc();
            lua_pushstring(L, b + 1);
        }
    } else {
        // printloc();
        lua_pushstring(L, b);
    }
    lua_freeline(L, b);
    return 1;
}

static int loadline(lua_State *L) {
    int status;
    lua_settop(L, 0);
    /// 返回值6表示命令，参见lua.h,42，6不与其它错误码冲突
    status = pushline(L, 1);
    if (status == 2) {
        // printloc();
        return 6;
    }
    if (status == 0) {
        return -1; /* no input */
    }
    for (;;) { /* repeat until gets a complete line */
        status = luaL_loadbuffer(L, lua_tostring(L, 1), lua_strlen(L, 1), "=stdin");
        if (!incomplete(L, status))
            break; /* cannot try to add lines? */
        if (!pushline(L, 0)) /* no more input? */
            return -1;
        lua_pushliteral(L, "\n"); /* add a new line... */
        lua_insert(L, -2); /* ...between the two lines */
        lua_concat(L, 3); /* join them */
    }
    lua_saveline(L, 1);
    lua_remove(L, 1); /* remove line */
    return status;
}

/// 在docommand.c里面
extern int docmd(lua_State *L);

static void dotty(lua_State *L) {
    int status;
    const char *oldprogname = progname;
    progname = NULL;
    /// ctrl-c/ctrl-z会loadline返回-1，此处阻止
    for (;;) {
        status = loadline(L);
        if (status == -1) {
            printf("\n");
            continue;
        }
        /// 返回值6表示命令
        if (status == 6) {
            // printloc();
            status = docmd(L);
        } else if (status == 0) {
            // printloc();
            status = docall(L, 0, 0);
        }
        report(L, status);
        if (status == 0 && lua_gettop(L) > 0) { /* any result to print? */
            lua_getglobal(L, "print");
            lua_insert(L, 1);
            if (lua_pcall(L, lua_gettop(L) - 1, 0, 0) != 0)
                l_message(progname, lua_pushfstring(L,
                                                    "error calling " LUA_QL("print") " (%s)",
                                                    lua_tostring(L, -1)));
        }
    }
    lua_settop(L, 0); /* clear stack */
    fputs("\n", stdout);
    fflush(stdout);
    progname = oldprogname;
}

static int handle_script(lua_State *L, char **argv, int n) {
    int status;
    const char *fname;
    int narg = getargs(L, argv, n); /* collect arguments */
    lua_setglobal(L, "arg");
    fname = argv[n];
    if (strcmp(fname, "-") == 0 && strcmp(argv[n - 1], "--") != 0)
        fname = NULL; /* stdin */
    status = luaL_loadfile(L, fname);
    lua_insert(L, -(narg + 1));
    if (status == 0)
        status = docall(L, narg, 0);
    else
        lua_pop(L, narg);
    return report(L, status);
}

/* check that argument has no extra characters at the end */
#define notail(x)           \
    {                       \
        if ((x)[2] != '\0') \
            return -1;      \
    }

static int collectargs(char **argv, int *pi, int *pv, int *pe) {
    int i;
    for (i = 1; argv[i] != NULL; i++) {
        // printf("%d %s\n", i, argv[i]);
        if (argv[i][0] != '-') /* not an option? */
            return i;
        switch (argv[i][1]) { /* option */
        case '-':
            notail(argv[i]);
            return (argv[i + 1] != NULL ? i + 1 : 0);
        case '\0':
            return i;
        case 'i':
            notail(argv[i]);
            *pi = 1;
            break; /// 此处不应该go through，否则-i也会导致-v的效果，即打印版本信息
        case 'v':
            notail(argv[i]);
            *pv = 1;
            break;
        case 'e':
            *pe = 1; /* go through */
        case 'l':
            if (argv[i][2] == '\0') {
                i++;
                if (argv[i] == NULL)
                    return -1;
            }
            break;
        default:
            return -1; /* invalid option */
        }
    }
    return 0;
}

static int runargs(lua_State *L, char **argv, int n) {
    int i;
    for (i = 1; i < n; i++) {
        if (argv[i] == NULL)
            continue;
        lua_assert(argv[i][0] == '-');
        switch (argv[i][1]) { /* option */
        case 'e': {
            const char *chunk = argv[i] + 2;
            if (*chunk == '\0')
                chunk = argv[++i];
            lua_assert(chunk != NULL);
            if (dostring(L, chunk, "=(command line)") != 0)
                return 1;
            break;
        }
        case 'l': {
            const char *filename = argv[i] + 2;
            if (*filename == '\0')
                filename = argv[++i];
            lua_assert(filename != NULL);
            if (dolibrary(L, filename))
                return 1; /* stop if file fails */
            break;
        }
        default:
            break;
        }
    }
    return 0;
}

static int handle_luainit(lua_State *L) {
    const char *init = getenv(LUA_INIT);
    if (init == NULL)
        return 0; /* status OK */
    else if (init[0] == '@')
        return dofile(L, init + 1);
    else
        return dostring(L, init, "=" LUA_INIT);
}

struct Smain {
    int argc;
    char **argv;
    int status;
};

static int pmain(lua_State *L) {
    struct Smain *s = (struct Smain *)lua_touserdata(L, 1);
    char **argv = s->argv;
    int script;
    int has_i = 0, has_v = 0, has_e = 0;
    globalL = L;
    if (argv[0] && argv[0][0])
        progname = argv[0];
    lua_gc(L, LUA_GCSTOP, 0); /* stop collector during initialization */
    luaL_openlibs(L); /* open libraries */
    lua_gc(L, LUA_GCRESTART, 0);
    s->status = handle_luainit(L);
    if (s->status != 0)
        return 0;
    script = collectargs(argv, &has_i, &has_v, &has_e);
    // printf("script:%d has_i:%d has_v:%d has_e:%d\n", script, has_i, has_v, has_e);
    if (script < 0) { /* invalid args? */
        print_usage();
        s->status = 1;
        return 0;
    }
    if (has_v)
        print_version();
    s->status = runargs(L, argv, (script > 0) ? script : s->argc);
    if (s->status != 0)
        return 0;
    if (script)
        s->status = handle_script(L, argv, script);
    if (s->status != 0)
        return 0;
    if (has_i)
        dotty(L);
    else if (script == 0 && !has_e && !has_v) {
        if (lua_stdin_is_tty()) {
            /// print_version();
            dotty(L);
        } else
            dofile(L, NULL); /* executes stdin as a file */
    }
    return 0;
}

int main(int argc, char **argv) {
    int status;
    struct Smain s;
    lua_State *L = lua_open(); /* create state */
    if (L == NULL) {
        l_message(argv[0], "cannot create state: not enough memory");
        return EXIT_FAILURE;
    }
    s.argc = argc;
    s.argv = argv;
    status = lua_cpcall(L, &pmain, &s);
    report(L, status);
    lua_close(L);
    return (status || s.status) ? EXIT_FAILURE : EXIT_SUCCESS;
}
