import torch
import crypten
import time
import os
from argparse import ArgumentParser

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
    torch.set_num_threads(1)

    word_embedding_lookup()