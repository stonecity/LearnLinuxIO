/*
 * @Author       : mark
 * @Date         : 2020-06-25
 * @copyleft Apache 2.0
 */
#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H

#include <unordered_map>
#include <fcntl.h>    // open
#include <unistd.h>   // close
#include <sys/stat.h> // stat
#include <sys/mman.h> // mmap, munmap

#include "../buffer/buffer.h"
#include "../log/log.h"

class HttpResponse
{
public:
    HttpResponse();
    ~HttpResponse();

    void Init(const std::string &srcDir, std::string &path, bool isKeepAlive = false, int code = -1);
    void MakeResponse(Buffer &buff);
    void UnmapFile();
    char *File();
    size_t FileLen() const;
    void ErrorContent(Buffer &buff, std::string message);
    int Code() const { return mCode; }

private:
    void AddStateLine(Buffer &buff);
    void AddHeader(Buffer &buff);
    void AddContent(Buffer &buff);

    void ErrorHtml();
    std::string GetFileType();

    int mCode;
    bool isKeepAlive;

    std::string mPath;
    std::string mSrcDir;

    char *mmFile;
    struct stat mmFileStat;

    static const std::unordered_map<std::string, std::string> SUFFIX_TYPE;
    static const std::unordered_map<int, std::string> CODE_STATUS;
    static const std::unordered_map<int, std::string> CODE_PATH;
};

#endif // HTTP_RESPONSE_H