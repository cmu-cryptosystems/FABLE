#include "aes.h"
#include <array>
#include <cryptoTools/Common/block.h>
#include <wmmintrin.h>

namespace sci {

AES::AES(const Integer key) {
  assert (key.size() == 128);
  KeySchedule(key);
}

Integer AES::EncryptECB(const Integer in) {
  assert(in.size() == blockBytesLen);
  
  AES_block inner_in;

  for (unsigned int i = 0; i < NumBytes; i++) {
    inner_in[NumBytes - i - 1].bits = std::vector<Bit>(in.bits.begin() + i * 8, in.bits.begin() + (i+1) * 8);
  }

  AES_block inner_out = EncryptBlock(inner_in);

  Integer out;
  for (unsigned int i = 0; i < NumBytes; i++) {
    out.bits.insert(out.bits.end(), inner_out[NumBytes - i - 1].bits.begin(), inner_out[NumBytes - i - 1].bits.end());
  }

  return out;
}

std::vector<Integer> AES::EncryptECB(const std::vector<Integer> in) {
  std::vector<Integer> res;
  for (auto& input : in)
    res.push_back(EncryptECB(input));
  return res;
}

AES::AES_block AES::EncryptBlock(const AES_block in) {
  State state;

  for (int i = 0; i < NumRows; i++) {
    for (int j = 0; j < NumColumns; j++) {
      state[i][j] = in[i + j * NumRows];
    }
  }

  AddRoundKey(state, roundKeys[0]);

  for (int round = 1; round <= Nr - 1; round++) {
    SubBytes(state);
    ShiftRows(state);
    MixColumns(state);
    AddRoundKey(state, roundKeys[round]);
  }

  SubBytes(state);
  ShiftRows(state);
  AddRoundKey(state, roundKeys[Nr]);

  AES_block out;

  for (int i = 0; i < NumRows; i++) {
    for (int j = 0; j < NumColumns; j++) {
      out[i + j * NumRows] = state[i][j];
    }
  }

  return out;
}

void AES::SubBytes(State& state) {
  for (int i = 0; i < NumRows; i++) {
    for (int j = 0; j < NumColumns; j++) {
      SBox(state[i][j]);
    }
  }
}

void AES::ShiftRow(State& state, unsigned int row, unsigned int n)  // shift row i on n positions
{
  Row tmp = state[row];
  for (unsigned int column = 0; column < NumColumns; column++) {
    state[row][column] = tmp[(column + n) % NumColumns];
  }
}

void AES::ShiftRows(State& state) {
  ShiftRow(state, 2, -1);
  ShiftRow(state, 1, -2);
  ShiftRow(state, 0, -3);
}

// https://en.wikipedia.org/wiki/Rijndael_MixColumns#Implementation_example
void AES::mixSingleColumn(Column& r) {
  Column b;
  for(int c=0;c<4;c++) {
    b[c] = xtime(r[c]);
  }
  Column res;
  res[0] = b[0] ^ r[3] ^ r[2] ^ b[1] ^ r[1]; /* 2 * a0 + a3 + a2 + 3 * a1 */
  res[1] = b[1] ^ r[0] ^ r[3] ^ b[2] ^ r[2]; /* 2 * a1 + a0 + a3 + 3 * a2 */
  res[2] = b[2] ^ r[1] ^ r[0] ^ b[3] ^ r[3]; /* 2 * a2 + a1 + a0 + 3 * a3 */
  res[3] = b[3] ^ r[2] ^ r[1] ^ b[0] ^ r[0]; /* 2 * a3 + a2 + a1 + 3 * a0 */
  r = res;
}

void AES::MixColumns(State& state) {

  Column temp;
  for(int i = 0; i < NumColumns; ++i) {
    for(int j = 0; j < 4; ++j) {
        temp[3-j] = state[j][i]; //place the current state column in temp
    }
    mixSingleColumn(temp); //mix it using the wiki implementation
    for(int j = 0; j < 4; ++j) {
        state[j][i] = temp[3-j]; //when the column is mixed, place it back into the state
    }
  }
}

void AES::AddRoundKey(State& state, const AES_block key) {
  for (int i = 0; i < NumRows; i++) {
    for (int j = 0; j < NumColumns; j++) {
      state[i][j] = state[i][j] ^ key[i + NumRows * j];
    }
  }
}

// https://github.com/randombit/botan/blob/master/src/lib/block/aes/aes.cpp
void AES::SBox(Integer& V) {
  assert (V.size() == 8);

  Bit U0 = V[7];
  Bit U1 = V[6];
  Bit U2 = V[5];
  Bit U3 = V[4];
  Bit U4 = V[3];
  Bit U5 = V[2];
  Bit U6 = V[1];
  Bit U7 = V[0];

  Bit y14 = U3 ^ U5;
  Bit y13 = U0 ^ U6;
  Bit y9 = U0 ^ U3;
  Bit y8 = U0 ^ U5;
  Bit t0 = U1 ^ U2;
  Bit y1 = t0 ^ U7;
  Bit y4 = y1 ^ U3;
  Bit y12 = y13 ^ y14;
  Bit y2 = y1 ^ U0;
  Bit y5 = y1 ^ U6;
  Bit y3 = y5 ^ y8;
  Bit t1 = U4 ^ y12;
  Bit y15 = t1 ^ U5;
  Bit y20 = t1 ^ U1;
  Bit y6 = y15 ^ U7;
  Bit y10 = y15 ^ t0;
  Bit y11 = y20 ^ y9;
  Bit y7 = U7 ^ y11;
  Bit y17 = y10 ^ y11;
  Bit y19 = y10 ^ y8;
  Bit y16 = t0 ^ y11;
  Bit y21 = y13 ^ y16;
  Bit y18 = U0 ^ y16;
  Bit t2 = y12 & y15;
  Bit t3 = y3 & y6;
  Bit t4 = t3 ^ t2;
  Bit t5 = y4 & U7;
  Bit t6 = t5 ^ t2;
  Bit t7 = y13 & y16;
  Bit t8 = y5 & y1;
  Bit t9 = t8 ^ t7;
  Bit t10 = y2 & y7;
  Bit t11 = t10 ^ t7;
  Bit t12 = y9 & y11;
  Bit t13 = y14 & y17;
  Bit t14 = t13 ^ t12;
  Bit t15 = y8 & y10;
  Bit t16 = t15 ^ t12;
  Bit t17 = t4 ^ y20;
  Bit t18 = t6 ^ t16;
  Bit t19 = t9 ^ t14;
  Bit t20 = t11 ^ t16;
  Bit t21 = t17 ^ t14;
  Bit t22 = t18 ^ y19;
  Bit t23 = t19 ^ y21;
  Bit t24 = t20 ^ y18;
  Bit t25 = t21 ^ t22;
  Bit t26 = t21 & t23;
  Bit t27 = t24 ^ t26;
  Bit t28 = t25 & t27;
  Bit t29 = t28 ^ t22;
  Bit t30 = t23 ^ t24;
  Bit t31 = t22 ^ t26;
  Bit t32 = t31 & t30;
  Bit t33 = t32 ^ t24;
  Bit t34 = t23 ^ t33;
  Bit t35 = t27 ^ t33;
  Bit t36 = t24 & t35;
  Bit t37 = t36 ^ t34;
  Bit t38 = t27 ^ t36;
  Bit t39 = t29 & t38;
  Bit t40 = t25 ^ t39;
  Bit t41 = t40 ^ t37;
  Bit t42 = t29 ^ t33;
  Bit t43 = t29 ^ t40;
  Bit t44 = t33 ^ t37;
  Bit t45 = t42 ^ t41;
  Bit z0 = t44 & y15;
  Bit z1 = t37 & y6;
  Bit z2 = t33 & U7;
  Bit z3 = t43 & y16;
  Bit z4 = t40 & y1;
  Bit z5 = t29 & y7;
  Bit z6 = t42 & y11;
  Bit z7 = t45 & y17;
  Bit z8 = t41 & y10;
  Bit z9 = t44 & y12;
  Bit z10 = t37 & y3;
  Bit z11 = t33 & y4;
  Bit z12 = t43 & y13;
  Bit z13 = t40 & y5;
  Bit z14 = t29 & y2;
  Bit z15 = t42 & y9;
  Bit z16 = t45 & y14;
  Bit z17 = t41 & y8;
  Bit tc1 = z15 ^ z16;
  Bit tc2 = z10 ^ tc1;
  Bit tc3 = z9 ^ tc2;
  Bit tc4 = z0 ^ z2;
  Bit tc5 = z1 ^ z0;
  Bit tc6 = z3 ^ z4;
  Bit tc7 = z12 ^ tc4;
  Bit tc8 = z7 ^ tc6;
  Bit tc9 = z8 ^ tc7;
  Bit tc10 = tc8 ^ tc9;
  Bit tc11 = tc6 ^ tc5;
  Bit tc12 = z3 ^ z5;
  Bit tc13 = z13 ^ tc1;
  Bit tc14 = tc4 ^ tc12;
  Bit S3 = tc3 ^ tc11;
  Bit tc16 = z6 ^ tc8;
  Bit tc17 = z14 ^ tc10;
  Bit tc18 = !tc13 ^ tc14;
  Bit S7 = z12 ^ tc18;
  Bit tc20 = z15 ^ tc16;
  Bit tc21 = tc2 ^ z11;
  Bit S0 = tc3 ^ tc16;
  Bit S6 = tc10 ^ tc18;
  Bit S4 = tc14 ^ S3;
  Bit S1 = !(S3 ^ tc16);
  Bit tc26 = tc17 ^ tc20;
  Bit S2 = !(tc26 ^ z17);
  Bit S5 = tc21 ^ tc17;

  V[7] = S0;
  V[6] = S1;
  V[5] = S2;
  V[4] = S3;
  V[3] = S4;
  V[2] = S5;
  V[1] = S6;
  V[0] = S7;
}

///////////////////////////////.///
//
//        Key Schedule functions
//
//////////////////////////////////


void AES::KeySchedule(const Integer key) {
  assert (Nk * WordLen == key.size());
  roundKeys.resize(Nr + 1);

  std::vector<Integer> w(NumWords * (Nr + 1));

  for (int i = 0; i < Nk; i++) {
    w[i].bits = std::vector<Bit>(key.bits.begin() + i * WordLen, key.bits.begin() + (i+1) * WordLen);
  }

  for (int i = Nk; i < NumWords * (Nr + 1); i++) {
    Integer temp = w[i - 1];

    if (i % Nk == 0) {
      temp = SubWord(RotWord(temp)) ^ Rcon(i / Nk);
    }

    w[i] = w[i - Nk] ^ temp;
  }
  
  for (int r = 0; r <= Nr; r++) {
    for (int i = 0; i < NumRows; i++) {
      std::array<Integer, 4> bytes = word2bytes(w[r * NumRows + NumRows - i - 1]);
      for (int j = 0; j < NumColumns; j++) {
        roundKeys[r][i * NumColumns + j] = bytes[j];
      }
    }
  }
}

inline std::array<Integer, 4> AES::word2bytes(Integer word) {
  std::array<Integer, 4> bytes;
  for (int i = 0; i < 4; i++) {
    bytes[i].bits = std::vector<Bit>(word.bits.begin() + (3 - i) * 8, word.bits.begin() + (4 - i) * 8);
  }
  return bytes;
}

inline Integer AES::bytes2word(std::array<Integer, 4> bytes) {
  Integer word;
  for (int i = 0; i < 4; i++) {
    word.bits.insert(word.bits.end(), bytes[3-i].bits.begin(), bytes[3-i].bits.end());
  }
  return word;
}

Integer AES::SubWord(const Integer a) {
  std::array<Integer, 4> bytes = word2bytes(a);
  for (int i = 0; i < 4; i++) {
    SBox(bytes[i]);
  }
  return bytes2word(bytes);
}

Integer AES::RotWord(const Integer a) {
  auto bytes = word2bytes(a);
  std::array<Integer, 4> rot_bytes{bytes[3], bytes[0], bytes[1], bytes[2]};
  return bytes2word(rot_bytes);
}

Integer AES::xtime(Integer b)  // multiply on x
{
  Integer mask(8, 0);
  mask[0] = mask[1] = mask[3] = mask[4] = b[7];
  return (b << 1) ^ mask;
}

Integer AES::Rcon(unsigned int n) {
  if (Rcon_buffer.count(n))
    return Rcon_buffer[n];

  Integer rc(8, 1);
  
  for (int i = 0; i < n - 1; i++) {
    rc = xtime(rc);
  }

  rc.resize(WordLen, false);
  Rcon_buffer[n] = rc;
  return rc;
}

} // namespace sci