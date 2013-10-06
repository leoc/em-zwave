#include "ruby.h"

#include <pthread.h>
#include <iostream>
#include <sstream>
#include <string.h>

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>

#include <limits.h>
#include <float.h>

#include <openzwave/Options.h>
#include <openzwave/Manager.h>
#include <openzwave/Driver.h>
#include <openzwave/Node.h>
#include <openzwave/Group.h>
#include <openzwave/Notification.h>
#include <openzwave/platform/Log.h>
#include <openzwave/value_classes/ValueStore.h>
#include <openzwave/value_classes/Value.h>
#include <openzwave/value_classes/ValueBool.h>

#include "em_zwave.h"

using namespace std;
using namespace OpenZWave;

extern "C"
void g_notification_queue_push(notification_t* notification) {
    notification->next = g_notification_queue;
    g_notification_queue = notification;
}

extern "C"
notification_t* g_notification_queue_pop() {
    notification_t* notification = g_notification_queue;
    if (notification) {
        g_notification_queue = notification->next;
    }
    return notification;
}

extern "C"
int g_notification_count()
{
    int count = 0;

    pthread_mutex_lock(&g_notification_mutex);
    notification_t* current = g_notification_queue;
    while(current != NULL)
    {
        count++;
        current = current->next;
    }
    pthread_mutex_unlock(&g_notification_mutex);

    return count;
}

extern "C"
node_info_t* g_get_node_info(Notification const* _notification)
{
    uint32 const home_id = _notification->GetHomeId();
    uint8 const node_id = _notification->GetNodeId();
    for(list<node_info_t*>::iterator it = g_nodes.begin(); it != g_nodes.end(); ++it)
    {
        node_info_t* node_info = *it;
        if((node_info->home_id == home_id) && (node_info->node_id == node_id))
        {
            return node_info;
        }
    }

    return NULL;
}

extern "C"
void zwave_send_notification(Notification const* _notification) {
    notification_t* notification = (notification_t*)malloc(sizeof(notification_t));

    notification->type         = _notification->GetType();
    notification->home_id      = _notification->GetHomeId();
    notification->node_id      = _notification->GetNodeId();

    notification->value_id   = _notification->GetValueID().GetId();

    if(Notification::Type_NodeEvent == _notification->GetType())
        notification->event        = _notification->GetEvent();
    if(Notification::Type_Group == _notification->GetType())
        notification->group_index  = _notification->GetGroupIdx();
    if(Notification::Type_CreateButton == _notification->GetType() ||
       Notification::Type_DeleteButton == _notification->GetType() ||
       Notification::Type_ButtonOn == _notification->GetType() ||
       Notification::Type_ButtonOff == _notification->GetType())
    {
        notification->button_id    = _notification->GetButtonId();
    }
    if(Notification::Type_SceneEvent == _notification->GetType())
        notification->scene_id     = _notification->GetSceneId();
    if(Notification::Type_Notification == _notification->GetType())
        notification->notification = _notification->GetNotification();

    pthread_mutex_lock(&g_notification_mutex);
    g_notification_queue_push(notification);
    pthread_cond_broadcast(&g_notification_cond);
    pthread_mutex_unlock(&g_notification_mutex);
}

extern "C"
void zwave_on_notification(Notification const* _notification, void* _context) {
    pthread_mutex_lock(&g_zwave_notification_mutex);

    switch(_notification->GetType()) {
    case Notification::Type_ValueAdded:
        {
            if(node_info_t* node_info = g_get_node_info(_notification))
            {
                node_info->values.push_back(_notification->GetValueID());
            }
            break;
        }
    case Notification::Type_ValueRemoved:
        {
            if(node_info_t* node_info = g_get_node_info(_notification))
            {
                for( list<ValueID>::iterator it = node_info->values.begin(); it != node_info->values.end(); ++it)
                {
                    if((*it) == _notification->GetValueID())
                    {
                        node_info->values.erase( it );
                        break;
                    }
                }
            }
            break;
        }
    case Notification::Type_ValueChanged:  break;
    case Notification::Type_ValueRefreshed: break;
    case Notification::Type_Group: break;
    case Notification::Type_NodeNew: break;
    case Notification::Type_NodeAdded:
        {
            node_info_t* node_info = (node_info_t*)malloc(sizeof(node_info_t));
            node_info->home_id = _notification->GetHomeId();
            node_info->node_id = _notification->GetNodeId();
            node_info->polling = false;
            g_nodes.push_back(node_info);
            break;
        }
    case Notification::Type_NodeRemoved:
        {
            uint32 const home_id = _notification->GetHomeId();
            uint8 const node_id = _notification->GetNodeId();
            for(list<node_info_t*>::iterator it = g_nodes.begin(); it != g_nodes.end(); ++it)
            {
                node_info_t* node_info = *it;
                if((node_info->home_id == home_id) && (node_info->node_id == node_id))
                {
                    g_nodes.erase(it);
                    break;
                }
            }
            break;
        }
    case Notification::Type_NodeProtocolInfo: break;
    case Notification::Type_NodeNaming: break;
    case Notification::Type_NodeEvent: break;
    case Notification::Type_PollingDisabled:
        {
            if(node_info_t* node_info = g_get_node_info(_notification))
            {
                node_info->polling = false;
            }
            break;
        }
    case Notification::Type_PollingEnabled:
        {
            if(node_info_t* node_info = g_get_node_info(_notification))
            {
                node_info->polling = true;
            }
            break;
        }
    case Notification::Type_SceneEvent: break;
    case Notification::Type_CreateButton: break;
    case Notification::Type_DeleteButton: break;
    case Notification::Type_ButtonOn: break;
    case Notification::Type_ButtonOff: break;
    case Notification::Type_DriverReady:
        {
            g_zwave_home_id = _notification->GetHomeId();
            break;
        }
    case Notification::Type_DriverFailed:
        {
            g_zwave_init_failed = true;
            pthread_cond_broadcast(&g_zwave_init_cond);
            break;
        }
    case Notification::Type_DriverReset: break;
    case Notification::Type_EssentialNodeQueriesComplete: break;
    case Notification::Type_NodeQueriesComplete: break;
    case Notification::Type_AwakeNodesQueried:
    case Notification::Type_AllNodesQueriedSomeDead:
    case Notification::Type_AllNodesQueried:
        {
            pthread_cond_broadcast(&g_zwave_init_cond);
            break;
        }
    case Notification::Type_Notification: break;
    }

    zwave_send_notification(_notification);

    pthread_mutex_unlock(&g_zwave_notification_mutex);
}

