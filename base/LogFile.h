//
// Created by morass on 2022/7/1.
//

#ifndef WEBSERVER_LOGFILE_H
#define WEBSERVER_LOGFILE_H


#include <memory>
#include <string>
#include <mutex>
#include "FileUtil.h"
#include "noncopyable.h"



class LogFile : noncopyable {
public:
    // 每被append flushEveryN次，flush一下，会往文件写，只不过，文件也是带缓冲区的
    LogFile(const std::string &basename, int flushEveryN = 1024);

    ~LogFile();

    void append(const char *logline, int len);

    void flush();

private:
    void append_unlocked(const char *logline, int len);

    const std::string basename_;
    const int flushEveryN_;
    int count_;
    std::unique_ptr<std::mutex> mutex_;
    std::unique_ptr<AppendFile> file_;
};


#endif //WEBSERVER_LOGFILE_H
