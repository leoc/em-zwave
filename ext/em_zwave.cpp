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
void* worker_thread(void* unused) {
    int i;
    for(i = 0; i < 500; i++) {
        usleep(20000);
        // printf("C-p: pushing notification %i\n", i);

        notification_t* notification = (notification_t*)malloc(sizeof(notification_t));
        notification->number = i;

        // push the notification into the notification queue
        pthread_mutex_lock(&g_notification_mutex);
        g_notification_queue_push(notification);
        pthread_mutex_unlock(&g_notification_mutex);

        // signal waiting ruby thread, that a notification was pushed
        pthread_cond_signal(&g_notification_cond);
    }
}


extern "C"
VALUE start_worker(void*) {
    pthread_t worker;
    pthread_create(&worker, 0, &worker_thread, NULL);
    pthread_join(worker, NULL);
    return Qnil;
}

extern "C"
void stop_worker(void*) {
    // shutdown the zwave manager correctly
}

extern "C"
VALUE em_zwave_worker_thread(void* unused) {
    rb_thread_blocking_region(start_worker, NULL, stop_worker, NULL);
    return Qnil;
}

extern "C"
VALUE wait_for_notification(void* w) {
    waiting_notification_t* waiting = (waiting_notification_t*)w;
    pthread_mutex_lock(&g_notification_mutex);
    while (waiting->abort == false && (waiting->notification = g_notification_queue_pop()) == NULL) {
        pthread_cond_wait(&g_notification_cond, &g_notification_mutex);
    }
    pthread_mutex_unlock(&g_notification_mutex);
    return Qnil;
}

extern "C"
void stop_waiting_for_notification(void* w) {
    waiting_notification_t* waiting = (waiting_notification_t*)w;
    pthread_mutex_lock(&g_notification_mutex);
    waiting->abort = true;
    pthread_cond_signal(&g_notification_cond);
    pthread_mutex_unlock(&g_notification_mutex);
}

/* This thread loops continuously, waiting for callbacks happening in the C thread. */
extern "C"
VALUE em_zwave_event_thread(void* args) {
    VALUE zwave = (VALUE)args;

    waiting_notification_t waiting = {
        .notification = NULL,
        .abort = false
    };

    while(waiting.abort == false) {
        rb_thread_blocking_region(wait_for_notification, &waiting,
                                  stop_waiting_for_notification, &waiting);

        VALUE notification = rb_obj_alloc(rb_cNotification);
        VALUE init_argv[1];
        init_argv[0] = INT2NUM(waiting.notification->number);
        rb_obj_call_init(notification, 1, init_argv);

        rb_funcall(zwave, id_push_notification, 1, notification);
    }

    return Qnil;
}

// starts the open zwave thread
extern "C"
VALUE rb_zwave_initialize_zwave(VALUE self) {
    rb_thread_create((VALUE (*)(...))em_zwave_worker_thread, NULL);
    rb_thread_create((VALUE (*)(...))em_zwave_event_thread, (void*)self);
    return Qnil;
}

extern "C"
void Init_emzwave() {
    id_push_notification = rb_intern("push_notification");

    rb_mEm = rb_define_module("EventMachine");

    rb_cZwave = rb_define_class_under(rb_mEm, "Zwave", rb_cObject);
    rb_define_method(rb_cZwave, "initialize_zwave", (VALUE (*)(...))rb_zwave_initialize_zwave, 0);

    rb_cNotification = rb_define_class_under(rb_cZwave, "Notification", rb_cObject);
}