extern "C"
VALUE start_openzwave(void* data) {
    zwave_init_data_t* zwave_data = (zwave_init_data_t*)data;

    pthread_mutexattr_t mutexattr;
    pthread_mutexattr_init(&mutexattr);
    pthread_mutexattr_settype(&mutexattr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&g_zwave_notification_mutex, &mutexattr);
    pthread_mutexattr_destroy(&mutexattr);

    pthread_mutex_lock(&g_zwave_init_mutex);

    // VALUE value = rb_ivar_get(self, rb_intern("@device"));
    // char * c_str = StringValuePtr(value);
    std::string config_path = zwave_data->config_path;
    Options::Create(config_path, "", "");
    Options::Get()->AddOptionBool("PerformReturnRoutes", false);
    Options::Get()->AddOptionBool("ConsoleOutput", false);
    Options::Get()->Lock();
    Manager::Create();

    Manager::Get()->AddWatcher(zwave_on_notification, NULL);

    for(int i = 0; i < zwave_data->devices_length; i++)
    {
        Manager::Get()->AddDriver(zwave_data->devices[i]);
    }

    pthread_cond_wait(&g_zwave_init_cond, &g_zwave_init_mutex);

    if(!g_zwave_init_failed) {
        Manager::Get()->WriteConfig(g_zwave_home_id);

        Driver::DriverData data;
        Manager::Get()->GetDriverStatistics(g_zwave_home_id, &data);
        printf("SOF: %d ACK Waiting: %d Read Aborts: %d Bad Checksums: %d\n", data.m_SOFCnt, data.m_ACKWaiting, data.m_readAborts, data.m_badChecksum);
        printf("Reads: %d Writes: %d CAN: %d NAK: %d ACK: %d Out of Frame: %d\n", data.m_readCnt, data.m_writeCnt, data.m_CANCnt, data.m_NAKCnt, data.m_ACKCnt, data.m_OOFCnt);
        printf("Dropped: %d Retries: %d\n", data.m_dropped, data.m_retries);

        g_zwave_init_done = true;
        pthread_mutex_unlock(&g_zwave_init_mutex);

        pthread_mutex_lock(&g_zwave_running_mutex);
        while (g_zwave_keep_running) {
            pthread_cond_wait(&g_zwave_running_cond, &g_zwave_running_mutex);
        }
        pthread_mutex_unlock(&g_zwave_running_mutex);
    } else {
        printf("Unable to initialize OpenZwave driver!\n");
        pthread_mutex_unlock(&g_zwave_init_mutex);
    }
    Manager::Destroy();

    // TODO: clean up

    /* Tell the event thread, that the manager was stopped */
    pthread_mutex_lock(&g_zwave_shutdown_mutex);
    g_zwave_manager_stopped = true;
    pthread_cond_signal(&g_zwave_shutdown_cond);
    pthread_mutex_unlock(&g_zwave_shutdown_mutex);

    return Qnil;
}

extern "C" void stop_openzwave(void*) {}

extern "C"
VALUE em_zwave_openzwave_thread(void* zwave_data)
{
    rb_thread_blocking_region(start_openzwave, zwave_data, stop_openzwave, NULL);
    return Qnil;
}

