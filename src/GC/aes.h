#ifndef EMP_AES_H_
#define EMP_AES_H_

#include <cstdio>
#include <cstring>
#include "GC/integer.h"
#include <fmt/core.h>
#include <vector>

namespace sci {

class AES {
private:
    static constexpr unsigned int NumColumns = 4;
    static constexpr unsigned int NumRows = 4;
    static constexpr unsigned int NumBytes = NumRows * NumColumns;
    static constexpr unsigned int ByteLen = 8;
    static constexpr unsigned int NumWords = NumRows;
    static constexpr unsigned int WordLen = NumColumns * ByteLen;
    static constexpr unsigned int blockBytesLen = NumBytes * ByteLen;

    typedef std::array<Integer, NumColumns> Row;
    typedef std::array<Integer, NumRows> Column;
    typedef std::array<Row, NumRows> State;
    typedef std::array<Integer, NumBytes> AES_block;

    static constexpr unsigned int Nk = 4;
    static constexpr unsigned int Nr = 10;
    std::vector<AES_block> roundKeys;

    void SubBytes(State& state);

    void ShiftRow(State& state, unsigned int i,
                    unsigned int n);  // shift row i on n positions

    void ShiftRows(State& state);

    Integer xtime(Integer b);  // multiply on x

    void mixSingleColumn(Column& r);
    
    void MixColumns(State& state);

    void AddRoundKey(State& state, const AES_block key);

    Integer SubWord(const Integer a);

    Integer RotWord(const Integer a);

    void SBox(Integer& state);

    Integer Rcon(unsigned int n);

    void KeySchedule(const Integer key);

    AES_block EncryptBlock(const AES_block in);

    std::array<Integer, 4> word2bytes(Integer word);
    Integer bytes2word(std::array<Integer, 4> bytes);
    std::map<uint32_t, Integer> Rcon_buffer;

public:
    explicit AES(const Integer key);

    Integer EncryptECB(const Integer in);
    
    std::vector<Integer> EncryptECB(const std::vector<Integer> in);

    inline static Integer create_block(uint64_t xh, uint64_t xl, int party) {
        return (Integer(128, xh, party) << 64) ^ Integer(128, xl, party); 
    }
};

} // namespace sci

#endif