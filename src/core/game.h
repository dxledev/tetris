#pragma once

#include <array>
#include <cstdint>
#include <random>
#include <vector>

namespace tetris {

enum class PieceType : std::uint8_t {
    Empty = 0,
    I,
    O,
    T,
    S,
    Z,
    J,
    L,
};

struct Point {
    int x = 0;
    int y = 0;

    friend bool operator==(const Point&, const Point&) = default;
};

struct GameSettings {
    bool gravityEnabled = true;
    std::uint64_t lockDelayMs = 500;
    int maxLockResets = 15;
    std::uint64_t gravityInitialMs = 1000;
    std::uint64_t gravityStepMs = 75;
    std::uint64_t minGravityMs = 100;
    int linesPerLevel = 10;
};

class Game {
public:
    static constexpr int Width = 10;
    static constexpr int VisibleRows = 20;
    static constexpr int HiddenRows = 2;
    static constexpr int TotalRows = VisibleRows + HiddenRows;
    static constexpr int PreviewCount = 5;
    using Row = std::array<PieceType, Width>;
    using Board = std::array<Row, TotalRows>;
    using Cells = std::array<Point, 4>;

    explicit Game(std::uint32_t seed = std::random_device{}(), GameSettings settings = {});

    void startNewGame(std::uint32_t seed);
    void setSettings(GameSettings settings);
    void pause();
    void resume();
    void advance(std::uint64_t deltaMs);

    bool moveLeft();
    bool moveRight();
    bool rotateClockwise();
    bool rotateCounterClockwise();
    bool softDrop();
    int hardDrop();
    bool hold();

    const Board& board() const;
    Cells activeCells() const;
    Cells ghostCells() const;
    const std::vector<PieceType>& nextPieces() const;
    static Cells pieceCells(PieceType type, int rotation = 0);
    PieceType activePiece() const;
    PieceType activePieceType() const;
    PieceType heldPiece() const;

    std::uint64_t score() const;
    int lines() const;
    int level() const;
    std::uint64_t elapsedMilliseconds() const;
    int combo() const;
    bool backToBack() const;

    bool isRunning() const;
    bool isPaused() const;
    bool isGameOver() const;

private:
    friend struct GameTestAccess;

    struct ActivePiece {
        PieceType type = PieceType::Empty;
        int x = 0;
        int y = 0;
        int rotation = 0;
    };

    GameSettings settings_;
    std::mt19937 random_;
    Board board_{};
    std::vector<PieceType> bag_;
    std::vector<PieceType> queue_;
    ActivePiece active_{};
    PieceType held_ = PieceType::Empty;
    bool holdUsed_ = false;
    bool running_ = false;
    bool paused_ = false;
    bool gameOver_ = false;
    std::uint64_t score_ = 0;
    int lines_ = 0;
    int level_ = 1;
    int combo_ = -1;
    bool backToBack_ = false;
    std::uint64_t elapsedMs_ = 0;
    std::uint64_t gravityElapsedMs_ = 0;
    std::uint64_t lockElapsedMs_ = 0;
    int lockResets_ = 0;

    void resetState(std::uint32_t seed);
    void ensureQueueSize(std::size_t size);
    void refillBag();
    PieceType takeNextPiece();
    void spawnPiece(PieceType type);
    void spawnNextPiece();
    static Cells localCells(PieceType type, int rotation);
    Cells cellsFor(const ActivePiece& piece) const;
    bool canPlace(const ActivePiece& piece) const;
    bool tryMove(int dx, int dy);
    bool tryRotate(int direction);
    bool isGrounded() const;
    void resetLockDelayAfterAction(bool wasGrounded);
    std::uint64_t gravityIntervalMs() const;
    void lockPiece();
    void clearLines();
    void updateScoreForClear(int cleared);
};

}