extern "C"
VALUE em_zwave_shutdown_no_gvl(void*)
{
    /* If the openzwave initialization is stopped prematurely, */
    /* then this initialization should be seen as failed. */
    pthread_mutex_lock(&g_zwave_init_mutex);
    if(!g_zwave_init_done)
    {
        g_zwave_init_failed = true;
        pthread_cond_broadcast(&g_zwave_init_cond);
    }
    pthread_mutex_unlock(&g_zwave_init_mutex);

    /* Finally we can stop the manager.  */
    pthread_mutex_lock(&g_zwave_running_mutex);
    g_zwave_keep_running = false;
    pthread_cond_broadcast(&g_zwave_running_cond);
    pthread_mutex_unlock(&g_zwave_running_mutex);

    /* Wait for the OpenZWave manager to shutdown. */
    pthread_mutex_lock(&g_zwave_shutdown_mutex);
    while(!g_zwave_manager_stopped)
    {
        pthread_cond_wait(&g_zwave_shutdown_cond, &g_zwave_shutdown_mutex);
    }
    pthread_mutex_unlock(&g_zwave_shutdown_mutex);

    /* Tell the event thread, that we want to stop as soon as there */
    /* are no more notifications in the queue! */
    pthread_mutex_lock(&g_notification_mutex);
    g_zwave_event_thread_keep_running = false;
    pthread_mutex_unlock(&g_notification_mutex);
    pthread_cond_broadcast(&g_notification_cond);

    return Qnil;
}

extern "C" void em_zwave_stop_shutdown(void*) {}

extern "C"
VALUE rb_zwave_shutdown_thread(void*)
{
    rb_thread_blocking_region(em_zwave_shutdown_no_gvl, NULL,
                              em_zwave_stop_shutdown, NULL);
    return Qnil;
}

extern "C"
VALUE rb_zwave_shutdown(VALUE self)
{
    rb_thread_create((VALUE (*)(...))rb_zwave_shutdown_thread, (void*)self);
    return Qnil;
}

extern "C"
VALUE rb_zwave_all_on(VALUE self)
{
    Manager::Get()->SwitchAllOn(g_zwave_home_id);
    return Qnil;
}

extern "C"
VALUE rb_zwave_all_off(VALUE self)
{
    Manager::Get()->SwitchAllOff(g_zwave_home_id);
    return Qnil;
}

extern "C"
VALUE wait_for_notification(void* n) {
    notification_t** notification = (notification_t**)n;

    pthread_mutex_lock(&g_notification_mutex);
    while (g_zwave_event_thread_keep_running & (*notification = g_notification_queue_pop()) == NULL)
    {
        pthread_cond_wait(&g_notification_cond, &g_notification_mutex);
    }

    pthread_mutex_unlock(&g_notification_mutex);
    return Qnil;
}

extern "C" void stop_waiting_for_notification(void* w) {}

extern "C"
VALUE em_zwave_get_notification_type_symbol(int type) {
    switch(type)
    {
    case Notification::Type_ValueAdded: return ID2SYM(rb_intern("value_added"));
    case Notification::Type_ValueRemoved: return ID2SYM(rb_intern("value_removed"));
    case Notification::Type_ValueChanged: return ID2SYM(rb_intern("value_changed"));
    case Notification::Type_ValueRefreshed: return ID2SYM(rb_intern("value_refreshed"));
    case Notification::Type_Group: return ID2SYM(rb_intern("group"));
    case Notification::Type_NodeNew: return ID2SYM(rb_intern("node_new"));
    case Notification::Type_NodeAdded: return ID2SYM(rb_intern("node_added"));
    case Notification::Type_NodeRemoved: return ID2SYM(rb_intern("node_removed"));
    case Notification::Type_NodeProtocolInfo: return ID2SYM(rb_intern("node_protocol_info"));
    case Notification::Type_NodeNaming: return ID2SYM(rb_intern("node_naming"));
    case Notification::Type_NodeEvent: return ID2SYM(rb_intern("node_event"));
    case Notification::Type_PollingDisabled: return ID2SYM(rb_intern("polling_disabled"));
    case Notification::Type_PollingEnabled: return ID2SYM(rb_intern("polling_enabled"));
    case Notification::Type_SceneEvent: return ID2SYM(rb_intern("scene_event"));
    case Notification::Type_CreateButton: return ID2SYM(rb_intern("create_button"));
    case Notification::Type_DeleteButton: return ID2SYM(rb_intern("delete_button"));
    case Notification::Type_ButtonOn: return ID2SYM(rb_intern("button_on"));
    case Notification::Type_ButtonOff: return ID2SYM(rb_intern("button_off"));
    case Notification::Type_DriverReady: return ID2SYM(rb_intern("driver_ready"));
    case Notification::Type_DriverFailed: return ID2SYM(rb_intern("driver_failed"));
    case Notification::Type_DriverReset: return ID2SYM(rb_intern("driver_reset"));
    case Notification::Type_EssentialNodeQueriesComplete: return ID2SYM(rb_intern("essential_node_queries_complete"));
    case Notification::Type_NodeQueriesComplete: return ID2SYM(rb_intern("node_queries_complete"));
    case Notification::Type_AwakeNodesQueried: return ID2SYM(rb_intern("awake_nodes_queried"));
    case Notification::Type_AllNodesQueriedSomeDead: return ID2SYM(rb_intern("all_nodes_queried_some_dead"));
    case Notification::Type_AllNodesQueried: return ID2SYM(rb_intern("all_nodes_queried"));
    case Notification::Type_Notification: return ID2SYM(rb_intern("notification"));
    default: return INT2FIX(type);
    }
}

