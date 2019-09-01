import argparse

import torch.distributed as dist
import torch.utils.data.distributed as datadist


def test_distributedsampler():
    # fake dataset
    dataset = list(range(12345))
    num_proc = 8
    rank = 0
    sampler = datadist.DistributedSampler(dataset=dataset,
                                          num_replicas=num_proc,
                                          duplicate_last=True,
                                          shuffle=False)


if __name__ == "__main__":
    test_distributedsampler()