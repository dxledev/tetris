#include "game.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <set>
#include <string>
#include <vector>

namespace tetris {

struct GameTestAccess {
    static void setActive(Game& game, PieceType type, int x, int y, int rotation = 0) {
        for (auto& row : game.board_) {
            row.fill(PieceType::Empty);
        }
        game.active_ = Game::ActivePiece{type, x, y, rotation};
        game.running_ = true;
        game.paused_ = false;
        game.gameOver_ = false;
        game.gravityElapsedMs_ = 0;
        game.lockElapsedMs_ = 0;
        game.lockResets_ = 0;
    }

    static void fillRowExcept(Game& game, int row, int emptyX1, int emptyX2) {
        for (int x = 0; x < Game::Width; ++x) {
            if (x != emptyX1 && x != emptyX2) {
                game.board_[row][x] = PieceType::J;
            }
        }
    }

    static void setCell(Game& game, int x, int y, PieceType type) {
        game.board_[y][x] = type;
    }
};

} // namespace tetris

namespace {

using namespace tetris;

void check(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

std::vector<PieceType> firstPieces(Game& game, int count) {
    std::vector<PieceType> pieces;
    for (int i = 0; i < count; ++i) {
        pieces.push_back(game.activePiece());
        game.hardDrop();
        if (game.isGameOver()) {
            break;
        }
    }
    return pieces;
}

void testPieceShapesAndBag() {
    constexpr std::array<PieceType, 7> types{
        PieceType::I, PieceType::O, PieceType::T, PieceType::S,
        PieceType::Z, PieceType::J, PieceType::L,
    };
    for (const auto type : types) {
        for (int rotation = 0; rotation < 4; ++rotation) {
            check(Game::pieceCells(type, rotation).size() == 4, "each tetromino rotation has four cells");
        }
    }

    Game first(12345);
    Game second(12345);
    check(first.activePiece() == second.activePiece(), "same seed produces same starting piece");
    check(first.nextPieces() == second.nextPieces(), "same seed produces same preview queue");
    const auto spawnedCells = first.activeCells();
    check(std::any_of(spawnedCells.begin(), spawnedCells.end(), [](Point cell) {
        return cell.y >= Game::HiddenRows;
    }), "newly spawned piece is partly visible");

    const auto pieces = firstPieces(first, 7);
    check(pieces.size() == 7, "seven pieces can be played from the first bag");
    const std::set<PieceType> unique(pieces.begin(), pieces.end());
    check(unique.size() == 7, "each seven-bag contains each tetromino once");
}

void testHoldGhostAndDrops() {
    Game game(9);
    const auto original = game.activePiece();
    const auto upcoming = game.nextPieces().front();
    check(game.hold(), "first hold succeeds");
    check(game.heldPiece() == original, "hold stores active piece");
    check(game.activePiece() == upcoming, "empty hold draws the next piece");
    check(!game.hold(), "hold can be used only once per piece");

    const auto ghost = game.ghostCells();
    check(ghost.size() == 4, "ghost exposes four cells");
    check(std::all_of(ghost.begin(), ghost.end(), [](Point point) {
        return point.y < Game::TotalRows;
    }), "ghost remains inside the board");

    check(game.softDrop(), "soft drop moves one cell");
    check(game.score() == 1, "soft drop awards one point per cell");
    const auto distance = game.hardDrop();
    check(distance >= 0, "hard drop reports its distance");
    check(game.score() >= 1 + static_cast<std::uint64_t>(distance * 2), "hard drop awards two points per cell");
    check(game.hold(), "hold becomes available after the piece locks");
}

void testRotationsAndWallKick() {
    Game game(2);
    GameTestAccess::setActive(game, PieceType::I, -2, 5, 1);
    check(game.rotateClockwise(), "I piece uses an SRS wall kick at the left wall");
    const auto cells = game.activeCells();
    const auto minimumX = std::min_element(cells.begin(), cells.end(), [](Point left, Point right) {
        return left.x < right.x;
    })->x;
    check(minimumX == 0, "wall kick keeps the rotated I piece inside the board");

    GameTestAccess::setActive(game, PieceType::T, 3, 5, 0);
    const auto before = game.activeCells();
    check(game.rotateClockwise(), "T piece rotates clockwise");
    check(game.activeCells() != before, "T rotation changes occupied cells");
    check(game.rotateCounterClockwise(), "T piece rotates counterclockwise");
    check(game.activeCells() == before, "opposite rotations restore the original cells");

    GameTestAccess::setActive(game, PieceType::I, 2, 17, 2);
    GameTestAccess::setCell(game, 3, 18, PieceType::J);
    GameTestAccess::setCell(game, 5, 20, PieceType::J);
    GameTestAccess::setCell(game, 2, 18, PieceType::J);
    check(game.rotateClockwise(), "I piece uses the fourth 2-to-L SRS kick");
    check(game.activeCells()[0] == Point{5, 19}, "2-to-L fourth kick moves up one row");

    GameTestAccess::setActive(game, PieceType::I, 2, 15, 2);
    GameTestAccess::setCell(game, 3, 16, PieceType::J);
    GameTestAccess::setCell(game, 5, 18, PieceType::J);
    GameTestAccess::setCell(game, 2, 16, PieceType::J);
    GameTestAccess::setCell(game, 5, 16, PieceType::J);
    check(game.rotateClockwise(), "I piece uses the fifth 2-to-L SRS kick");
    check(game.activeCells()[0] == Point{2, 20}, "2-to-L fifth kick moves down two rows");

    GameTestAccess::setActive(game, PieceType::I, 3, 16, 3);
    GameTestAccess::setCell(game, 3, 18, PieceType::J);
    GameTestAccess::setCell(game, 1, 18, PieceType::J);
    GameTestAccess::setCell(game, 5, 18, PieceType::J);
    check(game.rotateCounterClockwise(), "I piece uses the fourth L-to-2 SRS kick");
    check(game.activeCells()[0] == Point{4, 19}, "L-to-2 fourth kick moves down one row");

    GameTestAccess::setActive(game, PieceType::I, 3, 16, 3);
    GameTestAccess::setCell(game, 3, 18, PieceType::J);
    GameTestAccess::setCell(game, 1, 18, PieceType::J);
    GameTestAccess::setCell(game, 5, 18, PieceType::J);
    GameTestAccess::setCell(game, 1, 19, PieceType::J);
    check(game.rotateCounterClockwise(), "I piece uses the fifth L-to-2 SRS kick");
    check(game.activeCells()[0] == Point{7, 16}, "L-to-2 fifth kick moves up two rows");
}

void testGravityLockAndPause() {
    GameSettings settings;
    settings.gravityInitialMs = 100;
    settings.gravityStepMs = 0;
    settings.minGravityMs = 100;
    Game game(4, settings);
    GameTestAccess::setActive(game, PieceType::O, 3, 0);
    auto before = game.activeCells();
    game.advance(99);
    check(game.activeCells() == before, "gravity waits until its configured interval");
    game.advance(1);
    auto after = game.activeCells();
    check(after.front().y == before.front().y + 1, "gravity advances one row at the interval");

    game.pause();
    check(!game.isRunning(), "paused game is not running");
    check(game.isPaused(), "pause state remains queryable");
    const auto pausedCells = game.activeCells();
    const auto pausedTime = game.elapsedMilliseconds();
    game.advance(500);
    check(game.activeCells() == pausedCells, "paused game does not move");
    check(game.elapsedMilliseconds() == pausedTime, "paused time is not counted");
    game.resume();
    check(game.isRunning(), "resume returns the game to running state");

    GameSettings lockSettings;
    lockSettings.gravityInitialMs = 100000;
    lockSettings.lockDelayMs = 500;
    Game lockGame(5, lockSettings);
    GameTestAccess::setActive(lockGame, PieceType::O, 3, 20);
    lockGame.advance(499);
    check(lockGame.activePiece() == PieceType::O, "piece waits through lock delay");
    lockGame.advance(1);
    check(lockGame.board()[20][4] == PieceType::O && lockGame.board()[21][4] == PieceType::O,
          "grounded piece locks after configured delay");
}

void testLockResetCapAndPracticeMode() {
    GameSettings settings;
    settings.gravityInitialMs = 100000;
    settings.lockDelayMs = 500;
    settings.maxLockResets = 1;
    Game game(11, settings);
    GameTestAccess::setActive(game, PieceType::O, 3, 20);
    game.advance(400);
    check(game.moveLeft(), "grounded move succeeds and resets lock timer");
    game.advance(400);
    check(game.moveRight(), "second grounded move succeeds");
    game.advance(100);
    check(game.board()[21][4] == PieceType::O, "lock timer reset count is capped");

    GameSettings practice;
    practice.gravityEnabled = false;
    Game practiceGame(12, practice);
    const auto practiceSpawn = practiceGame.activeCells();
    check(std::any_of(practiceSpawn.begin(), practiceSpawn.end(), [](Point cell) {
        return cell.y >= Game::HiddenRows;
    }), "practice piece is visible without gravity");
    GameTestAccess::setActive(practiceGame, PieceType::O, 3, 20);
    practiceGame.advance(10000);
    check(practiceGame.activePiece() == PieceType::O, "practice mode disables gravity and automatic locking");
    check(practiceGame.board()[21][4] == PieceType::Empty, "practice piece remains active until manually placed");
    practiceGame.hardDrop();
    check(practiceGame.board()[21][4] == PieceType::O, "practice hard drop manually places the piece");
}

void testLineClearScoringAndTopOut() {
    Game game(15);
    GameTestAccess::setActive(game, PieceType::O, 2, 20);
    GameTestAccess::fillRowExcept(game, Game::TotalRows - 1, 3, 4);
    game.hardDrop();
    check(game.lines() == 1, "completed row clears");
    check(game.score() == 100, "single line awards level-one base score");
    check(game.combo() == 0, "first consecutive line clear starts combo");
    check(game.board()[Game::TotalRows - 1][3] == PieceType::O, "remaining rows shift down after clear");

    Game topOut(16);
    GameTestAccess::setActive(topOut, PieceType::O, 3, 0);
    GameTestAccess::setCell(topOut, 4, 2, PieceType::J);
    GameTestAccess::setCell(topOut, 5, 2, PieceType::J);
    topOut.hardDrop();
    check(topOut.isGameOver(), "locking a piece in hidden rows ends the game");
    check(!topOut.isRunning(), "game-over state is not running");
}

}

int main() {
    testPieceShapesAndBag();
    testHoldGhostAndDrops();
    testRotationsAndWallKick();
    testGravityLockAndPause();
    testLockResetCapAndPracticeMode();
    testLineClearScoringAndTopOut();
    std::cout << "All game engine tests passed.\n";
}