/* This thread loops continuously, waiting for callbacks happening in the C thread. */
extern "C"
VALUE em_zwave_event_thread(void* args)
{
    VALUE zwave = (VALUE)args;

    notification_t* waiting_notification = NULL;

    while(g_zwave_event_thread_keep_running || g_notification_count() > 0)
    {
        rb_thread_blocking_region(wait_for_notification, &waiting_notification,
                                  stop_waiting_for_notification, NULL);

        if(waiting_notification != NULL)
        {
            g_notification_overall++;

            VALUE notification = rb_obj_alloc(rb_cNotification);
            VALUE notification_options = rb_hash_new();

            rb_hash_aset(notification_options, ID2SYM(rb_intern("type")), em_zwave_get_notification_type_symbol(waiting_notification->type));
            rb_hash_aset(notification_options, ID2SYM(rb_intern("home_id")), INT2FIX(waiting_notification->home_id));
            rb_hash_aset(notification_options, ID2SYM(rb_intern("node_id")), INT2FIX(waiting_notification->node_id));
            rb_hash_aset(notification_options, ID2SYM(rb_intern("value_id")), LONG2FIX(waiting_notification->value_id));

            if(Notification::Type_NodeEvent == waiting_notification->type)
            {
                rb_hash_aset(notification_options, ID2SYM(rb_intern("event")), INT2FIX(waiting_notification->event));
            }
            if(Notification::Type_Group == waiting_notification->type)
            {
                rb_hash_aset(notification_options, ID2SYM(rb_intern("group_index")), INT2FIX(waiting_notification->group_index));
            }
            if(Notification::Type_CreateButton == waiting_notification->type ||
               Notification::Type_DeleteButton == waiting_notification->type ||
               Notification::Type_ButtonOn     == waiting_notification->type ||
               Notification::Type_ButtonOff    == waiting_notification->type)
            {
                rb_hash_aset(notification_options, ID2SYM(rb_intern("button_id")), INT2FIX(waiting_notification->button_id));
            }
            if(Notification::Type_SceneEvent == waiting_notification->type)
            {
                rb_hash_aset(notification_options, ID2SYM(rb_intern("scene_id")), INT2FIX(waiting_notification->scene_id));
            }
            if(Notification::Type_Notification == waiting_notification->type)
            {
                rb_hash_aset(notification_options, ID2SYM(rb_intern("notification")), INT2FIX(waiting_notification->notification));
            }

            VALUE init_argv[1];
            init_argv[0] = notification_options;
            rb_obj_call_init(notification, 1, init_argv);

            rb_funcall(zwave, id_push_notification, 1, notification);
        }

        waiting_notification = NULL;
    }

    // TODO: clean up all allocated resources

    rb_funcall(zwave, id_schedule_shutdown, 0, NULL);

    return Qnil;
}

extern "C"
VALUE rb_zwave_initialize_zwave(VALUE self) {
    zwave_init_data_t* zwave_data = (zwave_init_data_t*)malloc(sizeof(zwave_init_data_t));

    VALUE rb_config_path = rb_iv_get(self, "@config_path");
    zwave_data->config_path = StringValuePtr(rb_config_path);

    VALUE rb_devices = rb_iv_get(self, "@devices");
    zwave_data->devices_length = RARRAY_LEN(rb_devices);
    zwave_data->devices = (char**)malloc(zwave_data->devices_length * sizeof(char*));
    for(int i = 0; i < zwave_data->devices_length; i++)
    {
        VALUE rb_device_str = rb_ary_entry(rb_devices, (long)i);
        zwave_data->devices[0] = StringValuePtr(rb_device_str);
    }

    rb_thread_create((VALUE (*)(...))em_zwave_openzwave_thread, (void*)zwave_data);
    rb_thread_create((VALUE (*)(...))em_zwave_event_thread, (void*)self);
    return Qnil;
}

extern "C"
VALUE rb_node_type(VALUE self)
{
    uint32 home_id = FIX2UINT(rb_iv_get(self, "@home_id"));
    uint8 node_id = FIX2UINT(rb_iv_get(self, "@node_id"));
    string type = Manager::Get()->GetNodeType(home_id, node_id);
    return rb_str_new2(type.c_str());
}

