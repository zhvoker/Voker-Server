#ifndef BUFFER_H
#define BUFFER_H

#include <vector>
#include <string>
#include <algorithm>
#include <stddef.h>

// 网络库底层的缓冲区类型定义
/*
+-------------------------+----------------------+---------------------+
|    prependable bytes    |    readable bytes    |    writable bytes   |
|                         |      (CONTENT)       |                     |
+-------------------------+----------------------+---------------------+
|                         |                      |                     |
0        <=           readerIndex     <=     writerIndex             size
*/

// 注意，readable bytes空间才是要服务端要发送的数据，writable bytes空间是从socket读来的数据存放的地方。
// 具体的说：是write还是read都是站在buffer角度看问题的，不是站在和客户端通信的socket角度。当客户socket有数据发送过来时，
// socket处于可读，所以我们要把socket上的可读数据写如到buffer中缓存起来，所以对于buffer来说，当socket可读时，是需要向buffer中
// write数据的，因此 writable bytes 指的是用于存放socket上可读数据的空间, readable bytes同理

class Buffer
{
public:
    static const size_t kCheapPrepend = 8;          // 记录数据包的长度的变量长度，用于解决粘包问题
    static const size_t kInitialSize = 1024;        // 缓冲区长度（不包括kCheapPrepend）

    explicit Buffer(size_t initalSize = kInitialSize)
        : buffer_(kCheapPrepend + initalSize)
        , readerIndex_(kCheapPrepend)
        , writerIndex_(kCheapPrepend)
    {
    }

    size_t readableBytes() const { return writerIndex_ - readerIndex_; }
    size_t writableBytes() const { return buffer_.size() - writerIndex_; }
    size_t prependableBytes() const { return readerIndex_; }

    // 返回缓冲区中可读数据的起始地址
    const char *peek() const { return begin() + readerIndex_; }
    void retrieve(size_t len)
    {
        if (len < readableBytes())
        {
            readerIndex_ += len; // 说明应用只读取了可读缓冲区数据的一部分，就是len长度 还剩下readerIndex+=len到writerIndex_的数据未读
        }
        else // len == readableBytes()
        {
            retrieveAll();
        }
    }
    void retrieveAll()
    {
        readerIndex_ = kCheapPrepend;
        writerIndex_ = kCheapPrepend;
    }

    // 把onMessage函数上报的Buffer数据 转成string类型的数据返回
    std::string retrieveAllAsString() { return retrieveAsString(readableBytes()); }
    std::string retrieveAsString(size_t len)
    {
        std::string result(peek(), len);
        retrieve(len); // 上面一句把缓冲区中可读的数据已经读取出来 这里肯定要对缓冲区进行复位操作
        return result;
    }

    // buffer_.size - writerIndex_
    void ensureWritableBytes(size_t len)
    {
        if (writableBytes() < len)
        {
            makeSpace(len); // 扩容
        }
    }

    // 把[data, data+len]内存上的数据添加到writable缓冲区当中
    void append(const char *data, size_t len)
    {
        ensureWritableBytes(len);
        std::copy(data, data+len, beginWrite());
        writerIndex_ += len;
    }
    char *beginWrite() { return begin() + writerIndex_; }
    const char *beginWrite() const { return begin() + writerIndex_; }

    // 从fd上读取数据
    ssize_t readFd(int fd, int *saveErrno);
    // 通过fd发送数据
    ssize_t writeFd(int fd, int *saveErrno);

private:
    // vector底层数组首元素的地址 也就是数组的起始地址
    char *begin() { return &*buffer_.begin(); }
    const char *begin() const { return &*buffer_.begin(); }

    void makeSpace(size_t len)
    {
        /**
         * | kCheapPrepend |xxx| reader | writer |                     // xxx标示reader中已读的部分
         * | kCheapPrepend | reader ｜          len          |
         **/
        if (writableBytes() + prependableBytes() < len + kCheapPrepend) // 也就是说 len > xxx + writer的部分
        {
            buffer_.resize(writerIndex_ + len);
        }
        else // 这里说明 len <= xxx + writer 把reader搬到从xxx开始 使得xxx后面是一段连续空间
        {
            // p-kC(prependable减去kCheapPrepend)的长度与writable bytes的和就是可写的长度，如果len <= 可写的部分,
            // 说明还能len长的数据还能写的下，但是可写的空间是p-kC的长度与writable bytes
            // 共同构成，而且p-kC的长度与writable bytes不连续，所以需要将readable bytes向前移动p-kC个单位，使得可写部分空间是连续的
            // 调整前：
            // +-------------------------+----------------------+---------------------+
            // |    prependable bytes    |    readable bytes    |    writable bytes   |
            // | kCheapPrepend |  p-kC   |      (CONTENT)       |                     |
            // +-------------------------+----------------------+---------------------+
            // |               |         |                      |                     |
            // 0        <=     8     readerIndex     <=     writerIndex             size
            // 调整后：
            // +---------------+--------------------+---------------------------------+
            // | prependable   |  readable bytes    |     新的writable bytes          |
            // | kCheapPrepend |    (CONTENT)       | = p-kC+之前的writable bytes     |
            // +---------------+--------------------+---------------------------------+
            // |               |                    |                                 |
            // 0    <=    readerIndex    <=    writerIndex                    

            size_t readable = readableBytes(); // readable = reader的长度
            std::copy(begin() + readerIndex_,
                      begin() + writerIndex_,  // 把这一部分数据拷贝到begin+kCheapPrepend起始处
                      begin() + kCheapPrepend);
            readerIndex_ = kCheapPrepend;
            writerIndex_ = readerIndex_ + readable;
        }
    }

    std::vector<char> buffer_;
    size_t readerIndex_;
    size_t writerIndex_;
};

#endif