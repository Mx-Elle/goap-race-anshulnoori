from copy import deepcopy
from enum import Enum
import sys
from time import monotonic
from typing import Callable

import pygame
import pygame.locals

from game_world.racetrack import RaceTrack, load_track
from random_bot import random_move
import traceback

TRACK = load_track("./tracks/rooms.pkl")
PLAYER = random_move
REPLAY_SPEED = 0.05  # seconds per move in the replay. (lower is faster)
SHOW_REPLAY = True


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
        max_turns_without_progress: int = 100,
    ) -> None:
        self.player = player
        self.track = deepcopy(track)
        self.time = time
        self.delay = delay
        self.turns_without_progress = 0
        self.max_turns_without_progress = max_turns_without_progress
        self.pos = track.spawn
        self.min_dist = float("inf")
        self.history = []
        self.surface = self.track.render()

    def tick(self) -> tuple[Status, str, pygame.Surface]:
        start_time = monotonic()
        try:
            action = self.player(self.pos, self.track)
        except Exception as e:
            return (
                Status.DNF,
                f"Racer crashed with the following error message:\n{traceback.format_exc()}",
                self.surface,
            )
        time_taken = monotonic() - start_time
        self.time -= time_taken
        self.history.append(action)
        if self.time < 0:
            return Status.DNF, "Timed Out", self.surface
        self.time += min(time_taken, self.delay)
        if self.track.buttons[self.pos]:
            self.track.toggle(self.track.button_colors[self.pos])
        options = {(1, 0), (-1, 0), (0, 1), (0, -1)}
        self.pos = (self.pos[0] + action[0], self.pos[1] + action[1])
        if self.track.buttons[self.pos]:
            dupe = deepcopy(self.track)
            dupe.toggle(self.track.button_colors[self.pos])
            self.surface = dupe.render()
        if action not in options:
            return Status.DNF, f"Racer made illegal move {action}!", self.surface
        if not (
            self.pos[0] in range(self.track.shape[0])
            and self.pos[1] in range(self.track.shape[1])
        ):
            return Status.DNF, "Racer went out of bounds!", self.surface
        if not self.pos in self.track.find_traversable_cells():
            return Status.DNF, "Racer crashed into a wall!", self.surface
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
                    self.surface,
                )
        if self.pos == self.track.target:
            return (
                Status.FINISH,
                f"Racer made it to the finish line in {len(self.history)} steps!",
                self.surface,
            )
        return Status.ONGOING, "Still racing.", self.surface


def interpolate(start: Point, end: Point, p: float) -> tuple[float, float]:
    return (start[0] * (1 - p) + end[0] * p, start[1] * (1 - p) + end[1] * p)


def play_visible(track: RaceTrack, time_per_move: float):
    fps = 60
    fps_clock = pygame.time.Clock()
    pygame.init()
    screen = pygame.display.set_mode(track.screen_size)
    done = False
    status = Status.ONGOING
    cell_w = track.screen_size[0] / track.shape[1]
    cell_h = track.screen_size[1] / track.shape[0]

    most_recent_arrow = []

    def human_player(loc: Point, track: RaceTrack | None) -> Point:
        arrow = most_recent_arrow[-1]
        most_recent_arrow.clear()
        move_dict = {
            pygame.K_UP: (-1, 0),
            pygame.K_DOWN: (1, 0),
            pygame.K_LEFT: (0, -1),
            pygame.K_RIGHT: (0, 1),
        }
        return move_dict[arrow]

    game = Game(human_player, track, float("inf"), 0)
    track_surface = game.surface
    dt = 0
    p = 1
    move_start, move_end = game.pos, game.pos

    while True:

        p = min(p + dt / time_per_move, 1)
        if p >= 1:
            if done:
                break
            if most_recent_arrow:
                status, msg, track_surface = game.tick()
                most_recent_arrow.clear()
            if status != Status.ONGOING:
                print(msg)
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
                done = True
            elif event.type == pygame.locals.KEYDOWN:
                if event.key in {
                    pygame.K_UP,
                    pygame.K_DOWN,
                    pygame.K_RIGHT,
                    pygame.K_LEFT,
                }:
                    most_recent_arrow.append(event.key)

        pygame.display.flip()
        dt = fps_clock.tick(fps) / 1000


def main():
    play_visible(TRACK, REPLAY_SPEED)


if __name__ == "__main__":
    main()