extern "C"
VALUE rb_node_is_listening_device(VALUE self)
{
    uint32 home_id = FIX2UINT(rb_iv_get(self, "@home_id"));
    uint8 node_id = FIX2UINT(rb_iv_get(self, "@node_id"));
    bool is_listening_device = Manager::Get()->IsNodeListeningDevice(home_id, node_id);
    return (is_listening_device ? Qtrue : Qfalse);
}

extern "C"
VALUE rb_node_on(VALUE self)
{
    uint32 home_id = FIX2UINT(rb_iv_get(self, "@home_id"));
    uint8 node_id = FIX2UINT(rb_iv_get(self, "@node_id"));
    Manager::Get()->SetNodeOn(home_id, node_id);
    return Qnil;
}

extern "C"
VALUE rb_node_off(VALUE self)
{
    uint32 home_id = FIX2UINT(rb_iv_get(self, "@home_id"));
    uint8 node_id = FIX2UINT(rb_iv_get(self, "@node_id"));
    Manager::Get()->SetNodeOff(home_id, node_id);
    return Qnil;
}

extern "C"
VALUE rb_value_get_label(VALUE self)
{
    uint32 home_id = FIX2UINT(rb_iv_get(self, "@home_id"));
    uint64 long_id = (uint64)FIX2LONG(rb_iv_get(self, "@value_id"));
    uint32 id1 = (uint32)(long_id & 0xFFFFFFFF);
    uint32 id2 = (uint32)(long_id >> 32);
    uint8 node_id = (uint8)((id1 & 0xff000000) >> 24);
    ValueID::ValueGenre genre = (ValueID::ValueGenre)((id1 & 0x00c00000) >> 22);
    uint8 command_class_id = (uint8)((id1 & 0x003fc000) >> 14);
    uint8 instance = (uint8)(((id2 & 0xff000000)) >> 24);
    uint8 index = (uint8)((id1 & 0x00000ff0) >> 4);
    ValueID::ValueType type = (ValueID::ValueType)(id1 & 0x0000000f);

    ValueID id(home_id, node_id, genre, command_class_id, instance, index, type);

    string label = Manager::Get()->GetValueLabel(id);
    return rb_str_new2(label.c_str());
}

extern "C"
VALUE rb_value_get_units(VALUE self)
{
    uint32 home_id = FIX2UINT(rb_iv_get(self, "@home_id"));
    uint64 long_id = (uint64)FIX2LONG(rb_iv_get(self, "@value_id"));
    uint32 id1 = (uint32)(long_id & 0xFFFFFFFF);
    uint32 id2 = (uint32)(long_id >> 32);
    uint8 node_id = (uint8)((id1 & 0xff000000) >> 24);
    ValueID::ValueGenre genre = (ValueID::ValueGenre)((id1 & 0x00c00000) >> 22);
    uint8 command_class_id = (uint8)((id1 & 0x003fc000) >> 14);
    uint8 instance = (uint8)(((id2 & 0xff000000)) >> 24);
    uint8 index = (uint8)((id1 & 0x00000ff0) >> 4);
    ValueID::ValueType type = (ValueID::ValueType)(id1 & 0x0000000f);

    ValueID id(home_id, node_id, genre, command_class_id, instance, index, type);

    string units = Manager::Get()->GetValueUnits(id);
    return rb_str_new2(units.c_str());
}

extern "C"
VALUE rb_value_get_help(VALUE self)
{
    uint32 home_id = FIX2UINT(rb_iv_get(self, "@home_id"));
    uint64 long_id = (uint64)FIX2LONG(rb_iv_get(self, "@value_id"));
    uint32 id1 = (uint32)(long_id & 0xFFFFFFFF);
    uint32 id2 = (uint32)(long_id >> 32);
    uint8 node_id = (uint8)((id1 & 0xff000000) >> 24);
    ValueID::ValueGenre genre = (ValueID::ValueGenre)((id1 & 0x00c00000) >> 22);
    uint8 command_class_id = (uint8)((id1 & 0x003fc000) >> 14);
    uint8 instance = (uint8)(((id2 & 0xff000000)) >> 24);
    uint8 index = (uint8)((id1 & 0x00000ff0) >> 4);
    ValueID::ValueType type = (ValueID::ValueType)(id1 & 0x0000000f);

    ValueID id(home_id, node_id, genre, command_class_id, instance, index, type);

    string help = Manager::Get()->GetValueHelp(id);
    return rb_str_new2(help.c_str());
}

