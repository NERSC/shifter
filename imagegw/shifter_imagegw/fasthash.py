#!/usr/bin/env python

import hashlib
import argparse


def fast_hash(infile):
    """
    Calculate a sha256 hash of the first MB of a file and every 512MB offset
    """

    m = hashlib.sha256()
    with open(infile, 'rb', 1024 * 1024) as f:
        l = f.read(1024 * 1024)
        while (len(l) > 0):
            m.update(l)
            f.seek(1024 * 1024 * (512 - 1), 1)
            l = f.read(1024 * 1024)
    return m.hexdigest()


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Calculate file hash')
    parser.add_argument('infile', nargs=1, type=str)

    args = parser.parse_args()
    infile = args.infile[0]

    print fast_hash(infile)
