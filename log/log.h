#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <iostream>
#include <string>
#include <stdarg.h>
#include <pthread.h>
#include "block_queue.h"

using namespace std;

static pthread_mutex_t* m_mutex = new pthread_mutex_t;

class Log{
    static Log* get_instance(){
        pthread_mutex_lock(m_mutex);
        static Log instance;
        pthread_mutex_unlock(m_mutex);
        return &instance;
    }

    static void* flush_log_thread(void* args){
        Log::get_instance()->async_write_log();
    }

    bool init(const cahr* file_name, int log_buf_size = 8192, int max_lines = 5000000, int max_queue_size = 0);

    void write_log(int level, const cahr* format, ...);

    void flush(void);



private:
    char dir_name[128];
    char log_file[128];
    int m_max_lines;
    int m_log_buf_size;
    long long m_count;
    int m_today;
    FILE* m_fp;
    char* m_buf;
    block_queue<string>* m_log_queue;
    bool m_is_async;
}