extern "C"
VALUE rb_value_get_min(VALUE self)
{
    uint32 home_id = FIX2UINT(rb_iv_get(self, "@home_id"));
    uint64 long_id = (uint64)FIX2LONG(rb_iv_get(self, "@value_id"));
    uint32 id1 = (uint32)(long_id & 0xFFFFFFFF);
    uint32 id2 = (uint32)(long_id >> 32);
    uint8 node_id = (uint8)((id1 & 0xff000000) >> 24);
    ValueID::ValueGenre genre = (ValueID::ValueGenre)((id1 & 0x00c00000) >> 22);
    uint8 command_class_id = (uint8)((id1 & 0x003fc000) >> 14);
    uint8 instance = (uint8)(((id2 & 0xff000000)) >> 24);
    uint8 index = (uint8)((id1 & 0x00000ff0) >> 4);
    ValueID::ValueType type = (ValueID::ValueType)(id1 & 0x0000000f);

    ValueID id(home_id, node_id, genre, command_class_id, instance, index, type);

    return INT2FIX(Manager::Get()->GetValueMin(id));
}

extern "C"
VALUE rb_value_get_max(VALUE self)
{
    uint32 home_id = FIX2UINT(rb_iv_get(self, "@home_id"));
    uint64 long_id = (uint64)FIX2LONG(rb_iv_get(self, "@value_id"));
    uint32 id1 = (uint32)(long_id & 0xFFFFFFFF);
    uint32 id2 = (uint32)(long_id >> 32);
    uint8 node_id = (uint8)((id1 & 0xff000000) >> 24);
    ValueID::ValueGenre genre = (ValueID::ValueGenre)((id1 & 0x00c00000) >> 22);
    uint8 command_class_id = (uint8)((id1 & 0x003fc000) >> 14);
    uint8 instance = (uint8)(((id2 & 0xff000000)) >> 24);
    uint8 index = (uint8)((id1 & 0x00000ff0) >> 4);
    ValueID::ValueType type = (ValueID::ValueType)(id1 & 0x0000000f);

    ValueID id(home_id, node_id, genre, command_class_id, instance, index, type);

    return INT2FIX(Manager::Get()->GetValueMax(id));
}

extern "C"
VALUE rb_value_is_read_only(VALUE self)
{
    uint32 home_id = FIX2UINT(rb_iv_get(self, "@home_id"));
    uint64 long_id = (uint64)FIX2LONG(rb_iv_get(self, "@value_id"));
    uint32 id1 = (uint32)(long_id & 0xFFFFFFFF);
    uint32 id2 = (uint32)(long_id >> 32);
    uint8 node_id = (uint8)((id1 & 0xff000000) >> 24);
    ValueID::ValueGenre genre = (ValueID::ValueGenre)((id1 & 0x00c00000) >> 22);
    uint8 command_class_id = (uint8)((id1 & 0x003fc000) >> 14);
    uint8 instance = (uint8)(((id2 & 0xff000000)) >> 24);
    uint8 index = (uint8)((id1 & 0x00000ff0) >> 4);
    ValueID::ValueType type = (ValueID::ValueType)(id1 & 0x0000000f);

    ValueID id(home_id, node_id, genre, command_class_id, instance, index, type);

    bool readonly = Manager::Get()->IsValueReadOnly(id);
    return (readonly ? Qtrue : Qfalse);
}

extern "C"
VALUE rb_value_is_write_only(VALUE self)
{
    uint32 home_id = FIX2UINT(rb_iv_get(self, "@home_id"));
    uint64 long_id = (uint64)FIX2LONG(rb_iv_get(self, "@value_id"));
    uint32 id1 = (uint32)(long_id & 0xFFFFFFFF);
    uint32 id2 = (uint32)(long_id >> 32);
    uint8 node_id = (uint8)((id1 & 0xff000000) >> 24);
    ValueID::ValueGenre genre = (ValueID::ValueGenre)((id1 & 0x00c00000) >> 22);
    uint8 command_class_id = (uint8)((id1 & 0x003fc000) >> 14);
    uint8 instance = (uint8)(((id2 & 0xff000000)) >> 24);
    uint8 index = (uint8)((id1 & 0x00000ff0) >> 4);
    ValueID::ValueType type = (ValueID::ValueType)(id1 & 0x0000000f);

    ValueID id(home_id, node_id, genre, command_class_id, instance, index, type);

    bool writeonly = Manager::Get()->IsValueWriteOnly(id);
    return (writeonly ? Qtrue : Qfalse);
}

extern "C"
VALUE rb_value_is_set(VALUE self)
{
    uint32 home_id = FIX2UINT(rb_iv_get(self, "@home_id"));
    uint64 long_id = (uint64)FIX2LONG(rb_iv_get(self, "@value_id"));
    uint32 id1 = (uint32)(long_id & 0xFFFFFFFF);
    uint32 id2 = (uint32)(long_id >> 32);
    uint8 node_id = (uint8)((id1 & 0xff000000) >> 24);
    ValueID::ValueGenre genre = (ValueID::ValueGenre)((id1 & 0x00c00000) >> 22);
    uint8 command_class_id = (uint8)((id1 & 0x003fc000) >> 14);
    uint8 instance = (uint8)(((id2 & 0xff000000)) >> 24);
    uint8 index = (uint8)((id1 & 0x00000ff0) >> 4);
    ValueID::ValueType type = (ValueID::ValueType)(id1 & 0x0000000f);

    ValueID id(home_id, node_id, genre, command_class_id, instance, index, type);

    bool set = Manager::Get()->IsValueSet(id);
    return (set ? Qtrue : Qfalse);
}


