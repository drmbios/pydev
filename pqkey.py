"""Generate an OpenSSL PQ keypair in a new private directory, without overwrite."""
import os
import stat
import subprocess
import sys
from pqcommon import ALGORITHMS, run, save

def main():
    directory, key, data = None, None, bytearray()
    try:
        if len(sys.argv) != 3 or sys.argv[1] not in ALGORITHMS:
            raise ValueError('usage: pqkey.py {ML-KEM-768|ML-KEM-1024|ML-DSA-65|ML-DSA-87} NEW_DIRECTORY')
        os.mkdir(sys.argv[2],0o700)
        directory = os.open(sys.argv[2],os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
        info = os.fstat(directory)
        if info.st_uid != os.geteuid() or stat.S_IMODE(info.st_mode) & 0o077:
            raise ValueError('directory must be owner-only')
        data = run(['genpkey','-provider','default','-algorithm',sys.argv[1]])
        if not data:
            raise ValueError('empty key')
        save(directory,'private.pem',data)
        data[:] = b'\0' * len(data)
        key = os.open('private.pem',os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK,dir_fd=directory)
        info = os.fstat(key)
        if not stat.S_ISREG(info.st_mode) or info.st_size > 65536:
            raise ValueError('key must be a bounded regular file')
        data = run(['pkey','-provider','default','-pubout'],key)
        if not data:
            raise ValueError('empty public key')
        save(directory,'public.pem',data)
        print('Created private.pem (UNENCRYPTED, mode 0600) and public.pem. Protect and back up the private key.')
        return 0
    except (OSError, ValueError, subprocess.TimeoutExpired) as exc:
        print(f'{exc}\nKey creation incomplete; any newly created directory/files were retained for inspection. No existing files overwritten.',file=sys.stderr)
        return 2
    finally:
        data[:] = b'\0' * len(data)
        if key is not None:
            os.close(key)
        if directory is not None:
            os.close(directory)
if __name__ == '__main__':
    raise SystemExit(main())
