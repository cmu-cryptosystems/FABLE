#include <sys/types.h>
#include <iostream>
#include <algorithm>

#include "lowmc.h"

namespace sci {

/////////////////////////////
//     LowMC functions     //
/////////////////////////////

inline void print(secret_block b) {
	for (int i=blocksize-1; i>=0; i--) {
		std::cout << b[i][0].reveal();
	}
	std::cout << std::endl;
}

template <size_t _Nb>
inline void print(std::array<Bit, _Nb> b) {
	for (int i=_Nb-1; i>=0; i--) {
		std::cout << b[i].reveal();
	}
	std::cout << std::endl;
}

secret_block batch_xor(const secret_block &a, const shared_block &b) {
    secret_block res;
    uint32_t simd_elems = a[0].size();
    for (unsigned i = 0; i < blocksize; i++) {
        res[i] = Integer(simd_elems, 0);
        for (unsigned j = 0; j < simd_elems; j++) {
            res[i][j] = a[i][j] ^ b[i];
        }
    }
    return res;
}

// NumAnds = size * rounds * numofboxes * 3
secret_block LowMC::encrypt (const secret_block message) {
    auto c = batch_xor(message, roundkeys[0]);
    for (unsigned r = 1; r <= rounds; ++r) {
        Substitution(c);
        c =  MultiplyWithGF2Matrix(LinMatrices[r-1], c);
        c = batch_xor(c, roundconstants[r-1]);
        c = batch_xor(c, roundkeys[r]);
    }
    return c;
}

void LowMC::set_key (keyblock k, int party) {
    key = share(k, party);
    keyschedule();
}

void LowMC::set_key (secret_keyblock k) {
    key = k;
    keyschedule();
}

void LowMC::print_matrices() {
    std::cout << "LowMC matrices and constants" << std::endl;
    std::cout << "============================" << std::endl;
    std::cout << "Block size: " << blocksize << std::endl;
    std::cout << "Key size: " << keysize << std::endl;
    std::cout << "Rounds: " << rounds << std::endl;
    std::cout << std::endl;

    std::cout << "Linear layer matrices" << std::endl;
    std::cout << "---------------------" << std::endl;
    for (unsigned r = 1; r <= rounds; ++r) {
        std::cout << "Linear layer " << r << ":" << std::endl;
        for (auto row: LinMatrices[r-1]) {
            std::cout << "[";
            for (unsigned i = 0; i < blocksize; ++i) {
                std::cout << row[i];
                if (i != blocksize - 1) {
                    std::cout << ", ";
                }
            }
            std::cout << "]" << std::endl;
        }
        std::cout << std::endl;
    }

    std::cout << "Round constants" << std::endl;
    std::cout << "---------------------" << std::endl;
    for (unsigned r = 1; r <= rounds; ++r) {
        std::cout << "Round constant " << r << ":" << std::endl;
        std::cout << "[";
        for (unsigned i = 0; i < blocksize; ++i) {
            std::cout << roundconstants[r-1][i].reveal();
            if (i != blocksize - 1) {
                std::cout << ", ";
            }
        }
        std::cout << "]" << std::endl;
        std::cout << std::endl;
    }
    
    std::cout << "Round key matrices" << std::endl;
    std::cout << "---------------------" << std::endl;
    for (unsigned r = 0; r <= rounds; ++r) {
        std::cout << "Round key matrix " << r << ":" << std::endl;
        for (auto row: KeyMatrices[r]) {
            std::cout << "[";
            for (unsigned i = 0; i < keysize; ++i) {
                std::cout << row[i];
                if (i != keysize - 1) {
                    std::cout << ", ";
                }
            }
            std::cout << "]" << std::endl;
        }
        if (r != rounds) {
            std::cout << std::endl;
        }
    }
}

/////////////////////////////
// LowMC private functions //
/////////////////////////////


void LowMC::Substitution (secret_block &message) {
    for (unsigned i = 0; i < numofboxes; ++i) {
        Sbox(message, 3*i); 
    }
}

secret_block LowMC::MultiplyWithGF2Matrix
        (const std::array<block, blocksize>& matrix, const secret_block message) {
    secret_block temp;
    for (unsigned i = 0; i < blocksize; ++i) {
        temp[i] = Integer(nvals_, 0);
        for (unsigned j = 0; j < blocksize; ++j) {
            if (matrix[i][j]) {
                temp[i] = temp[i] ^ message[j];
            }
        }
    }
    return temp;
}

shared_block LowMC::MultiplyWithGF2Matrix_Key
        (const std::array<keyblock, blocksize>& matrix, const secret_keyblock k) {
    shared_block temp;
    for (unsigned i = 0; i < blocksize; ++i) {
        temp[i] = Bit(0);
        for (unsigned j = 0; j < keysize; ++j) {
            if (matrix[i][j]) {
                temp[i] = temp[i] ^ k[j];
            }
        }
    }
    return temp;
}

void LowMC::keyschedule () {
    for (unsigned r = 0; r <= rounds; ++r) {
        roundkeys[r] = MultiplyWithGF2Matrix_Key (KeyMatrices[r], key);
    }
    return;
}


void LowMC::instantiate_LowMC () {
    // Create LinMatrices and invLinMatrices
    for (unsigned r = 0; r < rounds; ++r) {
        // Create matrix
        std::array<block, blocksize> mat;
        // Fill matrix with random bits
        do {
            for (unsigned i = 0; i < blocksize; ++i) {
                mat[i] = getrandblock ();
            }
        // Repeat if matrix is not invertible
        } while ( rank_of_Matrix(mat) != blocksize );
        LinMatrices[r] = mat;
    }

    // Create roundconstants
    for (unsigned r = 0; r < rounds; ++r) {
        roundconstants[r] = share(getrandblock ());
    }

    // Create KeyMatrices
    for (unsigned r = 0; r <= rounds; ++r) {
        // Create matrix
        std::array<keyblock, blocksize> mat;
        // Fill matrix with random bits
        do {
            for (unsigned i = 0; i < blocksize; ++i) {
                mat[i] = getrandkeyblock ();
            }
        // Repeat if matrix is not of maximal rank
        } while ( rank_of_Matrix_Key(mat) < std::min(blocksize, keysize) );
        KeyMatrices[r] = mat;
    }
    
    return;
}


/////////////////////////////
// Binary matrix functions //
/////////////////////////////


unsigned LowMC::rank_of_Matrix (const std::array<block, blocksize>& matrix) {
    std::array<block, blocksize> mat = matrix; //Copy of the matrix 
    unsigned size = mat[0].size();
    //Transform to upper triangular matrix
    unsigned row = 0;
    for (unsigned col = 1; col <= size; ++col) {
        if ( !mat[row][size-col] ) {
            unsigned r = row;
            while (r < mat.size() && !mat[r][size-col]) {
                ++r;
            }
            if (r >= mat.size()) {
                continue;
            } else {
                auto temp = mat[row];
                mat[row] = mat[r];
                mat[r] = temp;
            }
        }
        for (unsigned i = row+1; i < mat.size(); ++i) {
            if ( mat[i][size-col] ) mat[i] ^= mat[row];
        }
        ++row;
        if (row == size) break;
    }
    return row;
}


unsigned LowMC::rank_of_Matrix_Key (const std::array<keyblock, blocksize>& matrix) {
    std::array<keyblock, blocksize> mat = matrix; //Copy of the matrix 
    unsigned size = mat[0].size();
    //Transform to upper triangular matrix
    unsigned row = 0;
    for (unsigned col = 1; col <= size; ++col) {
        if ( !mat[row][size-col] ) {
            unsigned r = row;
            while (r < mat.size() && !mat[r][size-col]) {
                ++r;
            }
            if (r >= mat.size()) {
                continue;
            } else {
                auto temp = mat[row];
                mat[row] = mat[r];
                mat[r] = temp;
            }
        }
        for (unsigned i = row+1; i < mat.size(); ++i) {
            if ( mat[i][size-col] ) mat[i] ^= mat[row];
        }
        ++row;
        if (row == size) break;
    }
    return row;
}

///////////////////////
// Pseudorandom bits //
///////////////////////


block LowMC::getrandblock () {
    block tmp = 0;
    for (unsigned i = 0; i < blocksize; ++i) tmp[i] = getrandbit ();
    return tmp;
}

keyblock LowMC::getrandkeyblock () {
    keyblock tmp = 0;
    for (unsigned i = 0; i < keysize; ++i) tmp[i] = getrandbit ();
    return tmp;
}


// Uses the Grain LSFR as self-shrinking generator to create pseudorandom bits
// Is initialized with the all 1s state
// The first 160 bits are thrown away
bool LowMC::getrandbit () {
    static std::bitset<80> state; //Keeps the 80 bit LSFR state
    bool tmp = 0;
    //If state has not been initialized yet
    if (state.none ()) {
        state.set (); //Initialize with all bits set
        //Throw the first 160 bits away
        for (unsigned i = 0; i < 160; ++i) {
            //Update the state
            tmp =  state[0] ^ state[13] ^ state[23]
                       ^ state[38] ^ state[51] ^ state[62];
            state >>= 1;
            state[79] = tmp;
        }
    }
    //choice records whether the first bit is 1 or 0.
    //The second bit is produced if the first bit is 1.
    bool choice = false;
    do {
        //Update the state
        tmp =  state[0] ^ state[13] ^ state[23]
                   ^ state[38] ^ state[51] ^ state[62];
        state >>= 1;
        state[79] = tmp;
        choice = tmp;
        tmp =  state[0] ^ state[13] ^ state[23]
                   ^ state[38] ^ state[51] ^ state[62];
        state >>= 1;
        state[79] = tmp;
    } while (! choice);
    return tmp;
}

} // namespace sci