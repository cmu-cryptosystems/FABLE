#ifndef FABLE_SILENT_OT_H__
#define FABLE_SILENT_OT_H__

#include <cstdint>
#include "libOTe/TwoChooseOne/Silent/SilentOtExtSender.h"
#include "libOTe/TwoChooseOne/Silent/SilentOtExtReceiver.h"
#include <cryptoTools/Common/BitVector.h>
#include <vector>
#include "coproto/Socket/AsioSocket.h"
#include <bitset>

using namespace oc;

enum class Role
{
    Sender,
    Receiver
};

const u64 POWER_MAX = 32;

inline uint32_t blocktoint(block b) {
  return b.get<uint32_t>()[0];
}
struct SilentOTResultServer {
  std::vector<std::array<block, 2>> messages;
};
struct SilentOTResultClient {
  std::vector<block> messages;
  BitVector choices;
};
struct SilentOTResultServer_N {
  std::vector<std::vector<block>> messages;
};
struct SilentOTResultServer_N_Compressed {
  std::vector<std::array<AES, 2>> AESs;
  uint64_t power;

  inline uint32_t get_mask(int k, int i) {
    block b = ZeroBlock;
    std::bitset<POWER_MAX> bits(i);
    for (int j = 0; j < power; j++) {
        b = b ^ AESs[k*power+j][bits[j]].ecbEncBlock(block(i));
    }
    return b.get<uint32_t>()[0];
  }
};

struct SilentOTResultClient_N {
  std::vector<block> message;
  std::vector<u64> choices;
};

SilentOTResultServer SilentOT_1_out_of_2_server(u64 numOTs, coproto::AsioSocket& chl, u64 numThreads = 1, SilentBaseType type = SilentBaseType::BaseExtend, MultType multType = MultType::ExConv7x24);

SilentOTResultClient SilentOT_1_out_of_2_client(u64 numOTs, coproto::AsioSocket& chl, u64 numThreads = 1, SilentBaseType type = SilentBaseType::BaseExtend, MultType multType = MultType::ExConv7x24);


// 1 out of N = 2^power silent OT
SilentOTResultServer_N SilentOT_1_out_of_N_server(u64 numOTs, u64 numThreads, coproto::AsioSocket& chl, uint64_t power, SilentBaseType type = SilentBaseType::BaseExtend, MultType multType = MultType::ExConv7x24);
SilentOTResultServer_N_Compressed SilentOT_1_out_of_N_server_Compressed(u64 numOTs, u64 numThreads, coproto::AsioSocket& chl, uint64_t power, SilentBaseType type = SilentBaseType::BaseExtend, MultType multType = MultType::ExConv7x24);
SilentOTResultClient_N SilentOT_1_out_of_N_client(u64 numOTs, u64 numThreads, coproto::AsioSocket& chl, uint64_t power, SilentBaseType type = SilentBaseType::BaseExtend, MultType multType = MultType::ExConv7x24);

#endif