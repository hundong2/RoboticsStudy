"""
Occupancy grid mapping toy.

Why this matters:
    SLAM maps are often occupancy grids. This toy shows the core intuition:
    cells along a laser ray become free, and the hit cell becomes occupied.

Run:
    python slam_nav2/04_occupancy_grid_toy.py
"""

from __future__ import annotations


UNKNOWN = 0
FREE = -1
OCCUPIED = 1
ROBOT = 2


def draw_grid(grid: list[list[int]]) -> None:
    symbols = {UNKNOWN: "?", FREE: ".", OCCUPIED: "#", ROBOT: "R"}
    for row in grid:
        print(" ".join(symbols[v] for v in row))


def mark_horizontal_ray(grid: list[list[int]], robot: tuple[int, int], hit: tuple[int, int]) -> None:
    rx, ry = robot
    hx, hy = hit
    if ry != hy:
        raise ValueError("This simple toy only supports a horizontal ray")
    step = 1 if hx > rx else -1
    for x in range(rx + step, hx, step):
        grid[ry][x] = FREE
    grid[hy][hx] = OCCUPIED
    grid[ry][rx] = ROBOT


def main() -> None:
    width, height = 14, 7
    grid = [[UNKNOWN for _ in range(width)] for _ in range(height)]
    robot = (2, 3)
    hit = (10, 3)

    print("Before observation")
    draw_grid(grid)

    print("\nAfter one LiDAR ray")
    mark_horizontal_ray(grid, robot, hit)
    draw_grid(grid)

    print("\nInterpretation")
    print("- R is robot position.")
    print("- . cells are likely free because the ray passed through them.")
    print("- # is likely occupied because the ray ended there.")
    print("- ? cells are still unknown.")
    print("\nReal SLAM repeats this idea over many rays while also estimating robot pose.")


if __name__ == "__main__":
    main()
