#!/usr/bin/env python3
import argparse, os, struct, random

MAGIC = b"NOXNET1\x00"

def write_f32(f, arr):
    f.write(struct.pack("<%sf" % len(arr), *arr))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out", help="output .nox path")
    ap.add_argument("--input-dim", type=int, default=782)
    ap.add_argument("--h1", type=int, default=512)
    ap.add_argument("--h2", type=int, default=64)
    ap.add_argument("--seed", type=int, default=1234)
    ap.add_argument("--scale", type=float, default=0.01, help="init scale for weights")
    args = ap.parse_args()

    random.seed(args.seed)

    in_dim, h1, h2, out_dim = args.input_dim, args.h1, args.h2, 1

    w1 = [(random.random()*2-1)*args.scale for _ in range(in_dim*h1)]
    b1 = [0.0 for _ in range(h1)]
    w2 = [(random.random()*2-1)*args.scale for _ in range(h1*h2)]
    b2 = [0.0 for _ in range(h2)]
    w3 = [(random.random()*2-1)*args.scale for _ in range(h2*out_dim)]
    b3 = [0.0]

    with open(args.out, 'wb') as f:
        f.write(MAGIC)
        f.write(struct.pack('<I', 1))           # version
        f.write(struct.pack('<I', in_dim))
        f.write(struct.pack('<I', h1))
        f.write(struct.pack('<I', h2))
        f.write(struct.pack('<I', out_dim))
        write_f32(f, w1)
        write_f32(f, b1)
        write_f32(f, w2)
        write_f32(f, b2)
        write_f32(f, w3)
        write_f32(f, b3)

    print(f"wrote {args.out} (dims {in_dim}-{h1}-{h2}-1)")

if __name__ == "__main__":
    main()
