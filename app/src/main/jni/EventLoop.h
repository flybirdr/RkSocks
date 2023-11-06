//
// Created by 张开海 on 2023/10/31.
//

#ifndef R_EVENTLOOP_H
#define R_EVENTLOOP_H

#include <memory>
#include <mutex>
#include <sys/epoll.h>
#include <thread>

namespace R {
    class EventListener {
    public:
        virtual void onReadEvent(void *ptr) = 0;

        virtual void onWriteEvent(void *ptr) = 0;

        virtual void onErrorEvent(void *ptr) = 0;

        virtual void onHupEvent(void *ptr) = 0;
    };

    class EventLoop {
    private:

        EventListener *mListener;
        volatile int mEpollFd;
        volatile bool mLooping;
        std::recursive_mutex mLock;
        std::thread mThread;

        void destroyLopper();

        bool setLoop(bool looping);

        static void threadLoop(EventLoop *self);

    public:
        EventLoop() = delete;

        EventLoop(EventLoop &eventLoop) = delete;

        EventLoop(EventLoop &&eventLoop) = delete;

        explicit EventLoop(EventListener *listener);

        EventLoop &operator=(EventLoop &eventLoop) = delete;

        EventLoop &operator=(EventLoop &&eventLoop) = delete;

        ~EventLoop();

        void createLopper();

        void loop();

        void quit();

        bool registerOnReadOnly(int fd, void *ptr) const;

        bool registerOnWriteOnly(int fd, void *ptr) const;

        bool registerOnReadWrite(int fd, void *ptr) const;

        bool modOnRead(int fd, void *ptr) const;

        bool modOnWrite(int fd, void *ptr) const;

        bool modNone(int fd, void *ptr) const;

        bool modOnReadWrite(int fd, void *ptr) const;

        bool unregister(int fd) const;

    };

} // r

#endif //R_EVENTLOOP_H
