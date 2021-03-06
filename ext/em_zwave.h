#ifndef EM_ZWAVE_H
#define EM_ZWAVE_H

#include<stdlib.h>
#include<queue>

#include <openzwave/value_classes/ValueID.h>

#define COMMAND_CLASS_MAR  0xef
#define COMMAND_CLASS_BASIC 0x20
#define COMMAND_CLASS_VERSION 0x86
#define COMMAND_CLASS_BATTERY 0x80
#define COMMAND_CLASS_WAKE_UP 0x84
#define COMMAND_CLASS_CONTROLLER_REPLICATION 0x21
#define COMMAND_CLASS_SWITCH_MULTILEVEL 0x26
#define COMMAND_CLASS_SWITCH_ALL 0x27
#define COMMAND_CLASS_SENSOR_BINARY 0x30
#define COMMAND_CLASS_SENSOR_MULTILEVEL 0x31
#define COMMAND_CLASS_SENSOR_ALARM 0x9c
#define COMMAND_CLASS_ALARM 0x71
#define COMMAND_CLASS_MULTI_CMD 0x8F
#define COMMAND_CLASS_CLIMATE_CONTROL_SCHEDULE 0x46
#define COMMAND_CLASS_CLOCK 0x81
#define COMMAND_CLASS_ASSOCIATION 0x85
#define COMMAND_CLASS_CONFIGURATION 0x70
#define COMMAND_CLASS_MANUFACTURER_SPECIFIC 0x72
#define COMMAND_CLASS_APPLICATION_STATUS 0x22
#define COMMAND_CLASS_ASSOCIATION_COMMAND_CONFIGURATION 0x9B
#define COMMAND_CLASS_AV_CONTENT_DIRECTORY_MD 0x95
#define COMMAND_CLASS_AV_CONTENT_SEARCH_MD 0x97
#define COMMAND_CLASS_AV_RENDERER_STATUS 0x96
#define COMMAND_CLASS_AV_TAGGING_MD 0x99
#define COMMAND_CLASS_BASIC_WINDOW_COVERING 0x50
#define COMMAND_CLASS_CHIMNEY_FAN 0x2A
#define COMMAND_CLASS_COMPOSITE 0x8D
#define COMMAND_CLASS_DOOR_LOCK 0x62
#define COMMAND_CLASS_ENERGY_PRODUCTION 0x90
#define COMMAND_CLASS_FIRMWARE_UPDATE_MD 0x7a
#define COMMAND_CLASS_GEOGRAPHIC_LOCATION 0x8C
#define COMMAND_CLASS_GROUPING_NAME 0x7B
#define COMMAND_CLASS_HAIL 0x82
#define COMMAND_CLASS_INDICATOR 0x87
#define COMMAND_CLASS_IP_CONFIGURATION 0x9A
#define COMMAND_CLASS_LANGUAGE 0x89
#define COMMAND_CLASS_LOCK 0x76
#define COMMAND_CLASS_MANUFACTURER_PROPRIETARY 0x91
#define COMMAND_CLASS_METER_PULSE 0x35
#define COMMAND_CLASS_METER 0x32
#define COMMAND_CLASS_MTP_WINDOW_COVERING 0x51
#define COMMAND_CLASS_MULTI_INSTANCE_ASSOCIATION 0x8E
#define COMMAND_CLASS_MULTI_INSTANCE 0x60
#define COMMAND_CLASS_NO_OPERATION 0x00
#define COMMAND_CLASS_NODE_NAMING 0x77
#define COMMAND_CLASS_NON_INTEROPERABLE 0xf0
#define COMMAND_CLASS_POWERLEVEL 0x73
#define COMMAND_CLASS_PROPRIETARY 0x88
#define COMMAND_CLASS_PROTECTION 0x75
#define COMMAND_CLASS_REMOTE_ASSOCIATION_ACTIVATE 0x7c
#define COMMAND_CLASS_REMOTE_ASSOCIATION 0x7d
#define COMMAND_CLASS_SCENE_ACTIVATION 0x2b
#define COMMAND_CLASS_SCENE_ACTUATOR_CONF 0x2C
#define COMMAND_CLASS_SCENE_CONTROLLER_CONF 0x2D
#define COMMAND_CLASS_SCREEN_ATTRIBUTES 0x93
#define COMMAND_CLASS_SCREEN_MD 0x92
#define COMMAND_CLASS_SECURITY 0x98
#define COMMAND_CLASS_SENSOR_CONFIGURATION 0x9E
#define COMMAND_CLASS_SILENCE_ALARM 0x9d
#define COMMAND_CLASS_SIMPLE_AV_CONTROL 0x94
#define COMMAND_CLASS_SWITCH_BINARY 0x25
#define COMMAND_CLASS_SWITCH_TOGGLE_BINARY 0x28
#define COMMAND_CLASS_SWITCH_TOGGLE_MULTILEVEL 0x29
#define COMMAND_CLASS_THERMOSTAT_FAN_MODE 0x44
#define COMMAND_CLASS_THERMOSTAT_FAN_STATE 0x45
#define COMMAND_CLASS_THERMOSTAT_HEATING 0x38
#define COMMAND_CLASS_THERMOSTAT_MODE 0x40
#define COMMAND_CLASS_THERMOSTAT_OPERATING_STATE 0x42
#define COMMAND_CLASS_THERMOSTAT_SETBACK 0x47
#define COMMAND_CLASS_THERMOSTAT_SETPOINT 0x43
#define COMMAND_CLASS_TIME_PARAMETERS 0x8B
#define COMMAND_CLASS_TIME 0x8a
#define COMMAND_CLASS_USER_CODE 0x63
#define COMMAND_CLASS_ZIP_ADV_CLIENT 0x34
#define COMMAND_CLASS_ZIP_ADV_SERVER 0x33
#define COMMAND_CLASS_ZIP_ADV_SERVICES 0x2F
#define COMMAND_CLASS_ZIP_CLIENT 0x2e
#define COMMAND_CLASS_ZIP_SERVER 0x24
#define COMMAND_CLASS_ZIP_SERVICES 0x23

