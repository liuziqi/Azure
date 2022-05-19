#ifndef __AZURE_BYTEARRAY_H__
#define __AZURE_BYTEARRAY_H__

#include <memory>
#include <string>
#include <stdint.h>

namespace azure {

class ByteArray {
public:
    typedef std::shared_ptr<ByteArray> ptr;

    struct Node {
        Node(size_t s);
        Node();
        ~Node();

        char *ptr;
        Node *next;
        size_t size;
    };

    ByteArray(size_t basesize=4096);
    ~ByteArray();

    // write
    void writeFint8(int8_t value);                       // F: fixed
    void writeFuint8(uint8_t value);
    void writeFint16(int16_t value);
    void writeFuint16(uint16_t value);
    void writeFint32(int32_t value);
    void writeFuint32(uint32_t value);
    void writeFint64(int64_t value);
    void writeFuint64(uint64_t value);

    void writeInt32(int32_t value);
    void writeUint32(uint32_t value);
    void writeInt64(int64_t value);
    void writeUint64(uint64_t value);

    void writeFloat(float value);
    void writeDouble(double value);

    // length:int16, data
    void writeStringF16(const std::string &value);
    // length:int32, data          
    void writeStringF32(const std::string &value);
    // length:int64, data              
    void writeStringF64(const std::string &value);
    // length:varint, data
    void writeStringVint(const std::string &value);
    // data          
    void writeStringWithoutLength(const std::string &value);    

    // read
    int8_t      readFint8();
    uint8_t     readFuint8();
    int16_t     readFint16();
    uint16_t    readFuint16();
    int32_t     readFint32();
    uint32_t    readFuint32();
    int64_t     readFint64();
    uint64_t    readFuint64();

    int32_t     readInt32();
    uint32_t    readUint32();
    int64_t     readInt64();
    uint64_t    readUint64();

    float       readFloat();
    double      readDouble();

    // length:int16, data
    std::string readStringF16();
    // length:int32, data
    std::string readStringF32();
    // length:int54, data
    std::string readStringF64();
    // length:varint, data
    std::string readStringVint();

    // 内部操作
    void clear();

    void write(const void *buf, size_t size);
    void read(void *buf, size_t size);
    void read(void *buf, size_t size, size_t position) const;

    size_t getPosition() const {return m_position;}
    void setPosition(size_t v);

    bool writeToFile(const std::string &name) const;
    bool readFromFile(const std::string &name);

    size_t getBaseSize() const {return m_basesize;}
    // 剩余可读长度
    size_t getReadSize() const {return m_size - m_position;}

    bool isLittleEndian() const;
    void setIsLittleEndian(bool val);

    std::string toString() const;
    std::string toHexString() const;

private:
    void addCapacity(size_t size);
    size_t getCapacity() const {return m_capacity - m_position;}

private:
    size_t m_basesize;
    size_t m_position;      // 当前读到的位置
    size_t m_capacity;
    size_t m_size;          // 真实数据大小
    int8_t m_endian;
    Node *m_root;           // 数据链表表头
    Node *m_cur;            // 当前读到的节点
};

}

#endif