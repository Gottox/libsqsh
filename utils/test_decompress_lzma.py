import lzma
import sys

print(lzma.open(sys.argv[1], format = lzma.FORMAT_ALONE).read())
