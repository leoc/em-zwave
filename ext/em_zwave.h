#ifndef EM_ZWAVE_H
#define EM_ZWAVE_H

typedef struct notification_t notification_t;
struct notification_t {
    int type;
    int home_id;
    int node_id;
    int value_id;
    int event;
    int group_index;
    int button_id;
    int scene_id;
    int notification;
    notification_t* next;
};

typedef struct waiting_notification_t waiting_notification_t;
struct waiting_notification_t {
    notification_t* notification;
    bool abort;
};

typedef struct zwave_init_data_t zwave_init_data_t;
struct zwave_init_data_t {
    char* config_path;
    char** devices;
    int devices_length;
};

static ID id_push_notification;
static VALUE rb_mEm;
static VALUE rb_cZwave;
static VALUE rb_cNotification;

static uint32 g_zwave_home_id = 0;
static bool   g_zwave_init_failed = false;
static bool   g_zwave_init_done = false;

/* Those are used to wait until the driver is ready */
static pthread_cond_t  g_zwave_init_cond  = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t g_zwave_init_mutex = PTHREAD_MUTEX_INITIALIZER;
/* The zwave notification mutex locks the notification handling region */
static pthread_mutex_t g_zwave_notification_mutex;

/* When the driver is ready, those control how long the OpenZwave
 * driver should be running. */
static bool   g_zwave_keep_running = true;
static pthread_cond_t  g_zwave_running_cond  = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t g_zwave_running_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Those control the access of the notification queue to schedule the
 * openzwave producer and the ruby consumer. */
static pthread_cond_t  g_notification_cond  = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t g_notification_mutex = PTHREAD_MUTEX_INITIALIZER;
static notification_t* g_notification_queue = NULL;

#endif
