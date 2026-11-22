#include "game.hpp"

#include <algorithm>
#include <cmath>
#include <queue>

namespace bomber {
// [01] Grid geometry and actor properties.
static constexpr Direction Directions[] = {
    Direction::Up, Direction::Down, Direction::Left, Direction::Right};
Cell offset(Cell p, Direction d) {
    if (d == Direction::Up) --p.y;
    if (d == Direction::Down) ++p.y;
    if (d == Direction::Left) --p.x;
    if (d == Direction::Right) ++p.x;
    return p;
}
Direction directionTo(Cell a, Cell b) {
    for (auto d : Directions) if (offset(a, d) == b) return d;
    return Direction::None;
}
Direction reverse(Direction d) {
    if (d == Direction::Up) return Direction::Down;
    if (d == Direction::Down) return Direction::Up;
    if (d == Direction::Left) return Direction::Right;
    if (d == Direction::Right) return Direction::Left;
    return Direction::None;
}
Cell Actor::cell() const { return {int(std::floor(x)), int(std::floor(y))}; }
bool Actor::centered() const {
    return std::abs(x - (std::floor(x) + .5f)) < .001f &&
           std::abs(y - (std::floor(y) + .5f)) < .001f;
}
float Enemy::speed() const {
    if (kind == EnemyKind::Swift) return 2.7f;
    if (kind == EnemyKind::Chaser) return 2.15f;
    if (kind == EnemyKind::Armored) return 1.4f;
    return 1.65f;
}
const char* bombName(BombKind k) {
    if (k == BombKind::Pierce) return "PIERCE";
    if (k == BombKind::Remote) return "REMOTE";
    return "NORMAL";
}
const char* sceneName(Scene s) {
    switch (s) {
    case Scene::Chain: return "01 / CHAIN REACTION";
    case Scene::Chase: return "02 / BFS PURSUIT";
    case Scene::Items: return "03 / HIDDEN UPGRADES";
    case Scene::Exit: return "04 / LOCKED EXIT";
    case Scene::Specials: return "05 / SPECIAL ARSENAL";
    case Scene::Life: return "06 / LIFE & RESPAWN";
    }
    return "";
}
bool Game::inside(Cell c) { return c.x >= 0 && c.x < Width && c.y >= 0 && c.y < Height; }
int Game::index(Cell c) { return c.y * Width + c.x; }
Tile Game::tile(Cell c) const { return inside(c) ? tiles[index(c)] : Tile::Hard; }
void Game::setTile(Cell c, Tile t) { if (inside(c)) tiles[index(c)] = t; }
Game::Game(unsigned seed) : random(seed), baseSeed(seed) { newGame(); }

// [02] Deterministic stage construction: lattice, connected lanes, safe spawns.
void Game::buildArena(unsigned seed) {
    random.seed(seed);
    hidden.fill(Item::None);
    for (int y = 0; y < Height; ++y) for (int x = 0; x < Width; ++x) {
        Cell c{x, y};
        bool hard = x == 0 || y == 0 || x == Width - 1 || y == Height - 1 ||
                    (x % 2 == 0 && y % 2 == 0);
        setTile(c, hard ? Tile::Hard : Tile::Floor);
        // Odd rows and columns form a connected graph once soft blocks are cleared.
        if (!hard && random() % 100 < unsigned(43 + stage * 3)) setTile(c, Tile::Soft);
    }
    for (Cell spawn : {Cell{1, 1}, Cell{13, 11}, Cell{13, 1}, Cell{1, 11}, Cell{7, 7}}) {
        setTile(spawn, Tile::Floor);
        for (auto d : Directions) if (tile(offset(spawn, d)) != Tile::Hard)
            setTile(offset(spawn, d), Tile::Floor);
    }
    // Guarantee each advanced item rather than relying on random drops.
    const Cell guaranteed[] = {{3, 1}, {1, 3}, {3, 3}, {11, 9}, {9, 11}, {11, 11}};
    for (int i = 0; i < 6; ++i) {
        setTile(guaranteed[i], Tile::Soft);
        hidden[index(guaranteed[i])] = Item(1 + i % 3);
    }
    for (int y = 1; y < Height - 1; ++y) for (int x = 1; x < Width - 1; ++x) {
        Cell c{x, y};
        if (tile(c) == Tile::Soft && hidden[index(c)] == Item::None && random() % 100 < 22)
            hidden[index(c)] = Item(1 + random() % 3);
    }
    exit = {13, 9};
    setTile(exit, Tile::Soft);
    hidden[index(exit)] = Item::None;
}
void Game::newGame(int count) {
    practice = false;
    score = 0; lives = 3; elapsed = 0; nextBombId = 1; blastSequence = 0;
    players.assign(std::clamp(count, 1, 2), Player{});
    for (int i = 0; i < int(players.size()); ++i) {
        players[i].id = i;
        players[i].spawn = i == 0 ? Cell{1, 1} : Cell{13, 11};
    }
    events.clear();
    loadStage(1, false);
}
void Game::loadStage(int number, bool keepAbilities) {
    stage = std::clamp(number, 1, 3);
    stageElapsed = 0; exitRevealed = false; phase = Phase::Playing;
    bombs.clear(); flames.clear(); items.clear(); enemies.clear();
    buildArena(baseSeed + unsigned(stage) * 7919);
    if (!keepAbilities) for (auto& p : players) {
        p.range = 1; p.capacity = 1; p.speedLevel = 0; p.selected = BombKind::Normal;
        p.fireItemsCollected = p.bombItemsCollected = p.speedItemsCollected = 0;
    }
    auto add = [&](Cell c, EnemyKind kind) {
        Enemy e; e.spawn = c; e.kind = kind; e.hp = kind == EnemyKind::Armored ? 2 : 1;
        enemies.push_back(e);
        setTile(c, Tile::Floor);
    };
    add({13, 1}, EnemyKind::Wanderer);
    add({1, 11}, EnemyKind::Chaser);
    add({7, 7}, EnemyKind::Wanderer);
    if (stage >= 2) add({11, 5}, EnemyKind::Armored);
    if (stage >= 3) { add({5, 9}, EnemyKind::Swift); add({9, 3}, EnemyKind::Chaser); }
    resetPositions();
    // Initial play has no immunity: the player can demonstrate self-inflicted damage.
    for (auto& p : players) p.invincible = 0;
    notice = "Clear enemies. Reveal the exit. Get out.";
}
void Game::nextStage() {
    if (phase != Phase::StageClear) return;
    if (stage < 3 && !practice) loadStage(stage + 1);
}

// [03] Explicit presentation fixtures; excluded from persistent rankings.
void Game::loadScene(Scene scene, int count) {
    newGame(count); practice = true; enemies.clear(); bombs.clear(); flames.clear(); items.clear();
    for (auto& t : tiles) if (t == Tile::Soft) t = Tile::Floor;
    hidden.fill(Item::None); exit = {13, 9}; setTile(exit, Tile::Soft);
    exitRevealed = false; notice = sceneName(scene);
    auto position = [](Actor& a, Cell c) {
        a.spawn = c; a.x = c.x + .5f; a.y = c.y + .5f; a.target = c;
        a.travel = a.direction = Direction::None;
    };
    // Labs reuse the bottom-right enemy area; keep co-op's second spawn separate.
    if (players.size() > 1) position(players[1], {1, 11});
    auto enemy = [&](Cell c, EnemyKind kind) {
        Enemy e; position(e, c); e.kind = kind;
        e.hp = kind == EnemyKind::Armored ? 2 : 1; enemies.push_back(e);
    };
    auto bomb = [&](Cell c, int range, float fuse, BombKind kind = BombKind::Normal) {
        bombs.push_back({nextBombId++, 0, c, range, fuse, kind, 0});
    };
    if (scene == Scene::Chain) {
        position(players[0], {1, 1});
        players[0].capacity = 4; players[0].range = 4;
        bomb({3, 3}, 4, 2.5f); bomb({7, 3}, 4, 30.f);
        bomb({7, 7}, 4, 30.f); bomb({11, 7}, 3, 30.f);
        setTile({12, 7}, Tile::Soft); setTile({11, 9}, Tile::Soft);
        enemy({13, 11}, EnemyKind::Wanderer);
        notice = "Watch A > B > C > D: one queue, four explosions. F3: next lab";
    } else if (scene == Scene::Chase) {
        position(players[0], {7, 3}); enemy({1, 3}, EnemyKind::Chaser);
        enemy({13, 11}, EnemyKind::Wanderer);
        notice = "Purple follows BFS within 8 cells. F2: path. Move away to break pursuit.";
    } else if (scene == Scene::Items) {
        position(players[0], {1, 1});
        setTile({3, 1}, Tile::Soft); hidden[index({3, 1})] = Item::Fire;
        setTile({1, 3}, Tile::Soft); hidden[index({1, 3})] = Item::Bomb;
        setTile({3, 3}, Tile::Soft); hidden[index({3, 3})] = Item::Speed;
        enemy({13, 11}, EnemyKind::Wanderer);
        notice = "Break crates at (3,1), (1,3), (3,3): Fire / Bomb / Speed.";
    } else if (scene == Scene::Exit) {
        position(players[0], {11, 9}); enemy({13, 9}, EnemyKind::Armored);
        // The last enemy lives near the concealed exit; two hits prove the lock condition.
        setTile(exit, Tile::Soft); position(enemies[0], {13, 11});
        players[0].capacity = 3; players[0].range = 3;
        notice = "Reveal the exit at (13,9). It stays locked until the last enemy dies.";
    } else if (scene == Scene::Specials) {
        position(players[0], {3, 5}); players[0].capacity = 5; players[0].range = 5;
        players[0].selected = BombKind::Pierce;
        setTile({5, 5}, Tile::Soft); setTile({7, 5}, Tile::Soft); setTile({9, 5}, Tile::Soft);
        enemy({11, 5}, EnemyKind::Armored); enemy({13, 11}, EnemyKind::Swift);
        notice = "Q: NORMAL / PIERCE / REMOTE. E: remote trigger. Armor takes two blasts.";
    } else {
        position(players[0], {1, 1}); enemy({5, 1}, EnemyKind::Chaser);
        notice = "Let the chaser touch you: shared lives -1, survivors return to spawn.";
    }
}

// [04] Static/dynamic occupancy and shortest paths (BFS with predecessor array).
bool Game::passable(Cell c, int playerId) const {
    if (tile(c) != Tile::Floor) return false;
    for (const auto& b : bombs) if (b.cell == c) {
        if (playerId < 0 || !(b.passMask & (1u << playerId))) return false;
    }
    return true;
}
std::vector<Cell> Game::shortestPath(Cell from, Cell to, int maxDistance) const {
    if (!inside(from) || !inside(to)) return {};
    std::array<int, Width * Height> dist, previous;
    dist.fill(-1); previous.fill(-1);
    std::queue<Cell> queue;
    queue.push(from); dist[index(from)] = 0;
    while (!queue.empty()) {
        Cell c = queue.front(); queue.pop();
        if (c == to) {
            std::vector<Cell> path;
            for (int at = index(to); at != -1; at = previous[at])
                path.push_back({at % Width, at / Width});
            std::reverse(path.begin(), path.end());
            return path;
        }
        if (dist[index(c)] >= maxDistance) continue;
        for (auto d : Directions) {
            Cell n = offset(c, d);
            if (!passable(n) || dist[index(n)] != -1) continue;
            dist[index(n)] = dist[index(c)] + 1; previous[index(n)] = index(c);
            queue.push(n);
        }
    }
    return {};
}
Direction Game::chooseEnemyDirection(Enemy& e) {
    e.path.clear();
    if (e.kind == EnemyKind::Chaser) {
        for (const auto& p : players) if (p.alive) {
            auto path = shortestPath(e.cell(), p.cell(), 8);
            if (!path.empty() && (e.path.empty() || path.size() < e.path.size())) e.path = path;
        }
        if (e.path.size() > 1) return directionTo(e.path[0], e.path[1]);
    }
    // The reference enemy keeps its direction until blocked; chasers use BFS first.
    if (e.direction != Direction::None && passable(offset(e.cell(), e.direction)))
        return e.direction;
    std::vector<Direction> choices;
    for (auto d : Directions) if (passable(offset(e.cell(), d))) choices.push_back(d);
    if (choices.empty()) return Direction::None;
    if (choices.size() > 1)
        choices.erase(std::remove(choices.begin(), choices.end(), reverse(e.direction)), choices.end());
    return choices[random() % choices.size()];
}

// [05] Continuous constant-speed travel between centers; no diagonal shortcuts.
void Game::moveActor(Actor& a, float distance, int playerId,
                     const std::function<Direction()>& choose) {
    for (int safety = 0; distance > .00001f && safety < 16; ++safety) {
        if (a.travel != Direction::None && !passable(a.target, playerId)) {
            // A newly placed bomb can invalidate a segment selected on an earlier tick.
            a.target = offset(a.target, reverse(a.travel));
            a.direction = a.travel = reverse(a.travel);
        }
        if (a.travel == Direction::None) {
            Direction d = choose();
            if (d == Direction::None) break;
            Cell next = offset(a.cell(), d);
            if (!passable(next, playerId)) break;
            a.target = next; a.direction = a.travel = d;
        }
        const float tx = a.target.x + .5f, ty = a.target.y + .5f;
        const float remain = std::abs(tx - a.x) + std::abs(ty - a.y);
        const float step = std::min(remain, distance);
        if (a.travel == Direction::Right) a.x += step;
        if (a.travel == Direction::Left) a.x -= step;
        if (a.travel == Direction::Down) a.y += step;
        if (a.travel == Direction::Up) a.y -= step;
        distance -= step;
        if (step >= remain - .00001f) { a.x = tx; a.y = ty; a.travel = Direction::None; }
        // Once the bomber no longer intersects its bomb, re-entry is forbidden.
        if (playerId >= 0) for (auto& b : bombs)
            if (!touches(a, b.cell)) b.passMask &= ~(1u << playerId);
    }
}

// [06] Bomb ownership/capacity, overlap permission, and special bomb controls.
bool Game::placeBomb(int id) {
    if (phase != Phase::Playing || id < 0 || id >= int(players.size())) return false;
    auto& p = players[id];
    if (!p.alive || tile(p.cell()) != Tile::Floor) return false;
    int active = 0;
    for (const auto& b : bombs) {
        if (b.cell == p.cell()) return false;
        if (b.owner == id) ++active;
    }
    if (active >= p.capacity) return false;
    Bomb b{nextBombId++, id, p.cell(), p.range,
           p.selected == BombKind::Remote ? -1.f : FuseTime, p.selected, 0};
    for (const auto& other : players) if (touches(other, b.cell)) b.passMask |= 1u << other.id;
    bombs.push_back(b); events.push_back(Event::Place);
    return true;
}
void Game::detonateRemote(int id) {
    if (phase != Phase::Playing) return;
    std::vector<int> ids;
    for (const auto& b : bombs) if (b.owner == id && b.kind == BombKind::Remote) ids.push_back(b.id);
    explodeBombs(ids);
}
void Game::cycleBomb(int id) {
    if (id >= 0 && id < int(players.size()))
        players[id].selected = BombKind((int(players[id].selected) + 1) % 3);
}
void Game::reveal(Cell c) {
    setTile(c, Tile::Floor); score += 10;
    if (c == exit) exitRevealed = true;
    Item item = hidden[index(c)]; hidden[index(c)] = Item::None;
    if (item != Item::None) items.push_back({c, item});
}
void Game::addFlame(Cell c, int owner, int blastId) {
    items.erase(std::remove_if(items.begin(), items.end(), [c](const Pickup& i) {
        return i.cell == c;
    }), items.end());
    flames.push_back({c, FlameTime, owner, blastId});
}

// [07] Iterative breadth-first chain reaction; walls stop rays, bombs enqueue once.
void Game::explodeBombs(const std::vector<int>& ids) {
    std::queue<int> queue;
    std::set<int> queued;
    auto enqueue = [&](int id) { if (queued.insert(id).second) queue.push(id); };
    for (int id : ids) enqueue(id);
    // Snapshot soft tiles for the whole simultaneous wave: another ray cannot tunnel
    // through a block removed earlier in the same chain. Pierce explicitly can.
    const auto before = tiles;
    while (!queue.empty()) {
        int id = queue.front(); queue.pop();
        auto it = std::find_if(bombs.begin(), bombs.end(), [id](const Bomb& b) { return b.id == id; });
        if (it == bombs.end()) continue;
        Bomb b = *it; bombs.erase(it); int blastId = ++blastSequence;
        addFlame(b.cell, b.owner, blastId); events.push_back(Event::Explode);
        for (auto d : Directions) {
          Cell c = b.cell;
          for (int k = 1; k <= b.range; ++k) {
            c = offset(c, d);
            if (!inside(c) || before[index(c)] == Tile::Hard) break;
            if (before[index(c)] == Tile::Soft) {
                // The newly exposed item survives its revealing blast. Later flames destroy it.
                if (tile(c) == Tile::Soft) reveal(c);
                if (b.kind != BombKind::Pierce) break;
                continue;
            }
            addFlame(c, b.owner, blastId);
            auto other = std::find_if(bombs.begin(), bombs.end(), [c](const Bomb& n) { return n.cell == c; });
            if (other != bombs.end()) { enqueue(other->id); break; }
          }
        }
    }
}

// [08] Contacts: circle/AABB, unique blast damage, items, shared lives, open exit.
bool Game::touches(const Actor& a, Cell c, float radius) const {
    float dx = a.x - std::clamp(a.x, float(c.x), float(c.x + 1));
    float dy = a.y - std::clamp(a.y, float(c.y), float(c.y + 1));
    return dx * dx + dy * dy < radius * radius;
}
int Game::aliveEnemies() const {
    return int(std::count_if(enemies.begin(), enemies.end(), [](const Enemy& e) { return e.alive; }));
}
bool Game::exitOpen() const { return exitRevealed && aliveEnemies() == 0; }
void Game::resetPositions() {
    auto reset = [](Actor& a) {
        a.x = a.spawn.x + .5f; a.y = a.spawn.y + .5f; a.target = a.spawn;
        a.direction = a.travel = Direction::None;
    };
    for (auto& p : players) { reset(p); p.invincible = 1.6f; }
    for (auto& e : enemies) if (e.alive) { reset(e); e.path.clear(); }
}
void Game::loseLife() {
    if (phase != Phase::Playing) return;
    --lives; events.push_back(Event::Hurt);
    bombs.clear(); flames.clear(); resetPositions();
    if (lives <= 0) { phase = Phase::GameOver; notice = "No lives left. Space: new run"; }
    else notice = "Life lost. Survivors reset. Brief spawn protection.";
}
void Game::resolveContacts() {
    if (phase != Phase::Playing) return;
    for (auto& e : enemies) if (e.alive) for (const auto& f : flames) {
        if (touches(e, f.cell) && e.hitBlasts.insert(f.blastId).second) {
            if (--e.hp <= 0) {
                e.alive = false; score += e.kind == EnemyKind::Armored ? 400 : 200;
                events.push_back(Event::EnemyDown); break;
            }
        }
    }
    for (auto& p : players) if (p.alive && p.invincible <= 0.f) {
        bool hit = false;
        for (const auto& f : flames) if (touches(p, f.cell)) hit = true;
        for (const auto& e : enemies) if (e.alive) {
            float dx = p.x - e.x, dy = p.y - e.y;
            if (dx * dx + dy * dy < .48f * .48f) hit = true;
        }
        if (hit) { loseLife(); return; } // A simultaneous team collision costs one life.
    }
    for (auto& p : players) if (p.alive) {
        for (auto it = items.begin(); it != items.end();) {
            bool burning = std::any_of(flames.begin(), flames.end(), [&](const Flame& f) {
                return f.cell == it->cell;
            });
            if (burning) { it = items.erase(it); continue; }
            if (!touches(p, it->cell, .12f)) { ++it; continue; }
            if (it->item == Item::Fire) { p.range = std::max(p.range, 2); ++p.fireItemsCollected; }
            if (it->item == Item::Bomb) { ++p.capacity; ++p.bombItemsCollected; }
            if (it->item == Item::Speed) { ++p.speedLevel; ++p.speedItemsCollected; }
            score += 50; events.push_back(Event::Pickup); it = items.erase(it);
        }
        if (exitOpen() && p.cell() == exit) {
            score += 1000 + std::max(0, 600 - int(stageElapsed) * 2);
            phase = !practice && stage == 3 ? Phase::Victory : Phase::StageClear;
            notice = "Stage clear! Enter: continue";
            events.push_back(phase == Phase::Victory ? Event::Win : Event::Clear);
            return;
        }
    }
}

// [09] Fixed-step update order; pausing freezes all simulation clocks.
void Game::togglePause() {
    if (phase == Phase::Playing) phase = Phase::Paused;
    else if (phase == Phase::Paused) phase = Phase::Playing;
}
void Game::update(float dt, const Input& input) {
    if (phase != Phase::Playing || dt <= 0) return;
    dt = std::min(dt, .05f); elapsed += dt; stageElapsed += dt;
    for (auto& p : players) p.invincible = std::max(0.f, p.invincible - dt);
    for (auto& item : items) item.age += dt;
    for (auto& f : flames) f.ttl -= dt;
    flames.erase(std::remove_if(flames.begin(), flames.end(), [](const Flame& f) { return f.ttl <= 0; }), flames.end());
    for (int i = 0; i < int(players.size()); ++i) {
        if (input.place[i]) placeBomb(i);
        if (input.remote[i]) detonateRemote(i);
        auto& p = players[i];
        moveActor(p, p.speed() * dt, i, [&] { return input.move[i]; });
    }
    for (auto& e : enemies) if (e.alive)
        moveActor(e, e.speed() * dt, -1, [&] { return chooseEnemyDirection(e); });
    std::vector<int> expired;
    for (auto& b : bombs) {
        if (b.kind != BombKind::Remote) { b.fuse -= dt; if (b.fuse <= 0) expired.push_back(b.id); }
        if (std::any_of(flames.begin(), flames.end(), [&](const Flame& f) { return f.cell == b.cell; }))
            expired.push_back(b.id);
    }
    explodeBombs(expired); resolveContacts();
}
std::vector<Event> Game::takeEvents() {
    auto result = std::move(events); events.clear(); return result;
}

// [10] Complete eight-cell BFS area for the imported F2 distance overlay.
std::array<int, Width * Height> Game::distanceMap(Cell from, int maxDistance) const {
    std::array<int, Width * Height> distances;
    distances.fill(-1);
    if (!inside(from) || tile(from) != Tile::Floor || maxDistance < 0) return distances;
    std::queue<Cell> queue;
    queue.push(from); distances[index(from)] = 0;
    while (!queue.empty()) {
        Cell cell = queue.front(); queue.pop();
        if (distances[index(cell)] >= maxDistance) continue;
        for (auto direction : Directions) {
            Cell next = offset(cell, direction);
            if (!passable(next) || distances[index(next)] >= 0) continue;
            distances[index(next)] = distances[index(cell)] + 1;
            queue.push(next);
        }
    }
    return distances;
}
} // namespace bomber
