//
// Created by morass on 2022/7/1.
//

#include "AsyncLogging.h"
#include <assert.h>
#include <stdio.h>
#include <chrono>
#include <unistd.h>
#include <functional>
#include "LogFile.h"

AsyncLogging::AsyncLogging(std::string logFileName_, int flushInterval)
        : flushInterval_(flushInterval),
          running_(false),
          basename_(logFileName_),
          thread_(std::bind(&AsyncLogging::threadFunc, this), "Logging"),
          mutex_(),
          cond_(),
          currentBuffer_(new Buffer),
          nextBuffer_(new Buffer),
          buffers_(),
          latch_(1) {
    assert(logFileName_.size() > 1);
    currentBuffer_->bzero();
    nextBuffer_->bzero();
    buffers_.reserve(16);
}

void AsyncLogging::append(const char *logline, int len) {
    std::lock_guard<std::mutex> lk(mutex_);
    if (currentBuffer_->avail() > len) {
        currentBuffer_->append(logline, len);
    } else {
        buffers_.push_back(std::move(currentBuffer_));
        if (nextBuffer_)
            currentBuffer_ = std::move(nextBuffer_);
        else
            currentBuffer_.reset(new Buffer);
        currentBuffer_->append(logline, len);
        cond_.notify_one();
    }
}

void AsyncLogging::threadFunc() {
    assert(running_ == true);
    latch_.countDown();
    LogFile output(basename_);
    BufferPtr newBuffer1(new Buffer);
    BufferPtr newBuffer2(new Buffer);
    newBuffer1->bzero();
    newBuffer2->bzero();
    BufferVector buffersToWrite;
    buffersToWrite.reserve(16);
    while (running_) {
        assert(newBuffer1 && newBuffer1->length() == 0);
        assert(newBuffer2 && newBuffer2->length() == 0);
        assert(buffersToWrite.empty());

        {
            std::unique_lock<std::mutex> lk(mutex_);
            if (buffers_.empty())  // unusual usage!
            {
                cond_.wait_for(lk,std::chrono::seconds(flushInterval_));
            }
            buffers_.push_back(std::move(currentBuffer_));
            currentBuffer_ = std::move(newBuffer1);
            buffersToWrite.swap(buffers_);
            if (!nextBuffer_) {
                nextBuffer_ = std::move(newBuffer2);
            }
        }

        assert(!buffersToWrite.empty());

        if (buffersToWrite.size() > 25) {
            buffersToWrite.erase(buffersToWrite.begin() + 2, buffersToWrite.end());
        }

        for (size_t i = 0; i < buffersToWrite.size(); ++i) {
            output.append(buffersToWrite[i]->data(), buffersToWrite[i]->length());
        }

        if (buffersToWrite.size() > 2) {
            // drop non-bzero-ed buffers, avoid trashing
            buffersToWrite.resize(2);
        }

        if (!newBuffer1) {
            assert(!buffersToWrite.empty());
            newBuffer1 = std::move(buffersToWrite.back());
            buffersToWrite.pop_back();
            newBuffer1->reset();
        }

        if (!newBuffer2) {
            assert(!buffersToWrite.empty());
            newBuffer2 = std::move(buffersToWrite.back());
            buffersToWrite.pop_back();
            newBuffer2->reset();
        }

        buffersToWrite.clear();
        output.flush();
    }
    output.flush();
}
