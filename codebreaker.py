"""Play a Mastermind-style four-digit code-breaking game."""

import argparse
import random
from typing import Optional, Tuple

CODE_LENGTH = 4


def generate_code(rng: Optional[random.Random] = None) -> str:
    return "".join((rng or random).sample("0123456789", CODE_LENGTH))


def score_guess(code: str, guess: str) -> Tuple[int, int]:
    exact = sum(expected == actual for expected, actual in zip(code, guess))
    common = sum(min(code.count(digit), guess.count(digit)) for digit in set(guess))
    return exact, common - exact


def valid_guess(guess: str) -> bool:
    return len(guess) == CODE_LENGTH and guess.isdigit()


def play(attempts: int = 8, code: Optional[str] = None) -> bool:
    secret = code or generate_code()
    used = 0
    while used < attempts:
        guess = input("Guess the 4-digit code: ").strip()
        if not valid_guess(guess):
            print("Enter exactly four digits (leading zeroes are allowed).")
            continue
        used += 1
        if guess == secret:
            print(f"Unlocked in {used} attempt(s)!")
            return True
        exact, misplaced = score_guess(secret, guess)
        print(f"{exact} exact, {misplaced} misplaced; {attempts - used} attempt(s) left.")
    print(f"Game over. The code was {secret}.")
    return False


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--attempts", type=int, default=8)
    args = parser.parse_args()
    if args.attempts < 1:
        parser.error("--attempts must be positive")
    play(args.attempts)


if __name__ == "__main__":
    main()
