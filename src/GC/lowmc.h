#ifndef EMP_LOWMC_H__
#define EMP_LOWMC_H__

#include <bitset>
#include <array>

#include "GC/bit.h"
#include "GC/integer.h"
#include <fmt/core.h>


namespace sci {

const unsigned numofboxes = 1;    // Number of Sboxes
const unsigned blocksize = 64;   // Block size in bits
const unsigned keysize = 128; // Key size in bits
const unsigned rounds = 184; // Number of rounds

const unsigned identitysize = blocksize - 3*numofboxes;
                  // Size of the identity part in the Sbox layer

typedef std::bitset<blocksize> block; // Store messages and states
typedef std::bitset<keysize> keyblock;
typedef std::array<Integer, blocksize> secret_block; // Store messages and states
typedef std::array<Bit, keysize> secret_keyblock;
typedef std::array<Bit, blocksize> shared_block; // Store messages and states

template <typename T = Bit, size_t _Nb>
inline std::array<T, _Nb> share(std::bitset<_Nb> b, int party=PUBLIC) {
    std::array<T, _Nb> result; 
    for (int i = 0; i < b.size(); i++) {
        result[i] = Bit(b[i], party);
    }
    return result;
}

class LowMC {
public:
    uint32_t nvals_;

    LowMC (keyblock k, int party, uint32_t nvals = 1) : nvals_(nvals) {
        key = share(k, party);
        instantiate_LowMC();
        keyschedule();   
    };

    secret_block encrypt (const secret_block message);
    void set_key (keyblock k, int party=PUBLIC);
    void set_key (secret_keyblock k);

    void print_matrices();

    void verify_sbox();

private:
// LowMC private data members //
    // // The Sbox and its inverse    
    // const std::vector<unsigned> Sbox =
    //     {0x00, 0x01, 0x03, 0x06, 0x07, 0x04, 0x05, 0x02};
    // const std::vector<unsigned> invSbox =
    //     {0x00, 0x01, 0x07, 0x02, 0x05, 0x06, 0x03, 0x04};

    // Compute with the lowest 3 bits of message
    void Sbox(secret_block &messages, size_t offset) {
        auto a = messages[offset+2];
        auto b = messages[offset+1];
        auto c = messages[offset+0];
        messages[offset+2] = a ^ (b & c);
        messages[offset+1] = a ^ b ^ (a & c);
        messages[offset+0] = a ^ b ^ c ^ (a & b);
    }

    std::array<std::array<block, blocksize>, rounds> LinMatrices;
        // Stores the binary matrices for each round
    // std::vector<std::vector<block>> invLinMatrices;
        // Stores the inverses of LinMatrices
    std::array<shared_block, rounds> roundconstants;
        // Stores the round constants
    secret_keyblock key;
        //Stores the master key
    std::array<std::array<keyblock, blocksize>, rounds+1> KeyMatrices;
        // Stores the matrices that generate the round keys
    std::array<shared_block, rounds+1> roundkeys;
        // Stores the round keys
    
// LowMC private functions //
    void Substitution (secret_block &message);
        // The substitution layer

    secret_block MultiplyWithGF2Matrix
        (const std::array<block, blocksize>& matrix, const secret_block message);
        // For the linear layer
    shared_block MultiplyWithGF2Matrix_Key
        (const std::array<keyblock, blocksize>& matrix, const secret_keyblock k);
        // For generating the round keys

    void keyschedule ();
        //Creates the round keys from the master key

    void instantiate_LowMC ();
        //Fills the matrices and roundconstants with pseudorandom bits 
   
// Binary matrix functions //   
    unsigned rank_of_Matrix (const std::array<block, blocksize>& matrix);
    unsigned rank_of_Matrix_Key (const std::array<keyblock, blocksize>& matrix);
    // std::vector<block> invert_Matrix (const std::vector<block> matrix);

// Random bits functions //
    block getrandblock ();
    keyblock getrandkeyblock ();
    bool  getrandbit ();

};

} // namespace sci

#endif