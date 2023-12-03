/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// SimpleBuffer
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <stdint.h>
#include <stddef.h>
#include <vector>

class SimpleBuffer
{
public:
    static const unsigned DEFAULT_MAX_LEN = 5000;
    SimpleBuffer(unsigned maxFrameLen = DEFAULT_MAX_LEN)
    {
        _bufMaxLen = maxFrameLen;
    }

    virtual ~SimpleBuffer()
    {
    }

    void clear()
    {
        _buffer.clear();
    }

    void setMaxLen(unsigned maxLen)
    {
        _bufMaxLen = maxLen;
    }

    bool resize(unsigned size)
    {
        if (size > _bufMaxLen)
            return false;
        _buffer.resize(size);
        return _buffer.size() == size;
    }

    bool setAt(unsigned idx, uint8_t val)
    {
        if (idx >= _bufMaxLen)
            return false;
        if (idx >= _buffer.size())
            _buffer.resize(idx+1);
        if (idx >= _buffer.size())
                return false;
        _buffer[idx] = val;
        return true;
    }

    uint8_t getAt(unsigned idx) const
    {
        if (idx >= _buffer.size())
            return 0;
        return _buffer[idx];
    }

    uint8_t* data()
    {
        return _buffer.data();
    }

    unsigned size()
    {
        return _buffer.size();
    }

private:
    std::vector<uint8_t> _buffer;
    unsigned _bufMaxLen;
};
