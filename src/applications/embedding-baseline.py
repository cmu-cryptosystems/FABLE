import torch
import crypten
import time
import os
from argparse import ArgumentParser

import functools

def prefix_function(function, prefunction):
    @functools.wraps(function)
    def run(*args, **kwargs):
        prefunction(*args, **kwargs)
        return function(*args, **kwargs)
    return run

def count_arith(size0, size1, op, device=None, *args, **kwargs):
    assert len(size0) == 2 and len(size1) == 2 and size0[1] == size1[0]
    assert op == "matmul", op
    crypten.mpc.get_default_provider().num_arith_triples += size0[0] * size0[1] * size1[1]
    
def count_binary(size0, size1, device=None):
    # assert len(size0) == 2 and len(size1) == 2 and size0[1] == size1[0]
    assert size0 == size1, (size0, size1)
    crypten.mpc.get_default_provider().num_binary_triples += size0.numel()

provider = crypten.mpc.get_default_provider()
setattr(provider, "num_arith_triples", 0)
setattr(provider, "num_binary_triples", 0)
provider.generate_additive_triple = prefix_function(provider.generate_additive_triple, count_arith)
provider.generate_binary_triple = prefix_function(provider.generate_binary_triple, count_binary)

num_dimensions = 32
vocab_size = 519820
words_per_sample = 16
samples_per_batch = 32

torch.manual_seed(1)
word_embedding = torch.randn(vocab_size, num_dimensions)
input_sentences = torch.randint(vocab_size, (samples_per_batch, words_per_sample))

gt = word_embedding[input_sentences.flatten()].view(samples_per_batch, words_per_sample, num_dimensions).sum(1)
masks = torch.arange(vocab_size).view(1, 1, -1)
gt_wc_vec = (input_sentences.unsqueeze(-1).expand(-1, -1, vocab_size) == masks).sum(1)

ALICE = 0
BOB = 1

# @mpc.run_multiprocess(world_size=2)
def word_embedding_lookup():
    crypten.print(f"Experiment started. ")
    input_sentences_secret: crypten.CrypTensor = crypten.cryptensor(input_sentences, src = BOB)
    masks = torch.arange(vocab_size).view(1, 1, -1)

    # Obtain word-count vector
    start_time_wc = time.time()

    wc_vec = (input_sentences_secret.reshape(samples_per_batch, words_per_sample, 1).expand(-1, -1, vocab_size) == masks).sum(1)

    crypten.print(f"word-count vector computation: elapsed {time.time() - start_time_wc} s")
    # wc_vec = crypten.cryptensor(gt_wc_vec, src = BOB)

    start_time = time.time()

    word_embedding_secret: crypten.CrypTensor = crypten.cryptensor(word_embedding, src = ALICE)
    prod = wc_vec.matmul(word_embedding_secret)
    crypten.print(f"matrix multiplication: elapsed {time.time() - start_time} s")
    crypten.print(f"Total: elapsed {time.time() - start_time_wc} s")

    plain_prod = prod.get_plain_text()
    assert torch.isclose(plain_prod, gt, atol=1e-3).all(), (plain_prod - gt).abs().max()

def word_embedding_lookup_cleartext():

    # Obtain word-count vector
    start_time = time.time()
    masks = torch.arange(vocab_size).view(1, 1, -1).broadcast_to((samples_per_batch, words_per_sample, vocab_size))
    wc_vec = (input_sentences.unsqueeze(-1).broadcast_to((samples_per_batch, words_per_sample, vocab_size)) == masks).sum(1).float()
    print(f"word-count vector computation: elapsed {time.time() - start_time} s")
    start_time = time.time()

    prod = wc_vec.matmul(word_embedding)
    print(f"matrix multiplication: elapsed {time.time() - start_time} s")

    assert torch.isclose(prod, gt, atol=1e-5).all(), (prod - gt).abs().max()

if __name__ == '__main__':

    parser = ArgumentParser()
    parser.add_argument(
        "--rank", '-r',
        type=int,
        default=0,
        help="Rank of current process. [0 | 1]",
    )
    parser.add_argument(
        "--addr", '-a',
        type=str,
        default="127.0.0.1",
        help="The address of rank 0 machine",
    )
    parser.add_argument(
        "--port", '-p',
        type=int,
        default=12345,
        help="The port to assign",
    )
    args = parser.parse_args()
    os.environ.update({
        "WORLD_SIZE": "2", 
        "RANK": str(args.rank), 
        "RENDEZVOUS": f"tcp://{args.addr}:{args.port}", 
        "MASTER_ADDR": args.addr, 
        "MASTER_PORT": str(args.port + 1), 
    })

    crypten.init()
    torch.set_num_threads(32)

    word_embedding_lookup()
    print(f"Num arithmetic triples generated = {crypten.mpc.get_default_provider().num_arith_triples}")
    print(f"Num binary triples generated = {crypten.mpc.get_default_provider().num_binary_triples}")