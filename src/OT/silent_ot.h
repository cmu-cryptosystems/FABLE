#ifndef BATCHLUT_SILENT_OT_H__
#define BATCHLUT_SILENT_OT_H__

#include <cstdint>
#include "libOTe/TwoChooseOne/Silent/SilentOtExtSender.h"
#include "libOTe/TwoChooseOne/Silent/SilentOtExtReceiver.h"
#include <cryptoTools/Common/BitVector.h>
#include <vector>
#include "coproto/Socket/AsioSocket.h"

using namespace oc;

enum class Role
{
    Sender,
    Receiver
};

const u64 POWER_MAX = 32;

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
struct SilentOTResultClient_N {
  std::vector<block> message;
  std::vector<u64> choices;
};

SilentOTResultServer SilentOT_1_out_of_2_server(u64 numOTs, coproto::AsioSocket& chl, u64 numThreads = 1, SilentBaseType type = SilentBaseType::BaseExtend, MultType multType = MultType::ExConv7x24);

SilentOTResultClient SilentOT_1_out_of_2_client(u64 numOTs, coproto::AsioSocket& chl, u64 numThreads = 1, SilentBaseType type = SilentBaseType::BaseExtend, MultType multType = MultType::ExConv7x24);


// 1 out of N = 2^power silent OT
SilentOTResultServer_N SilentOT_1_out_of_N_server(u64 numOTs, u64 numThreads, coproto::AsioSocket& chl, uint64_t power, SilentBaseType type = SilentBaseType::BaseExtend, MultType multType = MultType::ExConv7x24);
SilentOTResultClient_N SilentOT_1_out_of_N_client(u64 numOTs, u64 numThreads, coproto::AsioSocket& chl, uint64_t power, SilentBaseType type = SilentBaseType::BaseExtend, MultType multType = MultType::ExConv7x24);

#endif