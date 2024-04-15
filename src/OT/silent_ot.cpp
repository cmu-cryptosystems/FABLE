#include "silent_ot.h"
#include <bitset>

#include <coproto/Common/macoro.h>
#include <cryptoTools/Crypto/AES.h>
#include "utils/io_utils.h"

SilentOTResultServer SilentOT_1_out_of_2_server(u64 numOTs, coproto::AsioSocket& chl, u64 numThreads, SilentBaseType type, MultType multType)
{
    assert (numOTs > 0);
    // get up the networking

    PRNG prng(sysRandomSeed());

    macoro::thread_pool threadPool;
    auto work = threadPool.make_work();
    if (numThreads > 1)
        threadPool.create_threads(numThreads);

    SilentOtExtSender sender;

    // optionally request the LPN encoding matrix.
    sender.mMultType = multType;

    // optionally configure the sender. default is semi honest security.
    sender.configure(numOTs, 2, numThreads, SilentSecType::SemiHonest);

    // optional. You can request that the base ot are generated either
    // using just base OTs (few rounds, more computation) or 128 base OTs and then extend those.
    // The default is the latter, base + extension.
    cp::sync_wait(sender.genSilentBaseOts(prng, chl, type == SilentBaseType::BaseExtend));

    std::vector<std::array<block, 2>> messages(numOTs);

    // create the protocol object.
    auto protocol = sender.silentSend(messages, prng, chl);

    // run the protocol
    if (numThreads <= 1)
        cp::sync_wait(protocol);
    else
        cp::sync_wait(std::move(protocol) | macoro::start_on(threadPool));


    cp::sync_wait(chl.flush());

    return SilentOTResultServer{messages};

}

SilentOTResultClient SilentOT_1_out_of_2_client(u64 numOTs, coproto::AsioSocket& chl, u64 numThreads, SilentBaseType type, MultType multType)
{
    assert (numOTs > 0);
    // get up the networking

    PRNG prng(sysRandomSeed());

    macoro::thread_pool threadPool;
    auto work = threadPool.make_work();
    if (numThreads > 1)
        threadPool.create_threads(numThreads);

    SilentOtExtReceiver recver;

    // optionally request the LPN encoding matrix.
    recver.mMultType = multType;

    // configure the sender. optional for semi honest security...
    recver.configure(numOTs, 2, numThreads, SilentSecType::SemiHonest);

    // optional. You can request that the base ot are generated either
    // using just base OTs (few rounds, more computation) or 128 base OTs and then extend those.
    // The default is the latter, base + extension.
    cp::sync_wait(recver.genSilentBaseOts(prng, chl, type == SilentBaseType::BaseExtend));

    std::vector<block> messages(numOTs);
    BitVector choices(numOTs);

    // create the protocol object.
    auto protocol = recver.silentReceive(choices, messages, prng, chl);

    // run the protocol
    if (numThreads <= 1)
        cp::sync_wait(protocol);
    else
        // launch the protocol on the thread pool.
        cp::sync_wait(std::move(protocol) | macoro::start_on(threadPool));

    // choices, messages has been populated with random OT messages.
    // messages[i] = sender.message[i][choices[i]]
    // See the header for other options.

    cp::sync_wait(chl.flush());

    return SilentOTResultClient{
        messages,
        choices
    };
}

SilentOTResultServer_N SilentOT_1_out_of_N_server(u64 numOTs, u64 numThreads, coproto::AsioSocket& chl, uint64_t power, SilentBaseType type, MultType multType) {

    assert (power <= POWER_MAX);

    auto messages = SilentOT_1_out_of_2_server(numOTs * power, chl, numThreads, type, multType);

    u64 size = 1ULL << power;
    SilentOTResultServer_N result;
    result.messages.resize(numOTs, std::vector<block>(size, ZeroBlock));

    auto prng_seed = sysRandomSeed();
    cp::sync_wait(chl.send(prng_seed));
    PRNG prng(prng_seed);
    std::vector<block> iv_seeds(numOTs);
    prng.get(iv_seeds.data(), numOTs);

    # pragma omp parallel for if (numThreads > 1) num_threads(numThreads)
    for (int k = 0; k < numOTs; k++) {
        PRNG prng_k(iv_seeds[k]);
        for (int i = 0; i < size; i++) {
            std::bitset<POWER_MAX> bits(i);
            std::vector<block> seed;
            for (int j = 0; j < power; j++) {
                seed.push_back(messages.messages[k*power+j][bits[j]]);
            }
            result.messages[k][i] = AES_CBC(seed, prng_k.get<block>());
        }
    }

    return result;
}

SilentOTResultClient_N SilentOT_1_out_of_N_client(u64 numOTs, u64 numThreads, coproto::AsioSocket& chl, uint64_t power, SilentBaseType type, MultType multType) {

    assert (power <= POWER_MAX);

    auto messages = SilentOT_1_out_of_2_client(numOTs * power, chl, numThreads, type, multType);

    std::vector<std::bitset<POWER_MAX>> selection_bits(numOTs);
    for (int k = 0; k < numOTs; k++)
        for (int j = 0; j < power; j++)
            selection_bits[k][j] = messages.choices[k*power+j];

    u64 size = 1ULL << power;
    SilentOTResultClient_N result{std::vector<block>(numOTs)};
    
    for (int k = 0; k < numOTs; k++) {
        result.choices.push_back(selection_bits[k].to_ullong());
    }

    block prng_seed;
    cp::sync_wait(chl.recv(prng_seed));
    PRNG prng(prng_seed);
    std::vector<block> iv_seeds(numOTs);
    prng.get(iv_seeds.data(), numOTs);
    std::vector<block> iv(numOTs);

    for (int k = 0; k < numOTs; k++) {
        PRNG prng_k(iv_seeds[k]);
        for (int i = 0; i < size; i++) {
            auto temp = prng_k.get<block>();
            if (i == result.choices[k]) 
                iv[k] = temp;
        }
    }

    for (int k = 0; k < numOTs; k++) {
        std::vector<block> slice(messages.messages.begin() + k*power, messages.messages.begin() + (k+1)*power);
        result.message[k] = AES_CBC(slice, iv[k]);
    }

    return result;
}