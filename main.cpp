http_conn* users = new http_conn[MAX_FD];

int connfd = accept(listenfd, (struct sockaddr*)&client_address, &client_addrlength);

sers[connfd].init(connfd, client_address);

if(events[i].events & EPOLLIN){
    if(users[sockfd].read_once()){
        pool->append(users + sockfd);
    }
}

