"""Shell-free bounded OpenSSL invocation. Caller-selected executable is trusted."""
import os
import selectors
import signal
import subprocess
import time

ALGORITHMS = ('ML-KEM-768', 'ML-KEM-1024', 'ML-DSA-65', 'ML-DSA-87')

def run(arguments, input_fd=None):
    exe = os.environ.get('PYDEV_OPENSSL', 'openssl')
    if 'PYDEV_OPENSSL' in os.environ and not os.path.isabs(exe):
        raise ValueError('PYDEV_OPENSSL must be an absolute executable path')
    env = dict(os.environ, OPENSSL_CONF='/dev/null')
    env.pop('OPENSSL_MODULES', None)
    env.pop('OPENSSL_ENGINES', None)
    data = bytearray()
    deadline = time.monotonic() + 10
    with subprocess.Popen([exe] + arguments, stdin=input_fd if input_fd is not None else subprocess.DEVNULL,
                          stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
                          env=env, start_new_session=True) as proc:
        try:
            with selectors.DefaultSelector() as selector:
                selector.register(proc.stdout, selectors.EVENT_READ)
                while True:
                    remaining = deadline - time.monotonic()
                    if remaining <= 0:
                        raise ValueError('OpenSSL timeout')
                    if not selector.select(remaining):
                        raise ValueError('OpenSSL timeout')
                    chunk = os.read(proc.stdout.fileno(),4096)
                    if not chunk:
                        break
                    if len(data) + len(chunk) > 65536:
                        raise ValueError('OpenSSL output budget exceeded')
                    data.extend(chunk)
            if proc.wait(timeout=max(0.001,deadline-time.monotonic())):
                raise ValueError('OpenSSL operation failed or algorithm unavailable')
            return data
        except BaseException:
            try:
                os.killpg(proc.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            proc.wait()
            data[:] = b'\0' * len(data)
            raise

def save(directory, name, data):
    fd = os.open(name, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o600, dir_fd=directory)
    try:
        view = memoryview(data)
        while view:
            count = os.write(fd, view)
            if count <= 0:
                raise OSError('short write')
            view = view[count:]
        os.fsync(fd)
    finally:
        os.close(fd)
