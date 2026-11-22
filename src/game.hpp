#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <random>
#include <set>
#include <string>
#include <vector>

namespace bomber {
// [01] Constants and value types. Coordinates refer to cell centers (x + .5).
constexpr int Width = 15;
constexpr int Height = 13;
constexpr float FuseTime = 2.5f;
constexpr float FlameTime = .5f;
constexpr float FixedStep = 1.f / 120.f;
enum class Tile { Floor, Hard, Soft };
enum class Item { None, Fire, Bomb, Speed };
enum class Direction { None, Up, Down, Left, Right };
enum class BombKind { Normal, Pierce, Remote };
enum class EnemyKind { Wanderer, Chaser, Armored, Swift };
enum class Phase { Playing, Paused, StageClear, GameOver, Victory };
enum class Event { Place, Explode, Pickup, Hurt, EnemyDown, Clear, Win };
enum class Scene { Chain, Chase, Items, Exit, Specials, Life };
struct Cell {
    int x = 0, y = 0;
    bool operator==(Cell b) const { return x == b.x && y == b.y; }
    bool operator!=(Cell b) const { return !(*this == b); }
};
Cell offset(Cell p, Direction d);
Direction directionTo(Cell a, Cell b);
Direction reverse(Direction d);
struct Actor {
    float x = 1.5f, y = 1.5f;
    Cell spawn{1, 1};
    Direction direction = Direction::None;
    Direction travel = Direction::None;
    Cell target{1, 1};
    bool alive = true;
    Cell cell() const;
    bool centered() const;
};
struct Player : Actor {
    int id = 0, range = 1, capacity = 1, speedLevel = 0;
    BombKind selected = BombKind::Normal;
    float invincible = 0.f;
    float speed() const { return 3.f + .45f * speedLevel; }
};
struct Enemy : Actor {
    EnemyKind kind = EnemyKind::Wanderer;
    int hp = 1;
    std::set<int> hitBlasts;
    std::vector<Cell> path;
    float speed() const;
};
struct Bomb {
    int id = 0, owner = 0;
    Cell cell;
    int range = 1;
    float fuse = FuseTime;
    BombKind kind = BombKind::Normal;
    unsigned passMask = 0;
};
struct Flame {
    Cell cell;
    float ttl = FlameTime;
    int owner = 0, blastId = 0;
};
struct Pickup { Cell cell; Item item = Item::None; float age = 0.f; };
struct Input {
    std::array<Direction, 2> move{Direction::None, Direction::None};
    std::array<bool, 2> place{false, false};
    std::array<bool, 2> remote{false, false};
};

// [02] Pure C++ simulation; no window, OpenGL, sound, or clock dependency.
class Game {
public:
    std::array<Tile, Width * Height> tiles{};
    std::array<Item, Width * Height> hidden{};
    std::vector<Player> players;
    std::vector<Enemy> enemies;
    std::vector<Bomb> bombs;
    std::vector<Flame> flames;
    std::vector<Pickup> items;
    Cell exit{13, 11};
    bool exitRevealed = false, practice = false;
    int stage = 1, lives = 3, score = 0, nextBombId = 1;
    float elapsed = 0.f, stageElapsed = 0.f;
    Phase phase = Phase::Playing;
    std::string notice;
    std::vector<Event> events;

    explicit Game(unsigned seed = 2026);
    void newGame(int playerCount = 1);
    void loadStage(int number, bool keepAbilities = true);
    void loadScene(Scene scene, int playerCount = 1);
    void nextStage();
    void update(float dt, const Input& input = {});
    void togglePause();
    bool placeBomb(int playerId);
    void detonateRemote(int playerId);
    void cycleBomb(int playerId);
    void explodeBombs(const std::vector<int>& ids);
    std::vector<Cell> shortestPath(Cell from, Cell to, int maxDistance = 8) const;
    std::array<int, Width * Height> distanceMap(Cell from, int maxDistance = 8) const;
    bool passable(Cell cell, int playerId = -1) const;
    bool exitOpen() const;
    int aliveEnemies() const;
    Tile tile(Cell cell) const;
    void setTile(Cell cell, Tile value);
    static bool inside(Cell cell);
    static int index(Cell cell);
    bool touches(const Actor& actor, Cell cell, float radius = .26f) const;
    void resolveContacts();
    void loseLife();
    void resetPositions();
    std::vector<Event> takeEvents();

private:
    std::mt19937 random;
    unsigned baseSeed;
    int blastSequence = 0;
    void moveActor(Actor& actor, float distance, int playerId,
                   const std::function<Direction()>& choose);
    Direction chooseEnemyDirection(Enemy& enemy);
    void buildArena(unsigned seed);
    void reveal(Cell cell);
    void addFlame(Cell cell, int owner, int blastId);
};
const char* bombName(BombKind kind);
const char* sceneName(Scene scene);
} // namespace bomber
