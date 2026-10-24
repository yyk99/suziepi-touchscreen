import glob
import os
import sys

root = sys.argv[1]
for path in sorted(glob.glob(os.path.join(root, 'src', '**', '*.c'), recursive=True)):
    print(os.path.relpath(path, root))
