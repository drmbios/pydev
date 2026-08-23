"""Display Python-accessible system clock resolutions."""

import time


def resolutions() -> dict[str, float]:
    result = {}
    for name in ("time", "monotonic", "perf_counter", "process_time", "thread_time"):
        try:
            result[name] = time.get_clock_info(name).resolution
        except ValueError:
            pass
    return result


if __name__ == "__main__":
    for name, value in resolutions().items():
        print(f"{name}: {value:.12g} seconds")
