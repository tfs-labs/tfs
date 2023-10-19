/**
 * *****************************************************************************
 * @file        single.hpp
 * @brief       
 * @author HXD (2848973813@qq.com)
 * @date        2023-09-28
 * @copyright   tfsc
 * *****************************************************************************
 */
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/file.h>
 
#define MY_PID_FILE     "/tmp/my_pid_file"
#define BUF_LEN_FOR_PID 64
 
/**
 * @brief       
 * 
 * @param       fd: 
 * @param       pid: 
 * @return      int 
 */
static int WritePidIntoFd(int fd, pid_t pid)
{
	int ret = -1;
	char buf[BUF_LEN_FOR_PID] = {0};
 
	/* Move cursor to the start of file. */
	lseek(fd, 0, SEEK_SET);
 
	sprintf(buf, "%d", pid);
	ret = write(fd, buf, strlen(buf));
	if(ret <= 0) { /* Write fail or write 0 byte */
		if(ret == -1) 
			perror("Write " MY_PID_FILE" fail\n");
 
		ret = -1;
	} else {
		
		ret = 0;
	}
 
	return ret;
}
 
/*
 * Create MY_PID_FILE, write pid into it.
 *
 * @return: 0 is ok, -1 is error.
 */
static int CreatePidFile(pid_t pid)
{
	int fd, ret;
	char buf[BUF_LEN_FOR_PID] = {0};
 
	fd = open(MY_PID_FILE, O_WRONLY | O_CREAT | O_EXCL, 0666);  /* rw-rw-rw- */
	if(fd == -1) {
		perror("Create " MY_PID_FILE" fail\n");
		return -1;
	} 
 
	ret = flock(fd, LOCK_EX);
	if(ret == -1) {
		perror("flock " MY_PID_FILE" fail\n");
		close(fd);
		return -1;
	}
 
	ret = WritePidIntoFd(fd, pid);
 
	flock(fd, LOCK_UN);
	close(fd);
	
	return ret;
}
 
/*
 * If pid file already exists, check the pid value in it.
 * If pid from file is still running, this program need exit();
 * If it is not running, write current pid into file.
 *
 * @return: 0 is ok, -1 is error.
 */
static int CheckPidFile(int fd, pid_t pid)
{
	int ret = -1;
	pid_t old_pid;
	char buf[BUF_LEN_FOR_PID] = {0};
 
	ret = flock(fd, LOCK_EX);
	if(ret == -1) {
		perror("flock " MY_PID_FILE" fail\n");
		return -1;
	}
 
	ret = read(fd, buf, sizeof(buf)-1);
	if(ret < 0) {  /* read error */
		perror("read from " MY_PID_FILE" fail\n");
		ret = -1;
	} else if(ret > 0) {  /* read ok */
		old_pid = atol(buf);
 
		/* Check if old_pid is running */
		ret = kill(old_pid, 0);
		if(ret < 0) {
			if(errno == ESRCH) { /* old_pid is not running. */
				ret = WritePidIntoFd(fd, pid);
			} else {
				perror("send signal fail\n");
				ret = -1;
			}
		} else {  /* running */
			printf("Program already exists, pid=%d\n", old_pid);
			ret = -1;
		}
	} else if(ret == 0) { /* read 0 byte from file */
		ret = WritePidIntoFd(fd, pid);
	}
 
	flock(fd, LOCK_UN);
 
	return ret;
}
 
/*
 * It will create the only one pid file for app.
 * 
 * @return: 0 is ok, -1 is error.
 */
static int InitPidFile()
{
	pid_t pid;
	int fd, ret;
	
	pid = getpid();
 
	fd = open(MY_PID_FILE, O_RDWR);
	if(fd == -1) {  /* open file fail */
		if(errno == ENOENT) {  /* No such file. Create one for this program. */
			ret = CreatePidFile(pid);
		} else {
			perror("open " MY_PID_FILE" fail\n");
			ret = -1;
		}
	} else {  /* pid file already exists */
		ret = CheckPidFile(fd, pid);
		close(fd);
	}
 
	return ret;
}

/**
 * @brief       
 * 
 * @param       sig: 
 */
static void sigHandler(int sig)
{
	if(sig == SIGINT || sig == SIGTERM)
		remove(MY_PID_FILE);
 
	_exit(0);
}