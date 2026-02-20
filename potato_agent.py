from __future__ import annotations

import numpy as np
from game_world.racetrack import RaceTrack
from potato_solver import Solver


class PotatoAgent:
    __slots__ = ("_path", "_last_target")

    def __init__(self) -> None:
        self._path: list[tuple[int, int]] = []
        self._last_target: tuple[int, int] | None = None

    def _plan(self, loc: tuple[int, int], track: RaceTrack) -> None:
        walls = np.ascontiguousarray(track.walls, dtype=np.uint8)
        active = np.ascontiguousarray(track.active, dtype=np.uint8)
        buttons = np.ascontiguousarray(track.buttons, dtype=np.uint8)
        wall_colors = np.ascontiguousarray(track.wall_colors, dtype=np.int32)
        button_colors = np.ascontiguousarray(track.button_colors, dtype=np.int32)

        solver = Solver(
            walls,
            active,
            buttons,
            wall_colors,
            button_colors,
            int(track.target[0]),
            int(track.target[1]),
        )
        self._path = list(solver.solve(int(loc[0]), int(loc[1])))
        self._last_target = track.target

    def move(self, loc: tuple[int, int], track: RaceTrack) -> tuple[int, int]:
        if track.target != self._last_target:
            self._plan(loc, track)

        while self._path and self._path[0] == loc:
            self._path.pop(0)

        if not self._path:
            return (0, 0)

        next_pos = self._path[0]
        dy = max(-1, min(1, next_pos[0] - loc[0]))
        dx = max(-1, min(1, next_pos[1] - loc[1]))
        return (dy, dx)


_agent = PotatoAgent()


def get_move(loc: tuple[int, int], track: RaceTrack) -> tuple[int, int]:
    return _agent.move(loc, track)
