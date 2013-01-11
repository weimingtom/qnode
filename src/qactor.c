/*
 * See Copyright Notice in qnode.h
 */

#include "qactor.h"
#include "qassert.h"
#include "qluacapi.h"
#include "qluautil.h"
#include "qidmap.h"
#include "qmailbox.h"
#include "qmalloc.h"
#include "qmsg.h"
#include "qserver.h"
#include "qthread.h"

qid_t qactor_new_id() {
  qmutex_lock(&g_server->id_map_mutex);
  qid_t id = qid_new(&g_server->id_map);
  qmutex_unlock(&g_server->id_map_mutex);
  return id;
}

qactor_t *qactor_new(qid_t aid) {
  qactor_t *actor = qalloc_type(qactor_t);
  if (actor == NULL) {
    return NULL;
  }
  actor->state = NULL;
  qlist_entry_init(&(actor->entry));
  actor->aid = aid;
  actor->parent = QID_INVALID;
  qserver_new_actor(actor);
  return actor;
}

void qactor_destroy(qactor_t *actor) {
  qassert(actor);
  lua_close(actor->state);
  qfree(actor);
}

void qactor_attach(qactor_t *actor, lua_State *state) {
  qassert(actor->state == NULL);
  qluac_register(state, actor);
  actor->state = state;
}

qid_t qactor_spawn(qactor_t *actor, lua_State *state) {
  qassert(actor);
  qtid_t receiver_id = qserver_worker_thread();
  qmsg_t *msg = qmsg_new(actor->tid, receiver_id);
  if (msg == NULL) {
    return QID_INVALID;
  }
  qid_t aid = qactor_new_id();
  if (aid == QID_INVALID) {
    qfree(msg);
    return -1;
  }
  qid_t parent = actor->aid;
  qmsg_init_spawn(msg, aid, parent, state);
  qactor_t *new_actor = qactor_new(aid);
  qactor_attach(new_actor, state);
  new_actor->parent = actor->aid;
  msg->args.spawn.actor = new_actor;
  qmsg_send(msg);
  return aid;
}
