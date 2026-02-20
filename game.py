from copy import deepcopy
from enum import Enum
import sys
from time import monotonic
from typing import Callable

import pygame
import pygame.locals

from game_world.racetrack import RaceTrack, load_track
from potato_agent import get_move
import traceback

TRACK = load_track("./tracks/your_room.pkl")
PLAYER = get_move
REPLAY_SPEED = 0.1  # seconds per move in the replay. (lower is faster)
SHOW_REPLAY = True
CLOCK = 10
DELAY = 5
MAX_TURNS_WITHOUT_PROGRESS = None  # None means no limit


Point = tuple[int, int]
Player = Callable[
    [Point, RaceTrack], Point
]  # (location, velocity, track) -> change_in_velocity


class Status(Enum):
    ONGOING = 1
    FINISH = 2
    DNF = 3


def manhattan_dist(a: Point, b: Point) -> int:
    return abs(a[0] - b[0]) + abs(a[1] - b[1])


class Game:

    def __init__(
        self,
        player: Player,
        track: RaceTrack,
        time: float,
        delay: float,
        max_turns_without_progress: int | None = None,
    ) -> None:
        self.player = player
        self.track = deepcopy(track)
        self.time = time
        self.delay = delay
        self.turns_without_progress = 0
        self.max_turns_without_progress = (
            max_turns_without_progress if max_turns_without_progress else float("inf")
        )
        self.pos = track.spawn
        self.min_dist = float("inf")
        self.history = []

    def tick(self) -> tuple[Status, str]:
        track_copy = deepcopy(self.track)
        if track_copy.buttons[self.pos]:
            track_copy.toggle(self.track.button_colors[self.pos])
        start_time = monotonic()
        try:
            action = self.player(self.pos, track_copy)
        except Exception as e:
            return (
                Status.DNF,
                f"Racer crashed with the following error message:\n{traceback.format_exc()}",
            )
        time_taken = monotonic() - start_time
        self.time -= time_taken
        self.history.append(action)
        if self.time < 0:
            return Status.DNF, "Timed Out"
        self.time += min(time_taken, self.delay)
        options = {(1, 0), (-1, 0), (0, 1), (0, -1)}
        if action not in options:
            return Status.DNF, f"Racer made illegal move {action}!"
        if self.track.buttons[self.pos]:
            self.track.toggle(self.track.button_colors[self.pos])
        self.pos = (self.pos[0] + action[0], self.pos[1] + action[1])
        if not (
            self.pos[0] in range(self.track.shape[0])
            and self.pos[1] in range(self.track.shape[1])
        ):
            return Status.DNF, "Racer went out of bounds!"
        if not self.pos in self.track.find_traversable_cells():
            return Status.DNF, "Racer crashed into a wall!"
        new_dist = manhattan_dist(self.pos, self.track.target)
        if new_dist < self.min_dist:
            self.min_dist = new_dist
            self.turns_without_progress = 0
        else:
            self.turns_without_progress += 1
            if self.turns_without_progress >= self.max_turns_without_progress:
                return (
                    Status.DNF,
                    f"Racer spent {self.turns_without_progress} ticks dawdling!",
                )
        if self.pos == self.track.target:
            return (
                Status.FINISH,
                f"Racer made it to the finish line in {len(self.history)} steps!",
            )
        return Status.ONGOING, "Still racing."

    def play_game(self) -> tuple[Status, str]:
        status, msg = Status.ONGOING, "Just Started."
        while status == Status.ONGOING:
            status, msg = self.tick()
        return status, msg


def replay_player_generator(history: list[Point]) -> Player:
    def replay(loc: Point, track: RaceTrack) -> Point:
        if history:
            return history.pop(0)
        return (0, 0)

    return replay


def interpolate(start: Point, end: Point, p: float) -> tuple[float, float]:
    return (start[0] * (1 - p) + end[0] * p, start[1] * (1 - p) + end[1] * p)


def watch_replay(track: RaceTrack, history: list[Point], time_per_move: float):
    cell_w = track.screen_size[0] / track.shape[1]
    cell_h = track.screen_size[1] / track.shape[0]

    replay_player = replay_player_generator(history)
    game = Game(replay_player, track, float("inf"), float("inf"), None)
    dt = 0
    p = 1
    move_start, move_end = game.pos, game.pos

    fps = 60
    fps_clock = pygame.time.Clock()
    pygame.init()
    screen = pygame.display.set_mode(track.screen_size)
    done = False
    track_surface = game.track.render()

    while True:

        p = min(p + dt / time_per_move, 1)
        track_surface = game.track.render()
        if p >= 1:
            if done:
                break
            status, _ = game.tick()
            if status != Status.ONGOING:
                done = True
            move_start, move_end = move_end, game.pos
            p = 0
        player_location = interpolate(move_start, move_end, p)
        x, y = (player_location[1] + 0.5) * cell_w, (player_location[0] + 0.5) * cell_h
        screen.blit(track_surface, (0, 0))
        pygame.draw.circle(screen, "#000000", (x, y), 0.2 * min(cell_w, cell_h))
        pygame.draw.circle(screen, "#FFFFFF", (x, y), 0.2 * min(cell_w, cell_h), 2)

        for event in pygame.event.get():
            if event.type == pygame.locals.QUIT:
                pygame.quit()
                sys.exit()

        pygame.display.flip()
        dt = fps_clock.tick(fps) / 1000


def main():
    game = Game(PLAYER, TRACK, CLOCK, DELAY, MAX_TURNS_WITHOUT_PROGRESS)
    _, msg = game.play_game()
    if SHOW_REPLAY:
        watch_replay(TRACK, game.history, REPLAY_SPEED)
    print(msg)


if __name__ == "__main__":
    main()
