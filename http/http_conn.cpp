#include "http_conn.h"
#include "../log/log.h"



char* get_line(){
    return m_read_buf + m_finished_num;
}



bool http_conn::read_once(){
    if(m_read_length >= READ_BUFFER_SIZE) return false;

    int bytes_read = 0;
    while(true){
        bytes_read = recv(m_sockfd, m_read_buf + m_read_length, READ_BUFFER_SIZE - m_read_length, 0);
        if(bytes_read == -1){
            if(errno == EAGAIN || errno == EWOULDBLOCK) break;
            return false;
        }
        else if(bytes_read == 0) return false;

        m_read_length += bytes_read;
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

void addfd(int epollfd, int fd, bool one_shot){
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    if(one_shot) event.events |= EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}


void http_conn::init(int sockfd, sockaddr_in &addr){
    m_sockfd = sockfd;
    m_address = addr;

    addfd(m_epollfd, sockfd, true);
    m_user_count++;
    init();
}


void http_conn::init(){
    mysql = NULL;
    bytes_to_send = 0;
    bytes_have_send = 0;
    m_check_state = CHECK_STATE_REQUESTLINE;
    m_linger = false;
    m_method = GET;
    m_url = 0;
    m_version = 0;
    m_content_length = 0;
    m_host = 0;
    m_finished_num = 0;
    m_curret_idx = 0;
    m_read_length = 0;
    m_write_idx = 0;
    cgi = 0;
    
    memset(m_read_buf, '\0', READ_BUFFER_SIZE);
    memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);
    memset(m_file, '\0', FILENAME_LEN);
}

http_conn::HTTP_CODE http_conn::parse_request_line(char* text){
    m_url = strpbrk(text, " \t");

    if(!m_url) return BAD_REQUEST;

    *m_url++='\0';

    char* method = text;
    if(strcasecmp(method, "GET") == 0) m_method = GET;
    else if(strcasecmp(method, "POST") == 0){
        m_method = POST;
        cgi = 1;
    }
    else return BAD_REQUEST;

    m_url += strspn(m_url, " \t");
    
    m_version = strpbrk(m_url, " \t");
    if(!m_version) return BAD_REQUEST;
    *m_version++ = '\0';
    m_version += strspn(m_version, " \t");

    if(strcasecmp(m_verison, "HTTP/1.1") != 0) return BAD_REQUEST;

    if(strncasecmp(m_url, "https://", 7) == 0){
        m_url += 7;
        m_url = strchr(m_url, '/');
    }

    if(strncasecmp(m_url, "https://", 8) == 0){
        m_url += 8;
        m_url = strchr(m_url, '/');
    }

    if(!m_url || m_url[0] != '/') return BAD_REQUEST;

    if(strlen(m_url) == 1) strcat(m_url, "judge.html");
    
    m_check_state = CHECK_STATE_HEADER;
    return NO_REQUEST;
}