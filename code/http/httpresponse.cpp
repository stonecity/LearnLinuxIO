/*
 * @Author       : mark
 * @Date         : 2020-06-27
 * @copyleft Apache 2.0
 */
#include "httpresponse.h"

using namespace std;

const unordered_map<string, string> HttpResponse::SUFFIX_TYPE = {
    {".html", "text/html"},
    {".xml", "text/xml"},
    {".xhtml", "application/xhtml+xml"},
    {".txt", "text/plain"},
    {".rtf", "application/rtf"},
    {".pdf", "application/pdf"},
    {".word", "application/nsword"},
    {".png", "image/png"},
    {".gif", "image/gif"},
    {".jpg", "image/jpeg"},
    {".jpeg", "image/jpeg"},
    {".au", "audio/basic"},
    {".mpeg", "video/mpeg"},
    {".mpg", "video/mpeg"},
    {".avi", "video/x-msvideo"},
    {".gz", "application/x-gzip"},
    {".tar", "application/x-tar"},
    {".css", "text/css "},
    {".js", "text/javascript "},
};

const unordered_map<int, string> HttpResponse::CODE_STATUS = {
    {200, "OK"},
    {400, "Bad Request"},
    {403, "Forbidden"},
    {404, "Not Found"},
};

const unordered_map<int, string> HttpResponse::CODE_PATH = {
    {400, "/400.html"},
    {403, "/403.html"},
    {404, "/404.html"},
};

HttpResponse::HttpResponse()
{
    mCode = -1;
    mPath = mSrcDir = "";
    isKeepAlive = false;
    mmFile = nullptr;
    mmFileStat = {0};
};

HttpResponse::~HttpResponse()
{
    UnmapFile();
}

void HttpResponse::Init(const string &srcDir, string &path, bool isKeepAlive, int code)
{
    assert(srcDir != "");
    if (mmFile)
    {
        UnmapFile();
    }
    mCode = code;
    isKeepAlive = isKeepAlive;
    mPath = path;
    mSrcDir = srcDir;
    mmFile = nullptr;
    mmFileStat = {0};
}

void HttpResponse::MakeResponse(Buffer &buff)
{
    /* 判断请求的资源文件 */
    if (stat((mSrcDir + mPath).data(), &mmFileStat) < 0 || S_ISDIR(mmFileStat.st_mode))
    {
        mCode = 404;
    }
    else if (!(mmFileStat.st_mode & S_IROTH))
    {
        mCode = 403;
    }
    else if (mCode == -1)
    {
        mCode = 200;
    }
    ErrorHtml();
    AddStateLine(buff);
    AddHeader(buff);
    AddContent(buff);
}

char *HttpResponse::File()
{
    return mmFile;
}

size_t HttpResponse::FileLen() const
{
    return mmFileStat.st_size;
}

void HttpResponse::ErrorHtml()
{
    if (CODE_PATH.count(mCode) == 1)
    {
        mPath = CODE_PATH.find(mCode)->second;
        stat((mSrcDir + mPath).data(), &mmFileStat);
    }
}

void HttpResponse::AddStateLine(Buffer &buff)
{
    string status;
    if (CODE_STATUS.count(mCode) == 1)
    {
        status = CODE_STATUS.find(mCode)->second;
    }
    else
    {
        mCode = 400;
        status = CODE_STATUS.find(400)->second;
    }
    buff.Append("HTTP/1.1 " + to_string(mCode) + " " + status + "\r\n");
}

void HttpResponse::AddHeader(Buffer &buff)
{
    buff.Append("Connection: ");
    if (isKeepAlive)
    {
        buff.Append("keep-alive\r\n");
        buff.Append("keep-alive: max=6, timeout=120\r\n");
    }
    else
    {
        buff.Append("close\r\n");
    }
    buff.Append("Content-type: " + GetFileType() + "\r\n");
}

void HttpResponse::AddContent(Buffer &buff)
{
    int srcFd = open((mSrcDir + mPath).data(), O_RDONLY);
    if (srcFd < 0)
    {
        ErrorContent(buff, "File NotFound!");
        return;
    }

    /* 将文件映射到内存提高文件的访问速度
        MAP_PRIVATE 建立一个写入时拷贝的私有映射*/
    LOG_DEBUG("file path %s", (mSrcDir + mPath).data());
    int *mmRet = (int *)mmap(0, mmFileStat.st_size, PROT_READ, MAP_PRIVATE, srcFd, 0);
    if (*mmRet == -1)
    {
        ErrorContent(buff, "File NotFound!");
        return;
    }
    mmFile = (char *)mmRet;
    close(srcFd);
    buff.Append("Content-length: " + to_string(mmFileStat.st_size) + "\r\n\r\n");
}

void HttpResponse::UnmapFile()
{
    if (mmFile)
    {
        munmap(mmFile, mmFileStat.st_size);
        mmFile = nullptr;
    }
}

string HttpResponse::GetFileType()
{
    /* 判断文件类型 */
    string::size_type idx = mPath.find_last_of('.');
    if (idx == string::npos)
    {
        return "text/plain";
    }
    string suffix = mPath.substr(idx);
    if (SUFFIX_TYPE.count(suffix) == 1)
    {
        return SUFFIX_TYPE.find(suffix)->second;
    }
    return "text/plain";
}

void HttpResponse::ErrorContent(Buffer &buff, string message)
{
    string body;
    string status;
    body += "<html><title>Error</title>";
    body += "<body bgcolor=\"ffffff\">";
    if (CODE_STATUS.count(mCode) == 1)
    {
        status = CODE_STATUS.find(mCode)->second;
    }
    else
    {
        status = "Bad Request";
    }
    body += to_string(mCode) + " : " + status + "\n";
    body += "<p>" + message + "</p>";
    body += "<hr><em>TinyWebServer</em></body></html>";

    buff.Append("Content-length: " + to_string(body.size()) + "\r\n\r\n");
    buff.Append(body);
}
