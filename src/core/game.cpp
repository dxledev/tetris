#include "game.h"

#include <algorithm>
#include <array>
#include <utility>

namespace tetris {
namespace {

using KickTests = std::array<Point, 5>;

constexpr std::array<PieceType, 7> PieceTypes{
    PieceType::I, PieceType::O, PieceType::T, PieceType::S,
    PieceType::Z, PieceType::J, PieceType::L,
};

constexpr std::array<Point, 5> NoKick{
    Point{0, 0}, Point{0, 0}, Point{0, 0}, Point{0, 0}, Point{0, 0},
};

const KickTests& jlstzKicks(int from, int to) {
    static const std::array<std::array<KickTests, 4>, 4> kicks{{
        {{
            NoKick,
            {{{0, 0}, {-1, 0}, {-1, 1}, {0, -2}, {-1, -2}}},
            NoKick,
            {{{0, 0}, {1, 0}, {1, 1}, {0, -2}, {1, -2}}},
        }},
        {{
            {{{0, 0}, {1, 0}, {1, -1}, {0, 2}, {1, 2}}},
            NoKick,
            {{{0, 0}, {1, 0}, {1, -1}, {0, 2}, {1, 2}}},
            NoKick,
        }},
        {{
            NoKick,
            {{{0, 0}, {-1, 0}, {-1, 1}, {0, -2}, {-1, -2}}},
            NoKick,
            {{{0, 0}, {1, 0}, {1, 1}, {0, -2}, {1, -2}}},
        }},
        {{
            {{{0, 0}, {-1, 0}, {-1, -1}, {0, 2}, {-1, 2}}},
            NoKick,
            {{{0, 0}, {-1, 0}, {-1, -1}, {0, 2}, {-1, 2}}},
            NoKick,
        }},
    }};
    return kicks[from][to];
}

const KickTests& iKicks(int from, int to) {
    static const std::array<std::array<KickTests, 4>, 4> kicks{{
        {{
            NoKick,
            {{{0, 0}, {-2, 0}, {1, 0}, {-2, -1}, {1, 2}}},
            NoKick,
            {{{0, 0}, {-1, 0}, {2, 0}, {-1, 2}, {2, -1}}},
        }},
        {{
            {{{0, 0}, {2, 0}, {-1, 0}, {2, 1}, {-1, -2}}},
            NoKick,
            {{{0, 0}, {-1, 0}, {2, 0}, {-1, -2}, {2, 1}}},
            NoKick,
        }},
        {{
            NoKick,
            {{{0, 0}, {1, 0}, {-2, 0}, {1, 2}, {-2, -1}}},
            NoKick,
            {{{0, 0}, {2, 0}, {-1, 0}, {2, -1}, {-1, 2}}},
        }},
        {{
            {{{0, 0}, {1, 0}, {-2, 0}, {1, -2}, {-2, 1}}},
            NoKick,
            {{{0, 0}, {-2, 0}, {1, 0}, {-2, 1}, {1, -2}}},
            NoKick,
        }},
    }};
    return kicks[from][to];
}

Game::Cells baseCells(PieceType type) {
    switch (type) {
    case PieceType::I:
        return {{{0, 1}, {1, 1}, {2, 1}, {3, 1}}};
    case PieceType::O:
        return {{{1, 0}, {2, 0}, {1, 1}, {2, 1}}};
    case PieceType::T:
        return {{{1, 0}, {0, 1}, {1, 1}, {2, 1}}};
    case PieceType::S:
        return {{{1, 0}, {2, 0}, {0, 1}, {1, 1}}};
    case PieceType::Z:
        return {{{0, 0}, {1, 0}, {1, 1}, {2, 1}}};
    case PieceType::J:
        return {{{0, 0}, {0, 1}, {1, 1}, {2, 1}}};
    case PieceType::L:
        return {{{2, 0}, {0, 1}, {1, 1}, {2, 1}}};
    case PieceType::Empty:
        return {};
    }
    return {};
}

} // namespace

Game::Game(std::uint32_t seed, GameSettings settings)
    : settings_(settings), random_(seed) {
    settings_.gravityInitialMs = std::max<std::uint64_t>(1, settings_.gravityInitialMs);
    settings_.minGravityMs = std::max<std::uint64_t>(1, settings_.minGravityMs);
    settings_.linesPerLevel = std::max(1, settings_.linesPerLevel);
    settings_.maxLockResets = std::max(0, settings_.maxLockResets);
    resetState(seed);
}

void Game::startNewGame(std::uint32_t seed) {
    resetState(seed);
}

void Game::setSettings(GameSettings settings) {
    settings.gravityInitialMs = std::max<std::uint64_t>(1, settings.gravityInitialMs);
    settings.minGravityMs = std::max<std::uint64_t>(1, settings.minGravityMs);
    settings.linesPerLevel = std::max(1, settings.linesPerLevel);
    settings.maxLockResets = std::max(0, settings.maxLockResets);
    settings_ = settings;
    gravityElapsedMs_ = 0;
    lockElapsedMs_ = 0;
}

void Game::resetState(std::uint32_t seed) {
    random_.seed(seed);
    for (auto& row : board_) {
        row.fill(PieceType::Empty);
    }
    bag_.clear();
    queue_.clear();
    held_ = PieceType::Empty;
    holdUsed_ = false;
    running_ = true;
    paused_ = false;
    gameOver_ = false;
    score_ = 0;
    lines_ = 0;
    level_ = 1;
    combo_ = -1;
    backToBack_ = false;
    elapsedMs_ = 0;
    gravityElapsedMs_ = 0;
    lockElapsedMs_ = 0;
    lockResets_ = 0;
    ensureQueueSize(PreviewCount);
    spawnNextPiece();
}

void Game::pause() {
    if (running_ && !gameOver_) {
        paused_ = true;
    }
}

void Game::resume() {
    if (running_ && !gameOver_) {
        paused_ = false;
    }
}

void Game::advance(std::uint64_t deltaMs) {
    if (!running_ || paused_ || gameOver_ || deltaMs == 0) {
        return;
    }

    elapsedMs_ += deltaMs;
    if (!settings_.gravityEnabled) {
        return;
    }
    auto remainingMs = deltaMs;
    while (remainingMs > 0 && running_ && !paused_ && !gameOver_) {
        if (isGrounded()) {
            const auto untilLockMs = settings_.lockDelayMs > lockElapsedMs_
                ? settings_.lockDelayMs - lockElapsedMs_
                : 0;
            if (untilLockMs == 0) {
                lockPiece();
                continue;
            }
            const auto stepMs = std::min(remainingMs, untilLockMs);
            lockElapsedMs_ += stepMs;
            remainingMs -= stepMs;
            if (lockElapsedMs_ >= settings_.lockDelayMs) {
                lockPiece();
            }
            continue;
        }

        const auto intervalMs = gravityIntervalMs();
        const auto untilGravityMs = intervalMs > gravityElapsedMs_
            ? intervalMs - gravityElapsedMs_
            : 0;
        if (untilGravityMs == 0) {
            gravityElapsedMs_ = 0;
            if (!tryMove(0, 1)) {
                lockElapsedMs_ = 0;
            }
            continue;
        }
        const auto stepMs = std::min(remainingMs, untilGravityMs);
        gravityElapsedMs_ += stepMs;
        remainingMs -= stepMs;
        if (gravityElapsedMs_ >= intervalMs) {
            gravityElapsedMs_ = 0;
            if (!tryMove(0, 1)) {
                lockElapsedMs_ = 0;
            }
        }
    }
}

bool Game::moveLeft() {
    return tryMove(-1, 0);
}

bool Game::moveRight() {
    return tryMove(1, 0);
}

bool Game::rotateClockwise() {
    return tryRotate(1);
}

bool Game::rotateCounterClockwise() {
    return tryRotate(-1);
}

bool Game::softDrop() {
    if (!running_ || paused_ || gameOver_) {
        return false;
    }
    if (!tryMove(0, 1)) {
        return false;
    }
    ++score_;
    gravityElapsedMs_ = 0;
    return true;
}

int Game::hardDrop() {
    if (!running_ || paused_ || gameOver_) {
        return 0;
    }
    int distance = 0;
    while (tryMove(0, 1)) {
        ++distance;
    }
    score_ += static_cast<std::uint64_t>(distance) * 2;
    lockPiece();
    return distance;
}

bool Game::hold() {
    if (!running_ || paused_ || gameOver_ || holdUsed_) {
        return false;
    }
    const auto outgoing = active_.type;
    if (held_ == PieceType::Empty) {
        held_ = outgoing;
        spawnNextPiece();
    } else {
        const auto replacement = held_;
        held_ = outgoing;
        spawnPiece(replacement);
    }
    holdUsed_ = true;
    return true;
}

const Game::Board& Game::board() const {
    return board_;
}

Game::Cells Game::activeCells() const {
    if (!running_ || gameOver_) {
        return {};
    }
    return cellsFor(active_);
}

Game::Cells Game::ghostCells() const {
    if (!running_ || gameOver_) {
        return {};
    }
    auto ghost = active_;
    while (canPlace(ActivePiece{ghost.type, ghost.x, ghost.y + 1, ghost.rotation})) {
        ++ghost.y;
    }
    return cellsFor(ghost);
}

const std::vector<PieceType>& Game::nextPieces() const {
    return queue_;
}

Game::Cells Game::pieceCells(PieceType type, int rotation) {
    return localCells(type, rotation);
}

PieceType Game::activePiece() const {
    return active_.type;
}

PieceType Game::activePieceType() const {
    return active_.type;
}

PieceType Game::heldPiece() const {
    return held_;
}

std::uint64_t Game::score() const {
    return score_;
}

int Game::lines() const {
    return lines_;
}

int Game::level() const {
    return level_;
}

std::uint64_t Game::elapsedMilliseconds() const {
    return elapsedMs_;
}

int Game::combo() const {
    return combo_;
}

bool Game::backToBack() const {
    return backToBack_;
}

bool Game::isRunning() const {
    return running_ && !paused_ && !gameOver_;
}

bool Game::isPaused() const {
    return paused_;
}

bool Game::isGameOver() const {
    return gameOver_;
}

void Game::ensureQueueSize(std::size_t size) {
    while (queue_.size() < size) {
        if (bag_.empty()) {
            refillBag();
        }
        queue_.push_back(bag_.back());
        bag_.pop_back();
    }
}

void Game::refillBag() {
    bag_.assign(PieceTypes.begin(), PieceTypes.end());
    std::shuffle(bag_.begin(), bag_.end(), random_);
}

PieceType Game::takeNextPiece() {
    ensureQueueSize(1);
    const auto type = queue_.front();
    queue_.erase(queue_.begin());
    ensureQueueSize(PreviewCount);
    return type;
}

void Game::spawnPiece(PieceType type) {
    active_ = ActivePiece{type, 3, HiddenRows - 1, 0};
    holdUsed_ = false;
    gravityElapsedMs_ = 0;
    lockElapsedMs_ = 0;
    lockResets_ = 0;
    if (!canPlace(active_)) {
        running_ = false;
        gameOver_ = true;
    }
}

void Game::spawnNextPiece() {
    spawnPiece(takeNextPiece());
}

Game::Cells Game::localCells(PieceType type, int rotation) {
    auto cells = baseCells(type);
    if (type == PieceType::O) {
        return cells;
    }
    const int edge = type == PieceType::I ? 3 : 2;
    const int turns = (rotation % 4 + 4) % 4;
    for (int turn = 0; turn < turns; ++turn) {
        for (auto& cell : cells) {
            const auto oldX = cell.x;
            cell.x = edge - cell.y;
            cell.y = oldX;
        }
    }
    return cells;
}

Game::Cells Game::cellsFor(const ActivePiece& piece) const {
    auto cells = localCells(piece.type, piece.rotation);
    for (auto& cell : cells) {
        cell.x += piece.x;
        cell.y += piece.y;
    }
    return cells;
}

bool Game::canPlace(const ActivePiece& piece) const {
    for (const auto cell : cellsFor(piece)) {
        if (cell.x < 0 || cell.x >= Width || cell.y < 0 || cell.y >= TotalRows) {
            return false;
        }
        if (board_[cell.y][cell.x] != PieceType::Empty) {
            return false;
        }
    }
    return true;
}

bool Game::tryMove(int dx, int dy) {
    if (!running_ || paused_ || gameOver_) {
        return false;
    }
    const bool wasGrounded = isGrounded();
    auto moved = active_;
    moved.x += dx;
    moved.y += dy;
    if (!canPlace(moved)) {
        return false;
    }
    active_ = moved;
    if (dy > 0) {
        lockElapsedMs_ = 0;
    } else if (dx != 0) {
        resetLockDelayAfterAction(wasGrounded);
    }
    return true;
}

bool Game::tryRotate(int direction) {
    if (!running_ || paused_ || gameOver_) {
        return false;
    }
    const bool wasGrounded = isGrounded();
    const auto from = active_.rotation;
    const auto to = (from + direction + 4) % 4;
    const KickTests* kicks = &NoKick;
    if (active_.type == PieceType::I) {
        kicks = &iKicks(from, to);
    } else if (active_.type != PieceType::O) {
        kicks = &jlstzKicks(from, to);
    }
    for (const auto kick : *kicks) {
        auto rotated = active_;
        rotated.rotation = to;
        rotated.x += kick.x;
        rotated.y += kick.y;
        if (!canPlace(rotated)) {
            continue;
        }
        active_ = rotated;
        resetLockDelayAfterAction(wasGrounded);
        return true;
    }
    return false;
}

bool Game::isGrounded() const {
    auto below = active_;
    ++below.y;
    return !canPlace(below);
}

void Game::resetLockDelayAfterAction(bool wasGrounded) {
    if (!wasGrounded) {
        return;
    }
    if (lockResets_ < settings_.maxLockResets) {
        ++lockResets_;
        lockElapsedMs_ = 0;
    }
}

std::uint64_t Game::gravityIntervalMs() const {
    const auto reduction = static_cast<std::uint64_t>(level_ - 1) * settings_.gravityStepMs;
    if (reduction >= settings_.gravityInitialMs) {
        return settings_.minGravityMs;
    }
    return std::max(settings_.minGravityMs, settings_.gravityInitialMs - reduction);
}

void Game::lockPiece() {
    for (const auto cell : cellsFor(active_)) {
        board_[cell.y][cell.x] = active_.type;
        if (cell.y < HiddenRows) {
            running_ = false;
            gameOver_ = true;
        }
    }
    if (gameOver_) {
        return;
    }
    clearLines();
    spawnNextPiece();
}

void Game::clearLines() {
    int cleared = 0;
    int target = TotalRows - 1;
    for (int source = TotalRows - 1; source >= 0; --source) {
        const bool full = std::all_of(board_[source].begin(), board_[source].end(), [](PieceType cell) {
            return cell != PieceType::Empty;
        });
        if (full) {
            ++cleared;
            continue;
        }
        if (target != source) {
            board_[target] = board_[source];
        }
        --target;
    }
    while (target >= 0) {
        board_[target].fill(PieceType::Empty);
        --target;
    }
    updateScoreForClear(cleared);
}

void Game::updateScoreForClear(int cleared) {
    if (cleared == 0) {
        combo_ = -1;
        return;
    }

    static constexpr std::array<std::uint64_t, 5> BaseScores{0, 100, 300, 500, 800};
    const bool isTetris = cleared == 4;
    const bool isBackToBack = isTetris && backToBack_;
    if (cleared < static_cast<int>(BaseScores.size())) {
        score_ += BaseScores[cleared] * static_cast<std::uint64_t>(level_);
        if (isBackToBack) {
            score_ += BaseScores[cleared] * static_cast<std::uint64_t>(level_) / 2;
        }
    }
    if (combo_ < 0) {
        combo_ = 0;
    } else {
        ++combo_;
        score_ += static_cast<std::uint64_t>(50 * combo_ * level_);
    }
    if (isTetris) {
        backToBack_ = true;
    } else {
        backToBack_ = false;
    }
    lines_ += cleared;
    level_ = lines_ / settings_.linesPerLevel + 1;
}

} // namespace tetris
