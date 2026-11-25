"""Independent dictionary-based oracle for logical wave partitions."""
import random

M = 0xffffffff
WORDS = 12


def vectors(seed):
    rng = random.Random(seed)
    edge = [0, (1<<64)-1, 1<<31, 1<<32, 1<<63, M, M<<32, 0x8000000180000001]
    masks = [mask for mask in edge for _ in range(8)]
    masks += [rng.getrandbits(64) for _ in range(2048)]
    inp, expected = [], []
    for g, mask in enumerate(masks):
        mode = g % 8
        if mode == 0: keys = [M]*64
        elif mode == 1: keys = list(range(64))
        elif mode == 2: keys = [i%2 for i in range(64)]
        elif mode == 3: keys = [i//32 for i in range(64)]
        elif mode == 4: keys = [0x80000000+(i%3) for i in range(64)]
        elif mode == 5: keys = [rng.randrange(8) for _ in range(64)]
        elif mode == 6: keys = [rng.getrandbits(32) for _ in range(64)]
        else: keys = [i%32 for i in range(64)]
        vals = [M if g<64 else rng.getrandbits(32) for _ in range(64)]
        parts = {}
        for i, key in enumerate(keys):
            if mask>>i&1: parts.setdefault(key, []).append(i)
        # Insertion order is the first-active-lane waterfall order.
        meta = {}
        for rank, (key, members) in enumerate(parts.items()):
            bits = sum(1<<i for i in members)
            meta[key] = (members[0], sum(vals[i] for i in members)&M, bits&M, bits>>32, rank)
        total = sum(vals[i] for i in range(64) if mask>>i&1)&M
        for i, key in enumerate(keys):
            active = (mask>>i)&1
            src = [0,31,32,63,64,M,rng.randrange(64),i][(i+g)%8]
            inp.extend((vals[i], active, key, src))
            joined = int(bool(active and src<64 and mask>>src&1))
            valid = int(bool(joined and keys[src]==key))
            expected.extend((active, valid, vals[src] if valid else 0,
                             *(meta[key] if active else (64,0,0,0,64)),
                             joined, vals[src] if joined else 0,
                             total if active else 0, len(parts)))
    return masks, inp, expected


if __name__ == "__main__":
    import argparse
    from array import array
    from pathlib import Path

    parser = argparse.ArgumentParser(description="Write native uint32 vectors for the Vulkan partition test")
    parser.add_argument("inputs", type=Path)
    parser.add_argument("reference", type=Path)
    args = parser.parse_args()
    _, inputs, reference = vectors(0x57415645)
    for path, values in ((args.inputs, inputs), (args.reference, reference)):
        words = array("I", values)
        if words.itemsize != 4:
            raise RuntimeError("the partition fixture requires 32-bit unsigned integers")
        with path.open("wb") as output:
            words.tofile(output)
