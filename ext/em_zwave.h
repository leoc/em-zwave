#ifndef EM_ZWAVE_H
#define EM_ZWAVE_H

typedef struct notification_t notification_t;
struct notification_t {
    int number;
    notification_t* next;
};

typedef struct waiting_notification_t waiting_notification_t;
struct waiting_notification_t {
    notification_t* notification;
    bool abort;
};

static ID id_push_notification;
static VALUE rb_mEm;
static VALUE rb_cZwave;
static VALUE rb_cNotification;

static pthread_cond_t  g_notification_cond  = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t g_notification_mutex = PTHREAD_MUTEX_INITIALIZER;
static notification_t* g_notification_queue = NULL;

#endif
