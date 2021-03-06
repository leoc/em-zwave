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
    g_notification_queue.push(notification);
    pthread_cond_broadcast(&g_notification_cond);
    pthread_mutex_unlock(&g_notification_mutex);
}

extern "C"
void zwave_on_notification(Notification const* _notification, void* _context) {
    pthread_mutex_lock(&g_zwave_notification_mutex);

    switch(_notification->GetType()) {
    case Notification::Type_ValueAdded: break;
    case Notification::Type_ValueRemoved: break;
    case Notification::Type_ValueChanged: break;
    case Notification::Type_ValueRefreshed: break;
    case Notification::Type_Group: break;
    case Notification::Type_NodeNew: break;
    case Notification::Type_NodeAdded: break;
    case Notification::Type_NodeRemoved: break;
    case Notification::Type_NodeProtocolInfo: break;
    case Notification::Type_NodeNaming: break;
    case Notification::Type_NodeEvent: break;
    case Notification::Type_PollingDisabled: break;
    case Notification::Type_PollingEnabled: break;
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
    Options::Get()->AddOptionBool("ValidateValueChanges", true);
    Options::Get()->AddOptionBool("SuppressValueRefresh", true);
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
VALUE wait_for_notification(void*) {
    pthread_mutex_lock(&g_notification_mutex);
    while (g_zwave_event_thread_keep_running & g_notification_queue.empty()) {
        pthread_cond_wait(&g_notification_cond, &g_notification_mutex);
    }
    pthread_mutex_unlock(&g_notification_mutex);

    return Qnil;
}

extern "C" void stop_waiting_for_notification(void*) {}

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

    while(g_zwave_event_thread_keep_running || !g_notification_queue.empty())
    {
        rb_thread_blocking_region(wait_for_notification, &waiting_notification,
                                  stop_waiting_for_notification, NULL);

        pthread_mutex_lock(&g_notification_mutex);
        if(!g_notification_queue.empty())
            waiting_notification = g_notification_queue.front();
        pthread_mutex_unlock(&g_notification_mutex);

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

        pthread_mutex_lock(&g_notification_mutex);
        g_notification_queue.pop();
        pthread_mutex_unlock(&g_notification_mutex);
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
VALUE rb_node_is_listening_device(VALUE self)
{
    uint32 home_id = FIX2UINT(rb_iv_get(self, "@home_id"));
    uint8 node_id = FIX2UINT(rb_iv_get(self, "@node_id"));
    bool is_listening_device = Manager::Get()->IsNodeListeningDevice(home_id, node_id);
    return (is_listening_device ? Qtrue : Qfalse);
}

extern "C"
VALUE rb_node_is_frequent_listening_device(VALUE self)
{
    uint32 home_id = FIX2UINT(rb_iv_get(self, "@home_id"));
    uint8 node_id = FIX2UINT(rb_iv_get(self, "@node_id"));
    bool is_frequent_listening_device = Manager::Get()->IsNodeFrequentListeningDevice(home_id, node_id);
    return (is_frequent_listening_device ? Qtrue : Qfalse);
}

extern "C"
VALUE rb_node_is_beaming_device(VALUE self)
{
    uint32 home_id = FIX2UINT(rb_iv_get(self, "@home_id"));
    uint8 node_id = FIX2UINT(rb_iv_get(self, "@node_id"));
    bool is_beaming_device = Manager::Get()->IsNodeBeamingDevice(home_id, node_id);
    return (is_beaming_device ? Qtrue : Qfalse);
}

extern "C"
VALUE rb_node_is_routing_device(VALUE self)
{
    uint32 home_id = FIX2UINT(rb_iv_get(self, "@home_id"));
    uint8 node_id = FIX2UINT(rb_iv_get(self, "@node_id"));
    bool is_routing_device = Manager::Get()->IsNodeRoutingDevice(home_id, node_id);
    return (is_routing_device ? Qtrue : Qfalse);
}

extern "C"
VALUE rb_node_is_security_device(VALUE self)
{
    uint32 home_id = FIX2UINT(rb_iv_get(self, "@home_id"));
    uint8 node_id = FIX2UINT(rb_iv_get(self, "@node_id"));
    bool is_security_device = Manager::Get()->IsNodeSecurityDevice(home_id, node_id);
    return (is_security_device ? Qtrue : Qfalse);
}

extern "C"
VALUE rb_node_get_max_baud_rate(VALUE self)
{
    uint32 home_id = FIX2UINT(rb_iv_get(self, "@home_id"));
    uint8 node_id = FIX2UINT(rb_iv_get(self, "@node_id"));
    uint32 max_baud_rate = Manager::Get()->GetNodeMaxBaudRate(home_id, node_id);
    return INT2FIX(max_baud_rate);
}

extern "C"
VALUE rb_node_get_version(VALUE self)
{
    uint32 home_id = FIX2UINT(rb_iv_get(self, "@home_id"));
    uint8 node_id = FIX2UINT(rb_iv_get(self, "@node_id"));
    uint8 version = Manager::Get()->GetNodeMaxBaudRate(home_id, node_id);
    return INT2FIX(version);
}

extern "C"
VALUE rb_node_get_security(VALUE self)
{
    uint32 home_id = FIX2UINT(rb_iv_get(self, "@home_id"));
    uint8 node_id = FIX2UINT(rb_iv_get(self, "@node_id"));
    uint8 security = Manager::Get()->GetNodeSecurity(home_id, node_id);
    return INT2FIX(security);
}

extern "C"
VALUE rb_node_get_basic_type(VALUE self)
{
    uint32 home_id = FIX2UINT(rb_iv_get(self, "@home_id"));
    uint8 node_id = FIX2UINT(rb_iv_get(self, "@node_id"));
    uint8 basic_type = Manager::Get()->GetNodeBasic(home_id, node_id);
    return INT2FIX(basic_type);
}

extern "C"
VALUE rb_node_get_generic_type(VALUE self)
{
    uint32 home_id = FIX2UINT(rb_iv_get(self, "@home_id"));
    uint8 node_id = FIX2UINT(rb_iv_get(self, "@node_id"));
    uint8 generic_type = Manager::Get()->GetNodeGeneric(home_id, node_id);
    return INT2FIX(generic_type);
}

extern "C"
VALUE rb_node_get_specific_type(VALUE self)
{
    uint32 home_id = FIX2UINT(rb_iv_get(self, "@home_id"));
    uint8 node_id = FIX2UINT(rb_iv_get(self, "@node_id"));
    uint8 specific_type = Manager::Get()->GetNodeSpecific(home_id, node_id);
    return INT2FIX(specific_type);
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
VALUE rb_node_get_manufacturer_name(VALUE self)
{
    uint32 home_id = FIX2UINT(rb_iv_get(self, "@home_id"));
    uint8 node_id = FIX2UINT(rb_iv_get(self, "@node_id"));
    string manufacturer_name = Manager::Get()->GetNodeManufacturerName(home_id, node_id);
    return rb_str_new2(manufacturer_name.c_str());
}

extern "C"
VALUE rb_node_get_manufacturer_id(VALUE self)
{
    uint32 home_id = FIX2UINT(rb_iv_get(self, "@home_id"));
    uint8 node_id = FIX2UINT(rb_iv_get(self, "@node_id"));
    string manufacturer_id = Manager::Get()->GetNodeManufacturerId(home_id, node_id);
    return rb_str_new2(manufacturer_id.c_str());
}

extern "C"
VALUE rb_node_get_product_name(VALUE self)
{
    uint32 home_id = FIX2UINT(rb_iv_get(self, "@home_id"));
    uint8 node_id = FIX2UINT(rb_iv_get(self, "@node_id"));
    string product_name = Manager::Get()->GetNodeProductName(home_id, node_id);
    return rb_str_new2(product_name.c_str());
}

extern "C"
VALUE rb_node_get_product_type(VALUE self)
{
    uint32 home_id = FIX2UINT(rb_iv_get(self, "@home_id"));
    uint8 node_id = FIX2UINT(rb_iv_get(self, "@node_id"));
    string product_type = Manager::Get()->GetNodeProductType(home_id, node_id);
    return rb_str_new2(product_type.c_str());
}

extern "C"
VALUE rb_node_get_product_id(VALUE self)
{
    uint32 home_id = FIX2UINT(rb_iv_get(self, "@home_id"));
    uint8 node_id = FIX2UINT(rb_iv_get(self, "@node_id"));
    string product_id = Manager::Get()->GetNodeProductId(home_id, node_id);
    return rb_str_new2(product_id.c_str());
}

extern "C"
VALUE rb_node_get_name(VALUE self)
{
    uint32 home_id = FIX2UINT(rb_iv_get(self, "@home_id"));
    uint8 node_id = FIX2UINT(rb_iv_get(self, "@node_id"));
    string name = Manager::Get()->GetNodeName(home_id, node_id);
    return rb_str_new2(name.c_str());
}

extern "C"
VALUE rb_node_get_location(VALUE self)
{
    uint32 home_id = FIX2UINT(rb_iv_get(self, "@home_id"));
    uint8 node_id = FIX2UINT(rb_iv_get(self, "@node_id"));
    string location = Manager::Get()->GetNodeLocation(home_id, node_id);
    return rb_str_new2(location.c_str());
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
VALUE rb_value_get_node_id(VALUE self)
{
    uint64 long_id = (uint64)FIX2LONG(rb_iv_get(self, "@value_id"));
    uint32 id1 = (uint32)(long_id & 0xFFFFFFFF);
    uint8 node_id = (uint8)((id1 & 0xff000000) >> 24);
    return INT2FIX(node_id);
}

extern "C"
VALUE rb_value_get_instance(VALUE self)
{
    uint64 long_id = (uint64)FIX2LONG(rb_iv_get(self, "@value_id"));
    uint32 id2 = (uint32)(long_id >> 32);
    uint8 instance = (uint8)(((id2 & 0xff000000)) >> 24);
    return INT2FIX(instance);
}

extern "C"
VALUE rb_value_get_index(VALUE self)
{
    uint64 long_id = (uint64)FIX2LONG(rb_iv_get(self, "@value_id"));
    uint32 id1 = (uint32)(long_id & 0xFFFFFFFF);
    uint8 index = (uint8)((id1 & 0x00000ff0) >> 4);
    return INT2FIX(index);
}

extern "C"
VALUE rb_value_get_type(VALUE self)
{
    uint64 long_id = (uint64)FIX2LONG(rb_iv_get(self, "@value_id"));
    uint32 id1 = (uint32)(long_id & 0xFFFFFFFF);
    ValueID::ValueType type = (ValueID::ValueType)(id1 & 0x0000000f);
    switch(type) {
    case ValueID::ValueType_Bool: return ID2SYM(rb_intern("bool"));
    case ValueID::ValueType_Byte: return ID2SYM(rb_intern("byte"));
    case ValueID::ValueType_Decimal: return ID2SYM(rb_intern("decimal"));
    case ValueID::ValueType_Int: return ID2SYM(rb_intern("int"));
    case ValueID::ValueType_List: return ID2SYM(rb_intern("list"));
    case ValueID::ValueType_Schedule: return ID2SYM(rb_intern("schedule"));
    case ValueID::ValueType_Short: return ID2SYM(rb_intern("short"));
    case ValueID::ValueType_String: return ID2SYM(rb_intern("string"));
    case ValueID::ValueType_Button: return ID2SYM(rb_intern("button"));
    case ValueID::ValueType_Raw: return ID2SYM(rb_intern("raw"));
    default: return INT2FIX(type);
    };
}

extern "C"
VALUE rb_value_get_genre(VALUE self)
{
    uint64 long_id = (uint64)FIX2LONG(rb_iv_get(self, "@value_id"));
    uint32 id1 = (uint32)(long_id & 0xFFFFFFFF);
    ValueID::ValueGenre genre = (ValueID::ValueGenre)((id1 & 0x00c00000) >> 22);
    switch(genre) {
    case ValueID::ValueGenre_Basic: return ID2SYM(rb_intern("basic"));
    case ValueID::ValueGenre_User: return ID2SYM(rb_intern("user"));
    case ValueID::ValueGenre_Config: return ID2SYM(rb_intern("config"));
    case ValueID::ValueGenre_System: return ID2SYM(rb_intern("system"));
    case ValueID::ValueGenre_Count: return ID2SYM(rb_intern("count"));
    default: return INT2FIX(genre);
    };
}

extern "C"
VALUE rb_value_get_command_class(VALUE self)
{
    uint64 long_id = (uint64)FIX2LONG(rb_iv_get(self, "@value_id"));
    uint32 id1 = (uint32)(long_id & 0xFFFFFFFF);
    uint8 command_class_id = (uint8)((id1 & 0x003fc000) >> 14);
    switch(command_class_id) {
    case COMMAND_CLASS_MAR: return ID2SYM(rb_intern("mar"));
    case COMMAND_CLASS_BASIC: return ID2SYM(rb_intern("basic"));
    case COMMAND_CLASS_VERSION: return ID2SYM(rb_intern("version"));
    case COMMAND_CLASS_BATTERY: return ID2SYM(rb_intern("battery"));
    case COMMAND_CLASS_WAKE_UP: return ID2SYM(rb_intern("wake_up"));
    case COMMAND_CLASS_CONTROLLER_REPLICATION: return ID2SYM(rb_intern("controller_replication"));
    case COMMAND_CLASS_SWITCH_MULTILEVEL: return ID2SYM(rb_intern("switch_multilevel"));
    case COMMAND_CLASS_SWITCH_ALL: return ID2SYM(rb_intern("switch_all"));
    case COMMAND_CLASS_SENSOR_BINARY: return ID2SYM(rb_intern("sensor_binary"));
    case COMMAND_CLASS_SENSOR_MULTILEVEL: return ID2SYM(rb_intern("sensor_multilevel"));
    case COMMAND_CLASS_SENSOR_ALARM: return ID2SYM(rb_intern("sensor_alarm"));
    case COMMAND_CLASS_ALARM: return ID2SYM(rb_intern("alarm"));
    case COMMAND_CLASS_MULTI_CMD: return ID2SYM(rb_intern("multi_cmd"));
    case COMMAND_CLASS_CLIMATE_CONTROL_SCHEDULE: return ID2SYM(rb_intern("climate_control_schedule"));
    case COMMAND_CLASS_CLOCK: return ID2SYM(rb_intern("clock"));
    case COMMAND_CLASS_ASSOCIATION: return ID2SYM(rb_intern("association"));
    case COMMAND_CLASS_CONFIGURATION: return ID2SYM(rb_intern("configuration"));
    case COMMAND_CLASS_MANUFACTURER_SPECIFIC: return ID2SYM(rb_intern("manufacturer_specific"));
    case COMMAND_CLASS_APPLICATION_STATUS: return ID2SYM(rb_intern("application_status"));
    case COMMAND_CLASS_ASSOCIATION_COMMAND_CONFIGURATION: return ID2SYM(rb_intern("association_command_configuration"));
    case COMMAND_CLASS_AV_CONTENT_DIRECTORY_MD: return ID2SYM(rb_intern("av_content_directory_md"));
    case COMMAND_CLASS_AV_CONTENT_SEARCH_MD: return ID2SYM(rb_intern("av_content_search_md"));
    case COMMAND_CLASS_AV_RENDERER_STATUS: return ID2SYM(rb_intern("av_renderer_status"));
    case COMMAND_CLASS_AV_TAGGING_MD: return ID2SYM(rb_intern("av_tagging_md"));
    case COMMAND_CLASS_BASIC_WINDOW_COVERING: return ID2SYM(rb_intern("basic_window_covering"));
    case COMMAND_CLASS_CHIMNEY_FAN: return ID2SYM(rb_intern("chimney_fan"));
    case COMMAND_CLASS_COMPOSITE: return ID2SYM(rb_intern("composite"));
    case COMMAND_CLASS_DOOR_LOCK: return ID2SYM(rb_intern("door_lock"));
    case COMMAND_CLASS_ENERGY_PRODUCTION: return ID2SYM(rb_intern("energy_production"));
    case COMMAND_CLASS_FIRMWARE_UPDATE_MD: return ID2SYM(rb_intern("firmware_update_md"));
    case COMMAND_CLASS_GEOGRAPHIC_LOCATION: return ID2SYM(rb_intern("geographic_location"));
    case COMMAND_CLASS_GROUPING_NAME: return ID2SYM(rb_intern("grouping_name"));
    case COMMAND_CLASS_HAIL: return ID2SYM(rb_intern("hail"));
    case COMMAND_CLASS_INDICATOR: return ID2SYM(rb_intern("indicator"));
    case COMMAND_CLASS_IP_CONFIGURATION: return ID2SYM(rb_intern("ip_configuration"));
    case COMMAND_CLASS_LANGUAGE: return ID2SYM(rb_intern("language"));
    case COMMAND_CLASS_LOCK: return ID2SYM(rb_intern("lock"));
    case COMMAND_CLASS_MANUFACTURER_PROPRIETARY: return ID2SYM(rb_intern("manufacturer_proprietary"));
    case COMMAND_CLASS_METER_PULSE: return ID2SYM(rb_intern("meter_pulse"));
    case COMMAND_CLASS_METER: return ID2SYM(rb_intern("meter"));
    case COMMAND_CLASS_MTP_WINDOW_COVERING: return ID2SYM(rb_intern("mtp_window_covering"));
    case COMMAND_CLASS_MULTI_INSTANCE_ASSOCIATION: return ID2SYM(rb_intern("multi_instance_association"));
    case COMMAND_CLASS_MULTI_INSTANCE: return ID2SYM(rb_intern("multi_instance"));
    case COMMAND_CLASS_NO_OPERATION: return ID2SYM(rb_intern("no_operation"));
    case COMMAND_CLASS_NODE_NAMING: return ID2SYM(rb_intern("node_naming"));
    case COMMAND_CLASS_NON_INTEROPERABLE: return ID2SYM(rb_intern("non_interoperable"));
    case COMMAND_CLASS_POWERLEVEL: return ID2SYM(rb_intern("powerlevel"));
    case COMMAND_CLASS_PROPRIETARY: return ID2SYM(rb_intern("proprietary"));
    case COMMAND_CLASS_PROTECTION: return ID2SYM(rb_intern("protection"));
    case COMMAND_CLASS_REMOTE_ASSOCIATION_ACTIVATE: return ID2SYM(rb_intern("remote_association_activate"));
    case COMMAND_CLASS_REMOTE_ASSOCIATION: return ID2SYM(rb_intern("remote_association"));
    case COMMAND_CLASS_SCENE_ACTIVATION: return ID2SYM(rb_intern("scene_activation"));
    case COMMAND_CLASS_SCENE_ACTUATOR_CONF: return ID2SYM(rb_intern("scene_actuator_conf"));
    case COMMAND_CLASS_SCENE_CONTROLLER_CONF: return ID2SYM(rb_intern("scene_controller_conf"));
    case COMMAND_CLASS_SCREEN_ATTRIBUTES: return ID2SYM(rb_intern("screen_attributes"));
    case COMMAND_CLASS_SCREEN_MD: return ID2SYM(rb_intern("screen_md"));
    case COMMAND_CLASS_SECURITY: return ID2SYM(rb_intern("security"));
    case COMMAND_CLASS_SENSOR_CONFIGURATION: return ID2SYM(rb_intern("sensor_configuration"));
    case COMMAND_CLASS_SILENCE_ALARM: return ID2SYM(rb_intern("silence_alarm"));
    case COMMAND_CLASS_SIMPLE_AV_CONTROL: return ID2SYM(rb_intern("simple_av_control"));
    case COMMAND_CLASS_SWITCH_BINARY: return ID2SYM(rb_intern("switch_binary"));
    case COMMAND_CLASS_SWITCH_TOGGLE_BINARY: return ID2SYM(rb_intern("switch_toggle_binary"));
    case COMMAND_CLASS_SWITCH_TOGGLE_MULTILEVEL: return ID2SYM(rb_intern("switch_toggle_multilevel"));
    case COMMAND_CLASS_THERMOSTAT_FAN_MODE: return ID2SYM(rb_intern("thermostat_fan_mode"));
    case COMMAND_CLASS_THERMOSTAT_FAN_STATE: return ID2SYM(rb_intern("thermostat_fan_state"));
    case COMMAND_CLASS_THERMOSTAT_HEATING: return ID2SYM(rb_intern("thermostat_heating"));
    case COMMAND_CLASS_THERMOSTAT_MODE: return ID2SYM(rb_intern("thermostat_mode"));
    case COMMAND_CLASS_THERMOSTAT_OPERATING_STATE: return ID2SYM(rb_intern("thermostat_operating_state"));
    case COMMAND_CLASS_THERMOSTAT_SETBACK: return ID2SYM(rb_intern("thermostat_setback"));
    case COMMAND_CLASS_THERMOSTAT_SETPOINT: return ID2SYM(rb_intern("thermostat_setpoint"));
    case COMMAND_CLASS_TIME_PARAMETERS: return ID2SYM(rb_intern("time_parameters"));
    case COMMAND_CLASS_TIME: return ID2SYM(rb_intern("time"));
    case COMMAND_CLASS_USER_CODE: return ID2SYM(rb_intern("user_code"));
    case COMMAND_CLASS_ZIP_ADV_CLIENT: return ID2SYM(rb_intern("zip_adv_client"));
    case COMMAND_CLASS_ZIP_ADV_SERVER: return ID2SYM(rb_intern("zip_adv_server"));
    case COMMAND_CLASS_ZIP_ADV_SERVICES: return ID2SYM(rb_intern("zip_adv_services"));
    case COMMAND_CLASS_ZIP_CLIENT: return ID2SYM(rb_intern("zip_client"));
    case COMMAND_CLASS_ZIP_SERVER: return ID2SYM(rb_intern("zip_server"));
    case COMMAND_CLASS_ZIP_SERVICES: return ID2SYM(rb_intern("zip_services"));
    default: return INT2FIX(command_class_id);
    }
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
    case ValueID::ValueType_List: break;
    case ValueID::ValueType_Schedule: break;
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

    case ValueID::ValueType_Button: break;
    case ValueID::ValueType_Raw: break;
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

    case ValueID::ValueType_List: break;
    case ValueID::ValueType_Schedule: break;

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
    case ValueID::ValueType_Button: break;
    case ValueID::ValueType_Raw: break;
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
    rb_define_method(rb_cNode, "type", (VALUE (*)(...))rb_node_type, 0);
    rb_define_method(rb_cNode, "listening_device?", (VALUE (*)(...))rb_node_is_listening_device, 0);
    rb_define_method(rb_cNode, "frequent_listening_device?", (VALUE (*)(...))rb_node_is_frequent_listening_device, 0);
    rb_define_method(rb_cNode, "beaming_device?", (VALUE (*)(...))rb_node_is_beaming_device, 0);
    rb_define_method(rb_cNode, "routing_device?", (VALUE (*)(...))rb_node_is_routing_device, 0);
    rb_define_method(rb_cNode, "security_device?", (VALUE (*)(...))rb_node_is_security_device, 0);
    rb_define_method(rb_cNode, "max_baud_rate", (VALUE (*)(...))rb_node_get_max_baud_rate, 0);
    rb_define_method(rb_cNode, "version", (VALUE (*)(...))rb_node_get_version, 0);
    rb_define_method(rb_cNode, "security", (VALUE (*)(...))rb_node_get_security, 0);
    rb_define_method(rb_cNode, "basic_type", (VALUE (*)(...))rb_node_get_basic_type, 0);
    rb_define_method(rb_cNode, "generic_type", (VALUE (*)(...))rb_node_get_generic_type, 0);
    rb_define_method(rb_cNode, "specific_type", (VALUE (*)(...))rb_node_get_specific_type, 0);
    rb_define_method(rb_cNode, "manufacturer_name", (VALUE (*)(...))rb_node_get_manufacturer_name, 0);
    rb_define_method(rb_cNode, "manufacturer_id", (VALUE (*)(...))rb_node_get_manufacturer_id, 0);
    rb_define_method(rb_cNode, "product_name", (VALUE (*)(...))rb_node_get_product_name, 0);
    rb_define_method(rb_cNode, "product_type", (VALUE (*)(...))rb_node_get_product_type, 0);
    rb_define_method(rb_cNode, "product_id", (VALUE (*)(...))rb_node_get_product_id, 0);
    rb_define_method(rb_cNode, "name", (VALUE (*)(...))rb_node_get_name, 0);
    rb_define_method(rb_cNode, "location", (VALUE (*)(...))rb_node_get_location, 0);

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
    rb_define_method(rb_cValue, "node_id", (VALUE (*)(...))rb_value_get_node_id, 0);
    rb_define_method(rb_cValue, "instance", (VALUE (*)(...))rb_value_get_instance, 0);
    rb_define_method(rb_cValue, "index", (VALUE (*)(...))rb_value_get_index, 0);
    rb_define_method(rb_cValue, "type", (VALUE (*)(...))rb_value_get_type, 0);
    rb_define_method(rb_cValue, "genre", (VALUE (*)(...))rb_value_get_genre, 0);
    rb_define_method(rb_cValue, "command_class", (VALUE (*)(...))rb_value_get_command_class, 0);

    rb_cNotification = rb_define_class_under(rb_cZwave, "Notification", rb_cObject);
}
