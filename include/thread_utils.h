/**
 * @file thread_utils.h
 * @brief Utility functions for managing thread attributes.
 */

#ifndef THREAD_UTILS_H
#define THREAD_UTILS_H

#include <pthread.h>


/**
 * @brief Creates a detached pthread using a reusable static attribute.
 *
 * This function wraps pthread_create using a statically initialized
 * detached thread attribute. The attribute is initialized once and reused
 * for all subsequent detached thread spawns in the program.
 *
 * @param thread Pointer to the pthread_t object to initialize.
 * @param start_routine Thread function to run.
 * @param arg Argument passed to the thread function.
 * @return 0 on success, or a pthread error code on failure.
 */
int create_detached_thread(
    pthread_t *thread, 
    void *(*start_routine)(void *), 
    void *arg);


/**
 * @brief Destroys the reusable thread attribute if initialized.
 */
void destroy_detached_thread_attr(void);


#endif // THREAD_UTILS_H
 