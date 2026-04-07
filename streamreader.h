#pragma once
#include <QObject>
#include <vector>
#include <cstdint>

class StreamReader : public QObject {
    Q_OBJECT
public:
    explicit StreamReader(QObject* parent = nullptr);

    // 读取 groupsToRead 组（每组 4 个 uint32），返回实际读取组数
    size_t readGroups(FILE* f, size_t groupsToRead, std::vector<uint32_t>& out);

    // big-endian uint32 -> host int32
    static int32_t beToInt32(uint32_t be);
};
