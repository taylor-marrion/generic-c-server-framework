/**
 * @file server_threaded.c
 * @brief Multithreaded TCP server with modular configuration and logging.
 *
 * This module defines the entry point for a generic multithreaded server that:
 * - Accepts concurrent TCP clients using detached threads
 * - Supports signal-based graceful shutdown
 * - Loads runtime configuration from CLI and config files
 * - Uses modular components for networking, logging, and threading
 *
 * This file includes:
 * - Signal handling for SIGINT, SIGTERM, SIGPIPE
 * - Server socket setup (IPv4/TCP only in threaded mode)
 * - Accept loop spawning detached handler threads
 */

#define _POSIX_C_SOURCE 200112L // Required for getaddrinfo, pthreads, etc.

/* Standard Library */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* POSIX System Calls */
#include <unistd.h>

/* Threading */
#include <pthread.h>
#include <stdatomic.h>

/* Signal Handling */
#include <signal.h>

/* Networking */
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h> // for shutdown()

#include "../include/log_lib.h"
#include "../include/net_lib.h"
#include "../include/server_utils.h"
#include "../include/thread_utils.h"

/* Global flag for termination (atomic for thread and signal safety) */
atomic_int terminate = 0;
atomic_int active_clients = 0;

/* Global mutex for shared resources */

/** 
 * Function Prototypes - 
 * All functions that utilize atomic variables are left here for now
*/
void global_cleanup_server(void);
void signal_handler(int signum);
void accept_loop(accept_loop_args_t *args);
void *handle_client_connection(void *arg);

#ifndef TEST
/**
 * @brief Main entry point for the multithreaded server.
 *
 * Initializes server configuration, logging, shared data structures, and
 * restore threads. Accepts concurrent TCP client connections and delegates
 * each to a detached handler thread for request-response processing.
 *
 * The server handles graceful shutdown via SIGINT or SIGTERM and performs
 * full cleanup of all allocated resources and open sockets.
 *
 * @param argc Number of arguments passed to the program.
 * @param argv Array of arguments passed to the program.
 * @return Exit status code (0 for success, non-zero for failure).
 */
int main(int argc, char *argv[])
{
    /* ----------------------------------------------
     * Step 0: Setup cleanup and signal handling
     * ---------------------------------------------- */
    atexit(global_cleanup_server);      // Automatic cleanup on normal exit

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, signal_handler);    // avoid crashes on broken pipe

    /* ----------------------------------------------
     * Step 1: Parse command-line arguments and/or configuration file
     * ---------------------------------------------- */
    server_config_t server_config = {0};

    if (parse_arguments_server(argc, argv, &server_config) < 0)
    {
        fprintf(stderr, "Failed to parse arguments.\n");
        return EXIT_FAILURE;
    }

    /* ----------------------------------------------
     * Step 2: Initialize logging with parsed config
     * ---------------------------------------------- */
    FILE *log_fp = NULL;
    if (!server_config.log_to_stderr && server_config.log_file[0] != '\0')
    {
        log_fp = fopen(server_config.log_file, "a");
        if (!log_fp)
        {
            fprintf(stderr, "[!] Failed to open log file '%s'. Using stderr.\n",
                    server_config.log_file);
        }
    }

    log_init(parse_log_level(server_config.log_level),
             server_config.log_to_stderr ? NULL : log_fp);

    LOG_INFO("Server configuration loaded from: %s",
             server_config.source_path[0] ? server_config.source_path : "(unknown)");
    log_server_config(&server_config);

    /* ----------------------------------------------
     * Step 3: Globals
     * ---------------------------------------------- */
     // Initialize top-level resources and track startup status
    int server_socket = -1;
    int status = EXIT_FAILURE;
    accept_loop_args_t *accept_loop_args = NULL;

    if (server_config.enable_udp) 
    {
        LOG_FATAL("UDP is not supported in threaded mode.");
        LOG_FATAL("How did you even do that?");
        goto main_cleanup;
    }

    /* ----------------------------------------------
     * Step 4: Business logic & project-specifics
     * ---------------------------------------------- */
    
    // Resolve data file paths
    // Initialize data structures w/ threads
    // Join threads
    // Create pool of worker threads


    /* ----------------------------------------------
     * Step 5: Setup server socket
     * ---------------------------------------------- */
    LOG_INFO("Launching generic server...");
    server_socket = setup_server_socket(&server_config);
    if (server_socket == -1) 
    {
        LOG_FATAL("Failed to initialize server socket: %s", strerror(errno));
        goto main_cleanup;
    }

    accept_loop_args = malloc(sizeof(accept_loop_args_t));
    if (accept_loop_args == NULL) {
        LOG_FATAL("Failed to allocate memory for accept_loop_args.");
        goto main_cleanup;
    }

    accept_loop_args->server_socket     = server_socket;

    /* ----------------------------------------------
     * Step 6: Main server accept loop
     * ---------------------------------------------- */
    accept_loop(accept_loop_args);

    LOG_INFO("Shutdown signal processed. Proceeding to cleanup...");
    status = EXIT_SUCCESS;

    /* ----------------------------------------------
     * Step 7: Cleanup and shutdown
     * ---------------------------------------------- */
     // Ensure graceful shutdown of socket and memory cleanup
