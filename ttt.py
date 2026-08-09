"""Play two-player tic-tac-toe in the terminal."""

from typing import List, Optional

WINNING_LINES = (
    (0, 1, 2), (3, 4, 5), (6, 7, 8),
    (0, 3, 6), (1, 4, 7), (2, 5, 8),
    (0, 4, 8), (2, 4, 6),
)


def render(board: List[str]) -> str:
    cells = [value or str(index + 1) for index, value in enumerate(board)]
    rows = [" | ".join(cells[index:index + 3]) for index in range(0, 9, 3)]
    return "\n---------\n".join(rows)


def winner(board: List[str]) -> Optional[str]:
    for first, second, third in WINNING_LINES:
        if board[first] and board[first] == board[second] == board[third]:
            return board[first]
    return None


def make_move(board: List[str], position: int, player: str) -> None:
    if position not in range(1, 10):
        raise ValueError("position must be from 1 to 9")
    if board[position - 1]:
        raise ValueError("that position is already occupied")
    board[position - 1] = player


def play() -> Optional[str]:
    board = [""] * 9
    player = "X"
    while not winner(board) and any(not cell for cell in board):
        print(f"\n{render(board)}\n")
        try:
            position = int(input(f"Player {player}, choose 1-9: "))
            make_move(board, position, player)
        except ValueError as error:
            print(f"Invalid move: {error}")
            continue
        player = "O" if player == "X" else "X"
    result = winner(board)
    print(f"\n{render(board)}")
    print(f"Player {result} wins!" if result else "It's a draw!")
    return result


if __name__ == "__main__":
    play()
