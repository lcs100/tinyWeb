#include "http_conn.h"
#include "../log/log.h"
#include <map>



const char* doc_root = "/home/lcs100/tinyWeb/root";
const char* error_400_title = "Bad Request";
const char* error_400_form = "Your request has bad syntax or is inherently impossible to satisfy.\n";
const char* error_403_title = "Forbidden";
const char* error_403_form = "You do not have permission to get file from this server.\n";
const char* error_404_title = "Not Found";
const char* error_404_form = "The requested file was not found on this server.\n";
const char* error_500_title = "Internal Error";
const char* error_500_form = "There was an unusual problem serving the request file.\n";

unorderd_map<string, string> users;

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

bool http_conn::add_response(const char* format, ...){
    if(m_write_idx >= WRITE_BUFFER_SIZE) return false;

    va_list arg_list;
    
    va_start(arg_list, format);

    int len = vsnprintf(m_write_buf + m_write_idx, WRITE_BUFFER_SIZE - 1 - m_write_idx, format, arg_list);

    if(len >= (WRITE_BUFFER_SIZE - 1 - m_write_idx)){
        va_end(arg_list);
        return false;
    }
    
    m_write_idx += len;
    va_end(arg_list);

    return true;
}


bool http_conn::add_status_line(int status, const char* title){
    return add_response("%s %d %s\r\n", "HTTP/1.1", status, title);
}

bool http_conn::add_headers(int content_len){
    add_content_length(content_len);
    add_linger();
    add_blank_line();
}

bool http_conn::add_content_length(int content_len){
    return add_response("Content-Length:%d\r\n", content_len);
}

bool http_conn::add_content_type(){
    return add_response("Content-Type:%s\r\n", "text/html");
}

bool http_conn::add_linger(){
    return add_response("Connection:%s\r\n", (,_linger == true)?"keep-alive":"close");
}




bool http_conn::process_write(HTTP_CODE ret){
    switch(ret){
        case INTERNAL_ERROR:{
            add_status_line(500, error_500_title);
            add_headers(strlen(error_500_form));
            if( !add_content(error_500_form)) return false;
            break;
        }
        case 
    }
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

http_conn::HTTP_CODE http_conn::parse_headers(char* text){
    if(text[0] == '\0'){
        if(m_content_length != 0){
            m_check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    }
    else if(strncasecmp(text, "Connection:", 11) == 0){
        text += 11;
        text += strspn(text, " \t");
        if(strcasecmp(text, "keep-alive") == 0) m_linger = true;
    }
    else if(strncasecmp(text, "Content-length:", 15) == 0){
        text += 15;
        text += strspn(text, " \t");
        m_content_length = atol(text);
    }
    else if(strncasecmp(text, "Host:", 5) == 0){
        text += 5;
        text += strspn(text, " \t");
        m_host = text;
    }
    else cout << "oop! unknown header: " << text << endl;
    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::parse_content(char* text){
    if(m_read_length >= (m_content_length + m_curret_idx)){
        text[m_content_length] = '\0';
        m_string = text;
        return GET_REQUEST;
    }
    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::do_request(){
    strcpy(m_file, doc_root);
    int len = strlen(doc_root);
    const char* p = strrchr(m_url, '/');

    if(cgi == 1 && (*(p+1) == '2' || *(p+1) == '3')){
        //
    }

    if(*(p+1) == '0'){
        char* m_url_real = (char*)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/register.html");

        strncpy(m_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else if( *(p+1) == '1'){
        char* m_url_real = (char*)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/log.html");

        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));
        free(m_url_real);
    }
    else strncpy(m_real_file + len, m_url, FILENAME_LEN - len - 1);

    if(stat(m_real_file, &m_file_stat) < 0) return NO_REQUEST;

    if(!(m_file_stat.st_mode & S_IROTH)) return FORBIDDEN_REQUEST;
    if(S_ISDIR(m_file_stat.st_mode)) return BAD_REQUEST;

    int fd = open(m_real_file, O_RDONLY);
    m_file_address = (char*)mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);

    close(fd);

    return FILE_REQUEST;
}


http_conn::HTTP_CODE http_conn::process_read(){

    //initialize HTTP and others
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    char* text = 0;

    while((m_check_state == CHECK_STATE_CONTENT && line_status == LINE_OK) || ((line_status == parse_line()) == LINE_OK)){
        LINE_STATUS line_status = LINE_OK;
        HTTP_CODE ret = NO_REQUEST;
        char* text = 0;

        text = get_line();
        m_finished_num = m_curret_idx;
        LOG_INFO("%s", text);
        Log::get_instance()->flush();
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