using namespace std;
using namespace OpenZWave;

typedef struct notification_t notification_t;
struct notification_t {
    int    type;
    uint32 home_id;
    uint8  node_id;
    uint64 value_id;
    uint8  event;
    uint8  group_index;
    uint8  button_id;
    uint8  scene_id;
    uint8  notification;
};

typedef struct node_info_t node_info_t;
struct node_info_t {
    uint32        home_id;
    uint8         node_id;
    bool          polling;
    list<ValueID> values;
};

/* Used to pack configuration data from the ruby class and pass it the */
/* open zwave initialization function */
typedef struct zwave_init_data_t zwave_init_data_t;
struct zwave_init_data_t {
    char* config_path;
    char** devices;
    int devices_length;
};

static ID id_push_notification;
static ID id_schedule_shutdown;
static VALUE rb_mEm;
static VALUE rb_cZwave;
static VALUE rb_cNotification;
static VALUE rb_cNode;
static VALUE rb_cValue;

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
static bool            g_zwave_keep_running = true;
static pthread_cond_t  g_zwave_running_cond  = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t g_zwave_running_mutex = PTHREAD_MUTEX_INITIALIZER;

static bool            g_zwave_manager_stopped = false;
static pthread_cond_t  g_zwave_shutdown_cond  = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t g_zwave_shutdown_mutex = PTHREAD_MUTEX_INITIALIZER;

static bool            g_zwave_event_thread_keep_running = true;

/* Those control the access of the notification queue to schedule the
 * openzwave producer and the ruby consumer. */
static pthread_cond_t  g_notification_cond  = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t g_notification_mutex = PTHREAD_MUTEX_INITIALIZER;
static queue<notification_t*> g_notification_queue;

static int g_notification_overall = 0;
#endif
