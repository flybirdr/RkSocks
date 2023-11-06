//
// Created by 张开海 on 2023/10/31.
//

#include "EventLoop.h"
#include <sys/epoll.h>
#include "Logger.h"
#include <unistd.h>
#include "Util.h"
#include <thread>


namespace R {

    EventLoop::EventLoop(EventListener *listener) :
            mListener(listener),
            mEpollFd(0),
            mLooping(false) {

    }

    EventLoop::~EventLoop() {
        setLoop(false);
        destroyLopper();
    }

    void EventLoop::createLopper() {
        std::unique_lock<std::recursive_mutex> lock(mLock);
        if (mEpollFd) {
            return;
        }
        int fd = epoll_create(1024);
        if (fd <= 0) {
            LOGE("epoll_create failed errno=%d:%s", errno, strerror(errno));
            return;
        }
        mEpollFd = fd;
    }

    void EventLoop::destroyLopper() {
        std::unique_lock<std::recursive_mutex> lock(mLock);
        if (mEpollFd) {
            close(mEpollFd);
            mEpollFd = 0;
        }
    }

    void EventLoop::loop() {
        if (mLooping) {
            LOGE("already looping!!");
            return;
        }
        mThread = std::thread(threadLoop, this);
        mThread.detach();
    }

    void EventLoop::quit() {
        setLoop(false);
        if (mThread.joinable()) {
            mThread.join();
        }
    }

    bool EventLoop::setLoop(bool looping) {
        std::unique_lock<std::recursive_mutex> lock(mLock);
        if (mLooping == looping) {
            return false;
        }
        mLooping = looping;
        return true;
    }

    bool EventLoop::registerOnReadOnly(int fd, void *ptr) const {
        if (!mEpollFd) {
            LOGE("registerOnReadOnly(),epoll not created!");
            return false;
        }
        Util::setNonBlocking(fd);
        struct epoll_event event{0};
        event.data.ptr = ptr;
        event.events = EPOLLIN | EPOLLET;
        int ret = epoll_ctl(mEpollFd, EPOLL_CTL_ADD, fd, &event) != -1;
        if (!ret) {
            LOGE("Unable to register fd=%d read event!", fd);
        }
        return ret;
    }

    bool EventLoop::registerOnWriteOnly(int fd, void *ptr) const {
        if (!mEpollFd) {
            LOGE("registerOnWriteOnly(),epoll not created!");
            return false;
        }
        Util::setNonBlocking(fd);
        struct epoll_event event{0};
        event.data.ptr = ptr;
        event.events = EPOLLOUT | EPOLLET;

        int ret = epoll_ctl(mEpollFd, EPOLL_CTL_ADD, fd, &event) != -1;
        if (!ret) {
            LOGE("Unable to register fd=%d write event!", fd);
        }
        return ret;
    }

    bool EventLoop::registerOnReadWrite(int fd, void *ptr) const {
        if (!mEpollFd) {
            LOGE("registerOnReadWrite(),epoll not created!");
            return false;
        }
        Util::setNonBlocking(fd);
        struct epoll_event event{0};
        event.data.ptr = ptr;
        event.events = EPOLLIN | EPOLLOUT | EPOLLET;
        int ret = epoll_ctl(mEpollFd, EPOLL_CTL_ADD, fd, &event) != -1;
        if (!ret) {
            LOGE("Unable to register fd=%d read/write event!", fd);
        }
        return ret;
    }

    bool EventLoop::unregister(int fd) const {
        if (!mEpollFd) {
            LOGE("unregister(),epoll not created!");
            return false;
        }
        struct epoll_event event{0};
        event.data.fd = fd;
        return epoll_ctl(mEpollFd, EPOLL_CTL_DEL, fd, &event) != -1;
    }

    void EventLoop::threadLoop(EventLoop *self) {
        if (!self->mEpollFd) {
            LOGE("epoll create failed!");
            return;
        }
        if (!self->mListener) {
            LOGE("no listener set skip to loop!!!");
            return;
        }
        if (self->mLooping || !self->setLoop(true)) {
            LOGE("already looping!!");
            return;
        }
        struct epoll_event events[8];
        while (self->mLooping) {
            int nEvent = epoll_wait(self->mEpollFd, events, 8, 500);
            if (nEvent >= 0) {
                for (int i = 0; i < nEvent; ++i) {
                    auto ptr = events[i].data.ptr;
                    if (events[i].events & EPOLLIN) {
                        self->mListener->onReadEvent(ptr);
                    } else if (events[i].events & EPOLLOUT) {
                        self->mListener->onWriteEvent(ptr);
                    } else if (events[i].events & EPOLLERR) {
                        self->mListener->onErrorEvent(ptr);
                    } else if (events[i].events & EPOLLHUP) {
                        self->mListener->onHupEvent(ptr);
                    }
                }
            } else {
                if (errno == EINTR) {
                    continue;
                }
                LOGE("epoll_wait failed errno=%d:%s", errno, strerror(errno));
                break;
            }
        }
        self->setLoop(false);
        self->destroyLopper();
    }

    bool EventLoop::modOnRead(int fd, void *ptr) const {
        if (!mEpollFd) {
            LOGE("registerOnReadWrite(),epoll not created!");
            return false;
        }
        Util::setNonBlocking(fd);
        struct epoll_event event{0};
        event.data.ptr = ptr;
        event.events = EPOLLIN | EPOLLET;
        int ret = epoll_ctl(mEpollFd, EPOLL_CTL_MOD, fd, &event) != -1;
        if (!ret) {
            LOGE("Unable to mod fd=%d read event,errno=%d:%s", fd, errno, strerror(errno));
        }
        return ret;
    }

    bool EventLoop::modOnWrite(int fd, void *ptr) const {
        if (!mEpollFd) {
            LOGE("registerOnReadWrite(),epoll not created!");
            return false;
        }
        Util::setNonBlocking(fd);
        struct epoll_event event{0};
        event.data.ptr = ptr;
        event.events = EPOLLOUT | EPOLLET;
        int ret = epoll_ctl(mEpollFd, EPOLL_CTL_MOD, fd, &event) != -1;
        if (!ret) {
            LOGE("Unable to mod fd=%d out event!", fd);
        }
        return ret;
    }

    bool EventLoop::modNone(int fd, void *ptr) const {
        if (!mEpollFd) {
            LOGE("registerOnReadWrite(),epoll not created!");
            return false;
        }
        Util::setNonBlocking(fd);
        struct epoll_event event{0};
        event.data.ptr = ptr;
        event.events = EPOLLET;
        int ret = epoll_ctl(mEpollFd, EPOLL_CTL_MOD, fd, &event) != -1;
        if (!ret) {
            LOGE("Unable to mod fd=%d none,errno=%d:%s", fd, errno, strerror(errno));
        }
        return ret;
    }

    bool EventLoop::modOnReadWrite(int fd, void *ptr) const {
        if (!mEpollFd) {
            LOGE("registerOnReadWrite(),epoll not created!");
            return false;
        }
        Util::setNonBlocking(fd);
        struct epoll_event event{0};
        event.data.ptr = ptr;
        event.events = EPOLLIN | EPOLLOUT | EPOLLET;
        int ret = epoll_ctl(mEpollFd, EPOLL_CTL_MOD, fd, &event) != -1;
        if (!ret) {
            LOGE("Unable to mod fd=%d read/write!", fd);
        }
        return ret;
    }
} // r