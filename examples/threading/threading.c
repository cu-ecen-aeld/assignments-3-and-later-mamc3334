#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{

    // TODO: wait, obtain mutex, wait, release mutex as described by thread_data structure
    // hint: use a cast like the one below to obtain thread arguments from your parameter
    //struct thread_data* thread_func_args = (struct thread_data *) thread_param;
    
    struct thread_data* thread_func_args = (struct thread_data *) thread_param; // obtain thread args
    
    if( thread_func_args == NULL ) 
    {
        ERROR_LOG("threadfunc received null thread_param\n");
        pthread_exit(NULL);
    }

    int rc; // reuse for return vals
    thread_func_args->thread_complete_success = true; // success unless fail
    
    rc = usleep(thread_func_args->sleep_before_lock_ms * 1000); //sleep before lock

    if( rc != 0 ) 
    {
	// sleep failed
        ERROR_LOG("usleep returned %d\n", rc);
        thread_func_args->thread_complete_success = false;
    }
    else
    {
        rc = pthread_mutex_lock(thread_func_args->mutex);

        if( rc != 0 )
        {
	    // lock failed
            ERROR_LOG("pthread_mutex_lock returned %d\n", rc);
            thread_func_args->thread_complete_success = false;
        }
        else // return if lock fails
        {
            rc = usleep(thread_func_args->sleep_after_lock_ms * 1000); // sleep after lock

            if( rc != 0 )
            {
                ERROR_LOG("usleep after lock returned %d\n", rc);
                thread_func_args->thread_complete_success = false;
            }

            rc = pthread_mutex_unlock(thread_func_args->mutex);
            
            if( rc != 0 )
            {
		// unlock failed
                ERROR_LOG("pthread_mutex_unlock returned %d\n", rc);
                thread_func_args->thread_complete_success = false;
            }
        }
    }
    
    return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
    /**
     * TODO: allocate memory for thread_data, setup mutex and wait arguments, pass thread_data to created thread
     * using threadfunc() as entry point.
     *
     * return true if successful.
     *
     * See implementation details in threading.h file comment block
     */

    struct thread_data* tdata = malloc(sizeof(struct thread_data)); // malloc memory
    
    if( tdata != NULL )
    {
	// memory available
        tdata->mutex = mutex;
        tdata->sleep_before_lock_ms = wait_to_obtain_ms;
        tdata->sleep_after_lock_ms = wait_to_release_ms;
        tdata->thread_complete_success = false;

        int rc = pthread_create(thread, NULL, threadfunc, (void *)tdata); //create thread using threadfunc()
        
        if( rc != 0 ) 
        {
	    //create failed
            ERROR_LOG("pthread_create returned %d\n", rc);
            free(tdata); // free meory

            return false;
        }
        else 
        {
            return true;
        }
    }  
    
    return false;
}