extern "C"
VALUE rb_value_is_polled(VALUE self)
{
    uint32 home_id = FIX2UINT(rb_iv_get(self, "@home_id"));
    uint64 long_id = (uint64)FIX2LONG(rb_iv_get(self, "@value_id"));
    uint32 id1 = (uint32)(long_id & 0xFFFFFFFF);
    uint32 id2 = (uint32)(long_id >> 32);
    uint8 node_id = (uint8)((id1 & 0xff000000) >> 24);
    ValueID::ValueGenre genre = (ValueID::ValueGenre)((id1 & 0x00c00000) >> 22);
    uint8 command_class_id = (uint8)((id1 & 0x003fc000) >> 14);
    uint8 instance = (uint8)(((id2 & 0xff000000)) >> 24);
    uint8 index = (uint8)((id1 & 0x00000ff0) >> 4);
    ValueID::ValueType type = (ValueID::ValueType)(id1 & 0x0000000f);

    ValueID id(home_id, node_id, genre, command_class_id, instance, index, type);

    bool polled = Manager::Get()->IsValuePolled(id);
    return (polled ? Qtrue : Qfalse);
}

extern "C"
VALUE rb_value_get_value(VALUE self)
{
    uint32 home_id = FIX2UINT(rb_iv_get(self, "@home_id"));
    uint64 long_id = (uint64)FIX2LONG(rb_iv_get(self, "@value_id"));
    uint32 id1 = (uint32)(long_id & 0xFFFFFFFF);
    uint32 id2 = (uint32)(long_id >> 32);
    uint8 node_id = (uint8)((id1 & 0xff000000) >> 24);
    ValueID::ValueGenre genre = (ValueID::ValueGenre)((id1 & 0x00c00000) >> 22);
    uint8 command_class_id = (uint8)((id1 & 0x003fc000) >> 14);
    uint8 instance = (uint8)(((id2 & 0xff000000)) >> 24);
    uint8 index = (uint8)((id1 & 0x00000ff0) >> 4);
    ValueID::ValueType type = (ValueID::ValueType)(id1 & 0x0000000f);

    ValueID id(home_id, node_id, genre, command_class_id, instance, index, type);

    VALUE value = Qnil;

    string str_val;
    Manager::Get()->GetValueAsString(id, &str_val);

    printf("--- %s ---\n", str_val.c_str());

    if(str_val == "True") printf("AN! \n");
    if(str_val == "False") printf("AUS! \n");


    switch(id.GetType()) {
    case ValueID::ValueType_Bool:
        {
            bool val;
            Manager::Get()->GetValueAsBool(id, &val);
            value = val ? Qtrue : Qfalse;
            break;
        }
    case ValueID::ValueType_Byte:
        {
            uint8 val;
            Manager::Get()->GetValueAsByte(id, &val);
            value = INT2FIX(val);
            break;
        }
    case ValueID::ValueType_Decimal:
        {
            float val;
            Manager::Get()->GetValueAsFloat(id, &val);
            value = rb_float_new(val);
            break;
        }
    case ValueID::ValueType_Int:
        {
            int32 val;
            Manager::Get()->GetValueAsInt(id, &val);
            value = INT2FIX(val);
            break;
        }
    case ValueID::ValueType_List:
        {
            printf("List values not yet implemented!\n");
            break;
        }

    case ValueID::ValueType_Schedule:
        {
            printf("Schedule values not yet implemented!\n");
            break;
        }

    case ValueID::ValueType_Short:
        {
            int16 val;
            Manager::Get()->GetValueAsShort(id, &val);
            value = INT2FIX(val);
            break;
        }

    case ValueID::ValueType_String:
        {
            value = rb_str_new2(str_val.c_str());
            break;
        }

    case ValueID::ValueType_Button:
        {
            printf("Button values not yet implemented!\n");
            break;
        }

    case ValueID::ValueType_Raw:
        {
            printf("Raw values not yet implemented!\n");
            break;
        }
    }
    return value;
}

