#include "ray.h"

void _exit_cb(uv_process_t* handle, int status, int sigterm) {
  TRACE("EXIT : status %i, sigterm %i\n", status, sigterm);
  ray_object_t* self = container_of(handle, ray_object_t, h);

  if (status == -1) {
    TRACE("ERROR: %s\n", uv_strerror(uv_last_error(self->state->loop)));
  }

  /* TODO: fix these self->state accesses */
  lua_State* L = self->state->L;
  lua_pushinteger(L, status);
  lua_pushinteger(L, sigterm);

  rayL_cond_signal(&self->send);
}

/*
  ray.process.spawn("cat", {
    "foo.txt",
    env    = { HOME = "/home/cnorris" },
    cwd    = "/tmp",
    stdin  = ray.stdout,
    stdout = ray.stdin,
    stderr = ray.stderr,
    detach = true,
  })
*/

static int ray_new_process(lua_State* L) {
  const char* cmd = luaL_checkstring(L, 1);
  size_t argc;
  char** args;
  int i;
  char* cwd;
  char** env;
  uv_process_options_t opts;
  int rv;

  luaL_checktype(L, 2, LUA_TTABLE); /* options */
  memset(&opts, 0, sizeof(uv_process_options_t));

  argc = lua_objlen(L, 2);
  args = (char**)malloc((argc + 1) * sizeof(char*));
  args[0] = (char*)cmd;
  for (i = 1; i <= argc; i++) {
    lua_rawgeti(L, -1, i);
    args[i] = (char*)lua_tostring(L, -1);
    lua_pop(L, 1);
  }

  args[argc + 1] = NULL;

  cwd = NULL;
  lua_getfield(L, 2, "cwd");
  if (!lua_isnil(L, -1)) {
    cwd = (char*)lua_tostring(L, -1);
  }
  lua_pop(L, 1);

  env = NULL;
  lua_getfield(L, 2, "env");
  if (!lua_isnil(L, -1)) {
    int i, len;
    const char* key;
    const char* val;
    len = 32;
    env = (char**)malloc(32 * sizeof(char*));

    lua_pushnil(L);
    i = 0;
    while (lua_next(L, -2) != 0) {
      if (i >= len) {
        len *= 2;
        env = (char**)realloc(env, len * sizeof(char*));
      }
      key = lua_tostring(L, -2);
      val = lua_tostring(L, -1);
      lua_pushfstring(L, "%s=%s", key, val);
      env[i++] = (char*)lua_tostring(L, -1);
      lua_pop(L, 2);
    }

    env[i] = NULL;
  }
  lua_pop(L, 1);

  opts.exit_cb = _exit_cb;
  opts.file    = cmd;
  opts.args    = args;
  opts.env     = env;
  opts.cwd     = cwd;

  uv_stdio_container_t stdio[3];
  opts.stdio_count = 3;

  const char* stdfh_names[] = { "stdin", "stdout", "stderr" };
  for (i = 0; i < 3; i++) {
    lua_getfield(L, 2, stdfh_names[i]);
    if (lua_isnil(L, -1)) {
      stdio[i].flags = UV_IGNORE;
    }
    else {
      ray_object_t* obj = (ray_object_t*)luaL_checkudata(L, -1, RAY_PIPE_T);
      stdio[i].flags = UV_INHERIT_STREAM;
      stdio[i].data.stream = &obj->h.stream;
    }
    lua_pop(L, 1);
  }

  opts.stdio = stdio;

  lua_getfield(L, 2, "detach");
  if (lua_toboolean(L, -1)) {
    opts.flags |= UV_PROCESS_DETACHED;
  }
  else {
    opts.flags = 0;
  }
  lua_pop(L, 1);

  ray_state_t*  curr = rayL_state_self(L);

  ray_object_t* self = (ray_object_t*)lua_newuserdata(L, sizeof(ray_object_t));
  luaL_getmetatable(L, RAY_PROCESS_T);
  lua_setmetatable(L, -2);

  rayL_object_init(curr, self);

  lua_insert(L, 1);
  lua_settop(L, 1);

  rv = uv_spawn(rayL_event_loop(L), &self->h.process, opts);

  free(args);
  if (env) free(env);
  if (rv) {
    uv_err_t err = uv_last_error(rayL_event_loop(L));
    return luaL_error(L, uv_strerror(err));
  }

  if (opts.flags & UV_PROCESS_DETACHED) {
    uv_unref((uv_handle_t*)&self->h.process);
    return 1;
  }
  else {
    return rayL_cond_wait(&self->send, curr);
  }
}

static int ray_process_kill(lua_State* L) {
  ray_object_t* self = (ray_object_t*)luaL_checkudata(L, 1, RAY_PROCESS_T);
  int signum = luaL_checkint(L, 2);

  if (uv_process_kill(&self->h.process, signum)) {
    uv_err_t err = uv_last_error(rayL_event_loop(L));
    return luaL_error(L, uv_strerror(err));
  }

  return 0;
}

static int ray_process_free(lua_State* L) {
  ray_object_t* self = (ray_object_t*)lua_touserdata(L, 1);
  rayL_object_close(self);
  return 0;
}
static int ray_process_tostring(lua_State* L) {
  ray_object_t *self = (ray_object_t*)luaL_checkudata(L, 1, RAY_PROCESS_T);
  lua_pushfstring(L, "userdata<%s>: %p", RAY_PROCESS_T, self);
  return 1;
}

luaL_Reg ray_process_funcs[] = {
  {"spawn",       ray_new_process},
  {NULL,          NULL}
};

luaL_Reg ray_process_meths[] = {
  {"kill",        ray_process_kill},
  {"__gc",        ray_process_free},
  {"__tostring",  ray_process_tostring},
  {NULL,          NULL}
};
