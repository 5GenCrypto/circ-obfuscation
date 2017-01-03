#!/usr/bin/env python2

import tempfile, sys

def main(argv):
    if len(argv) != 2:
        print('Usage: %s <file>' % argv[0])
        sys.exit(1)
    fname = argv[1]
    newfile = tempfile.TemporaryFile()
    outputs = []
    with open(fname, 'r') as f:
        while True:
            try:
                line = f.next().strip()
            except StopIteration:
                break
            if line.startswith('# TEST'):
                items = line.split()
                newfile.write(':test %s %s\n' % (items[2], items[3]))
            if line.startswith(':nins'):
                continue
            if line.startswith(':depth'):
                continue
            if 'input x' in line:
                items = line.split()
                newfile.write('%s %s %s\n' % (items[0], items[1], items[2].split('x')[1]))
            if 'input y' in line:
                items = line.split()
                newfile.write('%s const %s\n' % (items[0], items[3]))
            if 'gate' in line:
                items = line.split()
                newfile.write('%s %s %s %s\n' % (items[0], items[2], items[3], items[4]))
            if 'output' in line:
                n, _, rest = line.split(' ', 2)
                outputs.append(n)
                newfile.write('%s\n' % (' '.join([n, rest])))
    newfile.write(':outputs ' + ' '.join(outputs) + '\n')
    newfile.seek(0)
    with open(fname, 'w') as f:
        for line in newfile:
            f.write(line)

if __name__ == '__main__':
    try:
        main(sys.argv)
    except KeyboardInterrupt:
        pass