extern "C"
VALUE rb_value_set_value(VALUE self, VALUE val)
{
    uint32 home_id = FIX2UINT(rb_iv_get(self, "@home_id"));
    uint64 long_id = (uint64)FIX2LONG(rb_iv_get(self, "@value_id"));
    uint32 id1 = (uint32)(long_id & 0xFFFFFFFF);
    uint32 id2 = (uint32)(long_id >> 32);
    uint8 node_id = (uint8)((id1 & 0xff000000) >> 24);
    ValueID::ValueGenre genre = (ValueID::ValueGenre)((id1 & 0x00c00000) >> 22);
    uint8 command_class_id = (uint8)((id1 & 0x003fc000) >> 14);
    uint8 instance = (uint8)(((id2 & 0xff000000)) >> 24);
    uint8 index = (uint8)((id1 & 0x00000ff0) >> 4);
    ValueID::ValueType type = (ValueID::ValueType)(id1 & 0x0000000f);

    ValueID id(home_id, node_id, genre, command_class_id, instance, index, type);

    switch(id.GetType()) {
    case ValueID::ValueType_Bool:
        {
            Manager::Get()->SetValue(id, (bool)(val == Qtrue ? 1 : 0));
            break;
        }

    case ValueID::ValueType_Byte:
        {
            Manager::Get()->SetValue(id, (uint8)FIX2UINT(val));
            break;
        }

    case ValueID::ValueType_Decimal:
        {
            Manager::Get()->SetValue(id, (float)NUM2DBL(val));
            break;
        }

    case ValueID::ValueType_Int:
        {
            Manager::Get()->SetValue(id, (int32)FIX2INT(val));
            break;
        }

    case ValueID::ValueType_List:
        {
            printf("List values not yet implemented!\n");
            break;
        }

    case ValueID::ValueType_Schedule:
        {
            printf("Schedule values not yet implemented!\n");
            break;
        }

    case ValueID::ValueType_Short:
        {
            Manager::Get()->SetValue(id, (int16)FIX2INT(val));
            break;
        }

    case ValueID::ValueType_String:
        {
            string val_str(StringValuePtr(val));
            Manager::Get()->SetValue(id, val_str);
            break;
        }
    case ValueID::ValueType_Button:
        {
            printf("Button values not yet implemented!\n");
            break;
        }

    case ValueID::ValueType_Raw:
        {
            printf("Raw values not yet implemented!\n");
            break;
        }
    }

    return Qnil;
}

extern "C"
void Init_emzwave() {
    id_push_notification = rb_intern("push_notification");
    id_schedule_shutdown = rb_intern("schedule_shutdown");

    rb_mEm = rb_define_module("EventMachine");

    rb_cZwave = rb_define_class_under(rb_mEm, "Zwave", rb_cObject);
    rb_define_method(rb_cZwave, "initialize_zwave", (VALUE (*)(...))rb_zwave_initialize_zwave, 0);
    rb_define_method(rb_cZwave, "shutdown", (VALUE (*)(...))rb_zwave_shutdown, 0);
    rb_define_method(rb_cZwave, "all_on!", (VALUE (*)(...))rb_zwave_all_on, 0);
    rb_define_method(rb_cZwave, "all_off!", (VALUE (*)(...))rb_zwave_all_off, 0);

    rb_cNode  = rb_define_class_under(rb_cZwave, "Node", rb_cObject);
    // rb_define_method(rb_cZwave, "basic_type", (VALUE (*)(...))rb_node_basic_type, 0);
    // rb_define_method(rb_cZwave, "generic_type", (VALUE (*)(...))rb_node_generic_type, 0);
    // rb_define_method(rb_cZwave, "specific_type", (VALUE (*)(...))rb_node_specific_type, 0);
    rb_define_method(rb_cNode, "type", (VALUE (*)(...))rb_node_type, 0);
    rb_define_method(rb_cNode, "on!", (VALUE (*)(...))rb_node_on, 0);
    rb_define_method(rb_cNode, "off!", (VALUE (*)(...))rb_node_off, 0);
    rb_define_method(rb_cNode, "listening_device?", (VALUE (*)(...))rb_node_is_listening_device, 0);
    //rb_define_method(rb_cZwave, "values", (VALUE (*)(...))rb_node_values, 0);

    rb_cValue = rb_define_class_under(rb_cZwave, "Value", rb_cObject);
    rb_define_method(rb_cValue, "label", (VALUE (*)(...))rb_value_get_label, 0);
    rb_define_method(rb_cValue, "units", (VALUE (*)(...))rb_value_get_units, 0);
    rb_define_method(rb_cValue, "help", (VALUE (*)(...))rb_value_get_help, 0);
    rb_define_method(rb_cValue, "min", (VALUE (*)(...))rb_value_get_min, 0);
    rb_define_method(rb_cValue, "max", (VALUE (*)(...))rb_value_get_max, 0);
    rb_define_method(rb_cValue, "get", (VALUE (*)(...))rb_value_get_value, 0);
    rb_define_method(rb_cValue, "set", (VALUE (*)(...))rb_value_set_value, 1);
    rb_define_method(rb_cValue, "read_only?", (VALUE (*)(...))rb_value_is_read_only, 0);
    rb_define_method(rb_cValue, "write_only?", (VALUE (*)(...))rb_value_is_write_only, 0);
    rb_define_method(rb_cValue, "set?", (VALUE (*)(...))rb_value_is_set, 0);
    rb_define_method(rb_cValue, "polled?", (VALUE (*)(...))rb_value_is_polled, 0);

    rb_cNotification = rb_define_class_under(rb_cZwave, "Notification", rb_cObject);
}
