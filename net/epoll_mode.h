/**
 * *****************************************************************************
 * @file        epoll_mode.h
 * @brief       
 * @author  ()
 * @date        2023-09-26
 * @copyright   tfsc
 * *****************************************************************************
 */
#ifndef _EPOLLMODE_H_
#define _EPOLLMODE_H_

#include <sys/epoll.h>
#include <sys/resource.h>

#include <iostream>
#include <map>
#include <thread>

#include "./peer_node.h"
#include "./api.h"
#include "./socket_buf.h"

#include "../include/logging.h"

class EpollMode
{
public:
    //Listen Thread Func
    /**
     * @brief       
     * 
     * @return      int 
     */
    int EpollLoop();
    //Pub Func
    //Control Epoll Func

    /**
     * @brief       
     * 
     * @param       fd 
     * @param       state 
     * @return      true 
     * @return      false 
     */
    bool EpollLoop(int fd, int state);

    /**
     * @brief       
     * 
     * @param       fd 
     * @return      true 
     * @return      false 
     */
    bool DeleteEpollEvent(int fd);

    /**
     * @brief       
     * 
     * @return      true 
     * @return      false 
     */
    bool InitListen();

    /**
     * @brief       
     * 
     * @param       epmd 
     */
    static void EpollWork(EpollMode *epmd);

    /**
     * @brief       
     * 
     * @return      true 
     * @return      false 
     */
    bool EpoolModeStart();

    /**
     * @brief       
     * 
     */
    void EpollStop() { _haltListening = false; }

    EpollMode();
    ~EpollMode();

public:
    int epollFd;

    //Inner Var
private:
    std::thread* _listenThread;
    //Inner Func
private:
    int _fdSerMain;
    std::atomic<bool> _haltListening = true;
};

#endif