main_cleanup:
    LOG_INFO("Cleaning up and exiting...");

    if (accept_loop_args != NULL) 
    {
        free(accept_loop_args);
        accept_loop_args = NULL;
        LOG_DESTROY("Freed accept loop args.");
    }

    if (server_socket != -1) 
    {
        shutdown(server_socket, SHUT_RDWR);
        close(server_socket);
    }

    LOG_INFO("Main() exiting with status %d", status);
    return status;
} // end main
#endif


/**
 * @brief Performs global server cleanup on exit.
 *
 * This is registered via atexit() and ensures any 
 * resources are properly released before shutdown.
 */
void global_cleanup_server(void) 
{
    if (atomic_load(&terminate))
    {
        LOG_INFO("Terminated by signal %d", atomic_load(&terminate));
    }

    destroy_detached_thread_attr();

    while (atomic_load(&active_clients) > 0)
    {
        LOG_INFO("Waiting for %d clients to disconnect...",
                atomic_load(&active_clients));
        sleep(2);
    }

    LOG_DESTROY("All clients disconnected. Final cleanup complete.");
    LOG_DESTROY("Server shut down gracefully.");
} // end global_cleanup


/**
 * @brief Signal handler for gracefully terminating the program.
 *
 * This function handles signals such as SIGINT by:
 * - Logging an interrupt message using async-signal-safe functions.
 * - Atomically setting the `terminate` flag to the signal number, allowing
 *   the main program or threads to detect the termination request.
 *
 * @param signum The signal number received (e.g., SIGINT, SIGTERM).
 */
void signal_handler(int signum)
{
    const char *msg = NULL;

    switch (signum)
    {
        case SIGINT:
            msg = "\n\n***SIGINT received. Exiting program...***\n";
            write(STDERR_FILENO, msg, strlen(msg)); // async-signal-safe
            atomic_store(&terminate, signum); // Signal safe and thread safe
            break;
        case SIGTERM:
            msg = "\n\n***SIGTERM received. Exiting program...***\n";
            write(STDERR_FILENO, msg, strlen(msg)); // async-signal-safe
            atomic_store(&terminate, signum); // Signal safe and thread safe
            break;
        case SIGPIPE:
            msg = "\n\n***SIGPIPE received. "
            "Client disconnected unexpectedly.***\n";
            write(STDERR_FILENO, msg, strlen(msg)); // async-signal-safe
            break;
        default:
            msg = "\n\n***Unhandled signal received.***\n";
            write(STDERR_FILENO, msg, strlen(msg)); // async-signal-safe
            break;
    }
} // end signal_handler


/**
 * @brief Main accept loop that handles incoming client connections.
 *
 * This function continuously listens for and accepts new TCP client connections
 * on the provided server socket. For each connection, it initializes a
 * `client_handler_args_t` structure with shared state (e.g., data structures,
 * mutexes, file paths) and spawns a detached thread to handle the client using
 * `handle_client_connection()`.
 *
 * If allocation or thread creation fails, the client socket is closed and the
 * error is logged. The loop exits cleanly when global `terminate` flag is set.
 *
 * @param args Pointer to `accept_loop_args_t` structure containing the server
 *             socket and shared resources needed by each client handler.
 */
void accept_loop(accept_loop_args_t *args) 
{
    int server_fd = args->server_socket;

    LOG_INFO("accept_loop started.");

    while (!atomic_load(&terminate)) 
    {
        struct sockaddr_storage client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);

        if (client_fd == -1) 
        {
            if (errno == EINTR) continue;
            LOG_WARNING("accept failed: %s", strerror(errno));
            continue;
        }

        atomic_fetch_add(&active_clients, 1);
        LOG_CREATE("Accepted new client connection (fd=%d).", client_fd);

        client_handler_args_t *handler_args = malloc(
                    sizeof(client_handler_args_t));
        if (handler_args == NULL) 
        {
            LOG_ERROR("Failed to allocate client_handler_args.");
            close(client_fd);
            atomic_fetch_sub(&active_clients, 1);
            continue;
        }

        *handler_args = (client_handler_args_t) {
            .client_socket    = client_fd,
        };

        pthread_t tid;
        if (create_detached_thread(
            &tid, handle_client_connection, handler_args) != 0) 
        {
            LOG_ERROR("Failed to create handler thread.");
            close(client_fd);
            free(handler_args);
            atomic_fetch_sub(&active_clients, 1);
        }
        LOG_DEBUG("Handler thread launched for client (fd=%d).", client_fd);
    }

    LOG_DESTROY("accept_loop terminating. Not accepting new connections.");
    return;
} // end accept_loop


void *handle_client_connection(void *arg) 
{
    client_handler_args_t *args = (client_handler_args_t *)arg;
    int client_fd = args->client_socket;

    LOG_INFO("Client handler started (fd=%d)", client_fd);

    while (1) 
    {
        int keep_alive = echo_client_message(client_fd);

        if (!keep_alive) 
        {
            LOG_RECV("Client [%d] disconnected or terminated connection.", 
                client_fd);
            break;
        }
    }

    close(args->client_socket);
    atomic_fetch_sub(&active_clients, 1);
    free(args);

    LOG_DESTROY("Client handler finished for fd=%d.", client_fd);
    return NULL;
} // end handle_client_connection
