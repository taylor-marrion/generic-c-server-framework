/**
 * @file thread_utils.c
 * @brief Utilities for creating detached threads with reusable pthread_attr_t.
 */

#include <pthread.h>
#include <errno.h>
#include <stdlib.h>

#include "../include/thread_utils.h"
#include "../include/log_lib.h"


/* Static reusable detached thread attributes */
static pthread_attr_t g_detached_attr;
static int g_attr_initialized = 0;
static pthread_once_t attr_once = PTHREAD_ONCE_INIT;
static pthread_mutex_t attr_mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * @brief Initializes the detached pthread attribute object (only once).
 * @internal
 */
static void init_detached_thread_attr_once(void)
{
    if (pthread_attr_init(&g_detached_attr) != 0 ||
        pthread_attr_setdetachstate(&g_detached_attr, PTHREAD_CREATE_DETACHED) != 0)
    {
        LOG_FATAL("Failed to initialize reusable detached thread attributes.");
        exit(EXIT_FAILURE);
    }

    g_attr_initialized = 1;
    LOG_DEBUG("Detached thread attribute initialized.");
} // end init_detached_thread_attr_once


/**
 * @brief Creates a detached thread using reusable pthread_attr_t.
 *
 * @param thread Pointer to pthread_t for the created thread.
 * @param start_routine Function pointer to the thread start routine.
 * @param arg Pointer to argument to be passed to the thread routine.
 * @return 0 on success, or error code from pthread_create on failure.
 */
int create_detached_thread(
    pthread_t *thread, 
    void *(*start_routine)(void *), 
    void *arg)
{
    pthread_once(&attr_once, init_detached_thread_attr_once);

    return pthread_create(thread, &g_detached_attr, start_routine, arg);
} // end create_detached_thread


/**
 * @brief Destroys the reusable thread attribute if initialized.
 */
void destroy_detached_thread_attr(void)
{
    pthread_mutex_lock(&attr_mutex);
    if (g_attr_initialized) 
    {
        pthread_attr_destroy(&g_detached_attr);
        g_attr_initialized = 0;
        LOG_DEBUG("Detached thread attribute destroyed.");
    }
    pthread_mutex_unlock(&attr_mutex);
} // end destroy_detached_thread_attr
