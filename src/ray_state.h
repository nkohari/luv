#ifndef _RAY_STATE_H_
#define _RAY_STATE_H_

#include "ray_common.h"
#include "ray_hash.h"
#include "ray_list.h"

#define RAY_EVENT_LOOP "ray:event:loop"
#define RAY_STATE_MAIN "ray:state:main"

/* state flags */
#define RAY_START  (1 << 0) /* initial state */
#define RAY_ACTIVE (1 << 1) /* currently running */
#define RAY_CLOSED (1 << 2) /* not among the living */

#define ray_is_start(S)  ((S)->flags & RAY_START)
#define ray_is_active(S) ((S)->flags & RAY_ACTIVE)
#define ray_is_closed(S) ((S)->flags & RAY_CLOSED)

/* ray states */
typedef struct ray_actor_s ray_actor_t;

typedef union ray_data_u {
  void*       data;
  ray_hash_t* hash;
  ray_list_t* list;
} ray_data_t;

typedef uv_buf_t ray_buf_t;

typedef struct ray_vtable_s {
  int (*await)(ray_actor_t* self, ray_actor_t* that);
  int (*rouse)(ray_actor_t* self, ray_actor_t* from);
  int (*close)(ray_actor_t* self);
} ray_vtable_t;

struct ray_actor_s {
  ray_vtable_t  v;
  ray_handle_t  h;
  ray_req_t     r;
  lua_State*    L;
  int           flags;
  ngx_queue_t   queue;
  ngx_queue_t   cond;
  ray_buf_t     buf;
  int           ref;
  ray_data_t    u;
};

int rayM_main_await(ray_actor_t* self, ray_actor_t* that);
int rayM_main_rouse(ray_actor_t* self, ray_actor_t* from);

int ray_init_main(lua_State* L);

uv_loop_t*   ray_get_loop(lua_State* L);
ray_actor_t* ray_get_self(lua_State* L);
ray_actor_t* ray_get_main(lua_State* L);


ray_actor_t* rayL_actor_new(lua_State* L, const char* m, const ray_vtable_t* v);

int ray_await(ray_actor_t* self, ray_actor_t* that);
int ray_rouse(ray_actor_t* self, ray_actor_t* from);
int ray_close(ray_actor_t* self);

int ray_xcopy(ray_actor_t* self, ray_actor_t* that, int narg);

int ray_notify(ray_actor_t* self, int narg);

/* default state methods */
int rayM_state_close(ray_actor_t* self);

#endif /* _RAY_STATE_H_ */
