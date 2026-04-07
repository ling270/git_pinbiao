#include "streamreader.h"
#include <cstdio>
//#include <arpa/inet.h>

#ifdef _WIN32
#include <winsock2.h>
#include <Ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

StreamReader::StreamReader(QObject* parent) : QObject(parent) {}

size_t StreamReader::readGroups(FILE* f, size_t groupsToRead, std::vector<uint32_t>& out)
{
    size_t words = groupsToRead * 4;
    out.resize(words);
    size_t r = fread(out.data(), sizeof(uint32_t), words, f);
    if (r == 0) {
        out.clear();
        return 0;
    }
    return r / 4;
}

int32_t StreamReader::beToInt32(uint32_t be)
{
    uint32_t host = ntohl(be);
    return static_cast<int32_t>(host);
}
