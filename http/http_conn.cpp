#include "http_conn.h"
#include "../log/log.h"



char* get_line(){
    return m_read_buf + m_start_line;
}



bool http_conn::read_once(){
    if(m_read_idx >= READ_BUFFER_SIZE) return false;

    int bytes_read = 0;
    while(true){
        bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
        if(bytes_read == -1){
            if(errno == EAGAIN || errno == EWOULDBLOCK) break;
            return false;
        }
        else if(bytes_read == 0) return false;

        m_read_idx += bytes_read;
    }

    return true;
}

void http_conn::process(){
    HTTP_CODE read_ret = process_read();

    if(read_ret == NO_REQUEST){
        modfd(m_epollfd, m_sockfd, EPOLLIN);
        return ;
    }

    bool write_ret = process_write(read_ret);
    if(!write_ret){
        close_conn();
    }

    modfd(m_epollfd, m_sockfd, EPOLLOUT);
}

http_conn::LINE_STATUS http_conn::parse_line(){
    char tmp;

    for(;m_curret_idx < m_read_idx; m_curret_idx++){
        tmp = m_read_buf[m_curret_idx];
        if(tmp == '\r'){
            if((m_curret_idx + 1) == m_read_idx) return LINE_OPEN;
            else if(m_read_buf[m_curret_idx + 1] == '\n'){
                m_read_buf[m_curret_idx++] = '\0';
                m_read_buf[m_curret_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
        else if(tmp == '\n'){
            if(m_curret_idx > 1 && m_read_buf[m_curret_idx - 1] == '\r'){
                m_read_buf[m_curret_idx - 1] = '\0';
                m_read_buf[m_curret_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }

    return LINE_OPEN;
}


http_conn::HTTP_CODE http_conn::process_read(){

    //initialize HTTP and others
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    char* text = 0;

    while((m_check_state == CHECK_STATE_CONTENT && line_status == LINE_OK) || ((line_status == parse_line()) == LINE_OK)){
        



    }

    switch(m_check_state){
        case CHECK_STATE_REQUESTLINE: {
            ret = parse_request_line(text);
            if(ret == BAD_REQUEST) return BAD_REQUEST;
            break;
        }
        case CHECK_STATE_HEADER: {
            ret = parse_headers(text);
            if(ret == BAD_REQUEST) return BAD_REQUEST;
            else if( ret == GET_REQUEST) return do_request();
            break;
        }
        case CHECK_STATE_CONTENT: {
            ret = parse_content(text);

            if( ret == GET_REQUEST) return do_request();

            line_status = LINE_OPEN;
            break;
        }
        default: return INTERNAL_ERROR;
    }

    return NO_REQUEST;
}