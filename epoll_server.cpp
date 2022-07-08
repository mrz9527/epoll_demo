// author : zhoukang
// date   : 2022-05-30 15:18:12


#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/epoll.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

// arg主要指向myEvent对象
typedef void (*callback)(int fd, int events, void* arg);
struct myEvent {
    int fd = 0;
    int  events = 0;
    void* arg = 0;
    char buf[1024] {0};
    int buflen = 0;
    callback cb = 0;

    void reset() {
        fd = 0;
        events = 0;
        arg = 0;
        bzero(buf, 1024);
        buflen = 0;
        cb = 0;
    }
};

#define MY_EVENT_SIZE 1024
myEvent myEv[MY_EVENT_SIZE + 1];

int epfd = -1;
void epoll_add(int fd, int events, myEvent *myev, callback cb);
void epoll_delete(int epfd, int fd, int events, myEvent *myev);
void epoll_mod(int fd, int events, myEvent* myev, callback cb);
void init_send(int fd, int events, void* arg);

/* 初始化服务端sock
 * 创建sock、设置端口可重用、绑定、监听
 * */
int init_server_sock(int port) {
    // 创建listen socket
    int lfd = socket(AF_INET, SOCK_STREAM, 0);
    if(lfd < 0) {
        perror("");
        return -1;
    }

    // 设置端口可重用
    bool nopt = true;
    int ret = setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, (const void*)&nopt, sizeof(int));
    if(ret == -1) {
        close(lfd);
        perror("");
        return -1;
    }

    // 绑定
    sockaddr_in server_addr;
    bzero(&server_addr,sizeof(server_addr));
    server_addr.sin_family=AF_INET;
    server_addr.sin_addr.s_addr=htonl(INADDR_ANY);
    server_addr.sin_port=htons(port);

    ret = bind(lfd, reinterpret_cast<const sockaddr *>(&server_addr), sizeof(server_addr));
    if(ret == -1) {
        perror("");
        close(lfd);
        return -1;
    }

    ret = listen(lfd, 5);
    if(ret == -1) {
        perror("");
        close(lfd);
        return -1;
    }

    return lfd;
}

void init_read(int fd, int events, void* arg) {
    myEvent * myev = (myEvent*)arg;
    memset(myev->buf, 0, sizeof(myev->buf));
    myev->buflen = read(fd, myev->buf, sizeof(myev->buf));
    if(myev->buflen <= 0) {
        printf("客户端已关闭，退出\n");
        close(fd);
        epoll_delete(epfd, fd, EPOLLIN, myev); // 删除，为啥还要设置events为EPOLLIN?
    } else {
        printf("接收消息:%s\n", myev->buf);
        epoll_mod(fd, EPOLLOUT, myev, init_send);
    }
}


void init_send(int fd, int events, void* arg) {
    myEvent* myev = (myEvent*)arg;

    strcat(myev->buf, "ok");

    printf("发送消息:%s\n", myev->buf);
    write(fd, myev->buf, myev->buflen);

    epoll_mod(fd, EPOLLIN, myev, init_read);
}

void init_accept(int fd, int events, void* arg) {
    int cfd = accept(fd, nullptr, nullptr); //todo: cfd <-1 , = 0 怎么处理
    if( cfd <= 0) {
       perror("");
       return;
    }


    printf("客户端上线\n");

    int i = 0;
    for (i = 0; i < MY_EVENT_SIZE; ++i) {
        if(myEv[i].fd == 0) {
            break;
        }
    }

   epoll_add(cfd, EPOLLIN, &myEv[i], init_read);
}


// 监听的lfd : epoll_ctl(epfd, EPOLL_CTL_ADD, lfd, &ev);
void epoll_add(int fd, int events, myEvent *myev, callback cb) {
    myev->fd = fd;
    myev->events = events;
    myev->cb = cb;
    myev->arg = myev;

    struct epoll_event ev;
    ev.events = events;
    ev.data.ptr = (void*)myev;
    epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
}

void epoll_delete(int epfd, int fd, int events, myEvent* myev) { // todo: 还需要情况对应的myEv中的元素
    myev->reset();

    struct epoll_event ev;
    ev.events = events;
    ev.data.ptr = (void*) nullptr;

    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, &ev);
}

void epoll_mod(int fd, int events, myEvent* myev, callback cb) {
    myev->fd = fd;
    myev->events = events;
    myev->cb = cb;
    myev->arg = myev;

    struct epoll_event ev;
    ev.events = events;
    ev.data.ptr = (void*)myev;

    epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev);
}

int main(int argc, char** argv) {
    if(argc != 2) {
        printf("%s port\n", argv[0]);
        return -1;
    }

    int port = atoi(argv[1]);

    // 初始化服务端sock
    int lfd = init_server_sock(port);

    // 创建epoll
    epfd = epoll_create(1024);

    // listen 添加到epoll
    struct epoll_event evs[MY_EVENT_SIZE];
    epoll_add(lfd, EPOLLIN, &myEv[MY_EVENT_SIZE], init_accept);
    while(1) {
        int nready = epoll_wait(epfd, evs, MY_EVENT_SIZE, -1);
        if(nready < 0) {
            perror("");
            break;
        } else if(nready > 0) {
            for (int i = 0; i < nready; ++i) {
                myEvent* myev = (myEvent*)evs[i].data.ptr;
                if (evs[i].events & myev->events) {
                    myev->cb(myev->fd, myev->events, myev); // 回调
                }

            }
        }

    }


    return 0;
}

