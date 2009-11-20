/* threading.c */

/* Firtree - A generic image processing library
 * Copyright (C) 2009 Rich Wareham <richwareham@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.    See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA    02110-1301, USA
 */

#include <glib.h>

#include "threading.h"
#include "system-info.h"

typedef struct {
    GFunc       func;       /* A function to call to do the work. */
    gpointer    user_data;  /* The user data parameter giving the context of the work. */

    gint        task_count; /* A count to decrement when a task has completed. */
    
    /* A condition to signal when all tasks have been completed (task_count <= 0) */
    GCond*      completion_cond;
    GMutex*     completion_mutex;
} ThreadingWorker;

typedef struct {
    ThreadingWorker*    worker; /* The worker to use for this task. */
    gpointer            data;   /* What to pass to 'data' */
} ThreadingTask;

static void
_worker_func(gpointer data, gpointer user_data)
{
    ThreadingTask* task = (ThreadingTask*)data;

    task->worker->func(task->data, task->worker->user_data);

    if(g_atomic_int_dec_and_test(&task->worker->task_count)) {
        /* This was the last task, signal completion */
        g_mutex_lock(task->worker->completion_mutex);
        g_cond_broadcast(task->worker->completion_cond);
        g_mutex_unlock(task->worker->completion_mutex);
    }
}

GThreadPool* 
_threading_get_global_pool()
{
    static GThreadPool* pool = NULL;

    /* If we already created the global pool, return it. */
    if(pool) 
        return pool;

    GError* error = NULL;

    /* Create an exclusive thread pool to do the work. */
    pool = g_thread_pool_new(
            _worker_func, 
            NULL, 
            system_info_cpu_cores(),
            TRUE,
            &error);

    g_assert(error == NULL);

    return pool;
}

typedef struct {
    ThreadingApplyFunc func;
    gpointer user_data;
} ThreadingApplyContext;

static void
_apply_task_func(gpointer data, gpointer user_data)
{
    ThreadingApplyContext* context = (ThreadingApplyContext*)user_data;
    context->func(GPOINTER_TO_UINT(data), context->user_data);
}

void
threading_apply(guint count, ThreadingApplyFunc func, gpointer user_data)
{
    guint i;

    /* Initialise a worker structure for this process. */
    GCond* completion_cond = g_cond_new();
    GMutex* completion_mutex = g_mutex_new();
    ThreadingApplyContext context = { func, user_data };
    ThreadingWorker worker = { 
        _apply_task_func, &context, count, 
        completion_cond, completion_mutex };

    /* Allocate an array to hold the task structures. */
    GArray* task_array = g_array_sized_new(FALSE, FALSE, 
            sizeof(ThreadingTask), count);

    /* Push all the tasks onto the pool */
    GThreadPool* pool = _threading_get_global_pool();
    for(i=0; i<count; ++i) {
        ThreadingTask task = { &worker, GUINT_TO_POINTER(i) };
        g_array_append_val(task_array, task);
        g_thread_pool_push(pool, 
                &(g_array_index(task_array, ThreadingTask, i)), 
                NULL);
    }

    /* Wait for the tasks to complete */
    g_mutex_lock(completion_mutex);
    while(worker.task_count > 0) {
        g_cond_wait(completion_cond, completion_mutex);
    }
    g_mutex_unlock(completion_mutex);

    /* Tidy up after ourselves. */
    g_array_free(task_array, TRUE);
    g_mutex_free(completion_mutex);
    g_cond_free(completion_cond);
}

/* vim:sw=4:ts=4:et:cindent
 */
