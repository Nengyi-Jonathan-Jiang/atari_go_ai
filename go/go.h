#pragma once

#include <memory>
#include <utility>
#include <vector>
#include <set>
#include <memory>
#include <bitset>
#include <ranges>
#include <cstring>

enum Color {
    BLACK, WHITE
};
Color operator~(const Color& c) { return c == BLACK ? WHITE : BLACK; }


struct Pos {
    int row, col;

    Pos() : Pos(0, 0) {}

    Pos(int row, int col) : row(row), col(col) {}

    Pos operator+(std::pair<int, int> a) const {
        return {row + a.first, col + a.second};
    }

    std::vector<Pos> neighbors() {
        return {{row,     col - 1},
                {row,     col + 1},
                {row - 1, col},
                {row + 1, col}};
    }

    std::vector<Pos> corners() {
        return {{row - 1, col - 1},
                {row - 1, col + 1},
                {row + 1, col - 1},
                {row + 1, col + 1}};
    }

    std::vector<Pos> locality() {
        std::vector<Pos> res(8);
        for (int i = 0; i < 8; i++) res[i] = *this + l1[i];
        return res;
    }

    std::vector<Pos> locality2() {
        std::vector<Pos> res(24);
        for (int i = 0; i < 24; i++) res[i] = *this + l2[i];
        return res;
    }

    std::strong_ordering operator<=>(const Pos &other) const {
        return std::pair{row, col} <=> std::pair{other.row, other.col};
    }

private:
    static std::pair<int, int> l1[], l2[];
};

std::pair<int, int> Pos::l1[] = {{-1, -1},
                                 {-1, 0},
                                 {-1, 1},
                                 {0,  -1},
                                 {0,  1},
                                 {1,  -1},
                                 {1,  0},
                                 {1,  1}};
std::pair<int, int> Pos::l2[] = {
        {-2, -2},
        {-2, -1},
        {-2, 0},
        {-2, 1},
        {-2, 2},
        {-1, -2},
        {-1, -1},
        {-1, 0},
        {-1, 1},
        {-1, 2},
        {0,  -2},
        {0,  -1},
        {0,  1},
        {0,  2},
        {1,  -2},
        {1,  -1},
        {1,  0},
        {1,  1},
        {1,  2},
        {2,  -2},
        {2,  -1},
        {2,  0},
        {2,  1},
        {2,  2},
};

struct Positions {
    std::set<Pos> elements;

    Positions() = default;

    template<std::ranges::range v> Positions(v e) : elements(e.begin(), e.end()) {} // NOLINT(google-explicit-constructor)

    [[nodiscard]] bool has(Pos p) const {
        return elements.contains(p);
    }

    void operator+=(Pos p) {
        elements.insert(p);
    }

    bool operator-=(Pos p) {
        bool res = has(p);
        elements.erase(p);
        return res;
    }

    [[nodiscard]] size_t count() const {
        return elements.size();
    }

    [[nodiscard]] Pos getAny() const {
        return *elements.begin();
    }

    [[nodiscard]] auto begin() const {
        return elements.begin();
    }

    [[nodiscard]] auto end() const {
        return elements.end();
    }

    Positions operator+(const Positions &other) const {
        std::set<Pos> merged = elements;
        for (auto i: other) merged.insert(i);
        return {merged};
    }

    void operator+=(const Positions &other) {
        for (auto i: other) elements.insert(i);
    }

    template<std::ranges::range v>
    void operator-=(v stones) {
        for (auto i: stones) *this -= i;
    }
};

struct _Group {
    Color color;
    Positions stones, liberties;

    _Group(Color color, Positions stones, Positions liberties) : color(color), stones(std::move(stones)),
                                                                 liberties(std::move(liberties)) {}

    _Group(Color color, Pos stone, const Positions& liberties) : _Group(color, Positions(std::array{stone}), liberties) {}

    _Group operator+(const _Group &other) const {
        _Group res = *this;
        res += other;
        return res;
    }
    void operator+=(const _Group &other){
        if (color == other.color) {
            stones += other.stones;
            liberties += other.liberties;
            liberties -= stones;
        }
        throw std::invalid_argument("Cannot merge groups of different colors");
    }

    [[nodiscard]] size_t numLiberties() const {
        return liberties.count();
    }

    [[nodiscard]] bool isDead() const {
        return numLiberties() == 0;
    }
};
typedef std::shared_ptr<_Group> Group;

//template<int size>
constexpr int size = 9;
struct Board {
    static bool is_pos_valid(Pos pos) {
        return pos.row >= 0 && pos.row < size && pos.col >= 0 && pos.col < size;
    }

    std::array<std::array<Group, size>, size> grid;
    std::set<Group> activeGroups;

    Board() = default;

    bool place_stone(Color color, Pos pos) {
        // Check simple invalid placement
        if (!is_pos_valid(pos) || !grid[pos.row][pos.col]) return false;

        // Classify adjacent positions
        std::set<Group> adj_friends, adj_enemies;
        Positions newLiberties;
        for (auto p : pos.neighbors()) if (is_pos_valid(p)) {
            auto group = grid[p.row][p.col];
            if (group) {
                (group->color == color ? adj_friends : adj_enemies).insert(group);
            } else newLiberties += p;
        }

        // Merge friends with stone
        auto new_group = std::make_shared<_Group>(color, pos, newLiberties);
        for (const auto& e : adj_friends) *new_group += *e;

        // Check suicide rule
        if (new_group->isDead()) return false;

        // Update self state

        for (const auto& e : adj_friends) activeGroups.erase(e);

        // Put group on grid
        activeGroups.insert(new_group);
        for (auto p : new_group->stones) grid[p.row][p.col] = new_group;

        // Update enemies
        for (const auto& enemy : adj_enemies){
            enemy->liberties -= pos;
            if (enemy->isDead()) removeDeadGroup(enemy);
        }

        return true;
    }

    void removeDeadGroup(Group g){
        activeGroups.erase(g);
        for (auto p: g->stones) getGroupPtr(p).reset();
    }

    void clear() {
        for(auto& i : grid) for(auto& x : i) x.reset();
    }

    [[nodiscard]] Group& getGroupPtr(Pos p){
        return grid[p.row][p.col];
    }
    [[nodiscard]] const Group& getGroupPtr(Pos p) const {
        return grid[p.row][p.col];
    }
    [[nodiscard]] Group& operator[](Pos p){
        return getGroupPtr(p);
    }
    [[nodiscard]] const Group& operator[](Pos p) const {
        return getGroupPtr(p);
    }

    [[nodiscard]] Board copy() const {
        Board res;
        res.activeGroups = activeGroups;
        for(const Group& group : activeGroups){
            Group copy {new _Group{*group}};
            for(Pos p : copy->stones){
                res[p] = copy;
            }
        }
        return res;
    }
};

struct Move {
    const Color color;
    const Pos pos;
    const enum MoveType { PLACE, RESIGN } type;

    static Move play_at(Color color, Pos p){
        return {color, p, PLACE};
    }
    static Move resign(Color color){
        return {color, {}, RESIGN};
    }
private:
    Move(Color c, Pos p, MoveType t) : color(c), pos(p), type(t) {}
};

//template<int size>
class Bot {
    enum BotLevel {
        JOKE, EASY, MEDIUM, HARD, CRAZY, DEMON
    };

    Board* const board;
    const Color color;

    int mcts_visits{}, ladder_depth{}, anti_ladder_depth{}, minimax_depth{};
    bool anti_ladder_nearest{}, can_resign{}, minimax_ladder{};

    Bot(BotLevel level, Color color, Board& board) : board(&board), color(color) {
        switch (level) {
            case JOKE:
                mcts_visits = 5;
                break;
            case EASY:
                mcts_visits = 50;
                minimax_depth = 1;
                ladder_depth = anti_ladder_depth = 4;
                break;
            case MEDIUM:
                mcts_visits = 100;
                minimax_depth = 1;
                ladder_depth = anti_ladder_depth = 6;
                break;
            case HARD:
                mcts_visits = 100;
                minimax_depth = 1;
                ladder_depth = anti_ladder_depth = 6;
                anti_ladder_nearest = can_resign = true;
                break;
            case CRAZY:
                mcts_visits = 250;
                minimax_depth = 1;
                ladder_depth = anti_ladder_depth = 10;
                anti_ladder_nearest = minimax_ladder = can_resign = true;
                break;
            case DEMON:
                mcts_visits = 500;
                minimax_depth = 2;
                ladder_depth = anti_ladder_depth = 10;
                anti_ladder_nearest = can_resign = true;
                break;
        }
    }

    bool play(Move m) {
        return m.type != Move::MoveType::PLACE || board->place_stone(m.color, m.pos);
    }

    Move get_move() {
        {   // Try to capture if possible
            Pos p;
            if(find_capture_move(p)){
                return Move::play_at(color, p);
            }
        }

        {   // Prevent captures
            std::vector<Pos> p;
            if(find_anti_capture_moves(p)){
                if(!p.empty()) {
                    return Move::play_at(color, p[std::rand() % (int) p.size()]);
                }
            }
            else return Move::resign(color);
        }

        {   // Try to play a ladder if possible
            Pos p;
            if(find_ladder_move(p)){
                return Move::play_at(color, p);
            }
        }

        {   // Prevent ladders
            std::vector<Pos> p;
            if(find_anti_ladder_moves(p)){
                if(!p.empty()) {
                    return Move::play_at(color, p[std::rand() % (int) p.size()]);
                }
            }
            else return Move::resign(color);
        }

        // Use minimax

        if (minimax_depth > 0) {
            if ((l = t(l = find_minimax_moves(this.board, e), o)).length > 0) return r = Math.floor(Math.random() * l.length), i = l[r];
            if (this.can_resign) return Move::resign(color);
        }
        if (mcts_visits > 0) {
            let n;
            const a = t(new h(this.board, true).list_moves(), o);
            for (let t of a) if (null == board.grid_get_string(t) && !this.is_point_an_eye(this.board, t, e) && is_valid_move(this.board, t, e)) {
                    const _ = {
                            point: t, visits: 0, wins: 0, losses: 0
                    };
                    l.length++, l[l.length - 1] = _
            }
            if (0 == l.length) return "pass";
            for (let t = 0; t < l.length; t++) {
                const set = l[t];
                for (let t = 0; t < mcts_visits; t++) n = play_random_game(this.board, e, set.point), set.visits++, n == e ? set.wins++ : n == curr(e) && set.losses++
            }

            function d(e) {
                return l[e].wins / (0 == l[e].losses ? .1 : l[e].losses)
            }

            let u = Array();
            u.length = 1, u[0] = 0;
            let c = d(0), g = 0;
            for (let e = 1; e < l.length; e++) (g = d(e)) == c ? (u.length++, u[u.length - 1] = e) : g > c && ((u = Array()).length = 1, u[0] = e, c = g);
            return i = u[r = Math.floor(Math.random() * u.length)], l[i].point
        }
        return "pass";
    }

    void play_random_game(e, t, r) {
        const i = e.board_copy();
        i.place_stone(t, r);
        let o, n, a, h = t;
        const _ = Array();
        for (let e = 0; e < i.num_cols; e++) for (let t = 0; t < i.num_rows; t++) {
                let r = new Pos(e, t);
                null == i.grid_get_string(r) && (_.length++, _[_.length - 1] = r)
            }
        while(true) {
            if (h = curr(h), n = null, isInAtari(i, curr(h))) return h;
            if (0 == (a = find_anti_capture_moves(i, h))) return curr(h);
            if (a.length > 0 && (n = a[0]), null != n) for (let e = 0; e < _.length - 1; e++) if (n = _[e]) {
                        _[e] = _[_.length - 1], _.length--;
                        break
                    }
            if (null == n) for (; 0 != _.length && (n = _[o = Math.floor(Math.random() * _.length)], _[o] = _[_.length - 1], _.length--, is_point_an_eye(i, n, h)) && !this.is_valid_move(i, n, h);) ;
            if (null == n) break;
            i.place_stone(h, n)
        }
        return;
    }

    bool isInAtari() {
        for (let r of e.strings) if (r.color == t && 1 == r.num_liberties()) return true;
        return false
    }

    find_capture_moves(e, t) {
        const r = new Positions;
        for (let i of e.strings) if (i.color != t && 1 == i.num_liberties()) {
            let curr = i.liberties.first();
            is_move_violates_ko(e, curr, t) || r.add_if_new(curr)
        }
        return Array.from(r.elements)
    }

    find_anti_capture_moves(e, t) {
        const r = new Positions;
        for (let i of e.strings) if (i.color == t && 1 == i.num_liberties()) {
            let curr = i.liberties.first();
            if (this.is_move_self_capture(this.board, curr, t)) {
                if (this.can_resign) return false
            } else r.add_if_new(curr);
            if (this.can_resign) {
                let r = e.board_copy();
                if (r.place_stone(t, curr), isInAtari(r, t)) return false
            }
        }
        return Array.from(r.elements)
    }

    find_ladder_move(e, t, r = 1, i = false) {
        if (i) {
            if (r > anti_ladder_depth) return null
        } else if (r > ladder_depth) return null;
        for (let r of e.strings) if (r.color != t && 1 == r.num_liberties()) return true;
        let l = null;
        for (let a of e.strings) if (a.color != t && 2 == a.num_liberties()) {
            for (let h of a.liberties.elements) if (this.is_valid_move(e, h, t)) {
                const o = e.board_copy();
                if (o.place_stone(t, h), !this.isInAtari(o, t)) {
                    let n = null;
                    for (let e of o.strings) e.color != t && 1 == e.num_liberties() && (n = e.liberties.first());
                    if (null == n) return false;
                    if (o.place_stone(curr(t), n), find_ladder_move(o, t, r + 1, i)) {
                        l = h;
                        break
                    }
                }
            }
            if (null != l) break
        }
        return null == l ? null : 1 != r || l
    }

    find_anti_ladder_moves(e, t) {
        let r, i, o = Array();
        if (null == find_ladder_move(e, curr(t), 1, true)) return o;
        for (let n = 0; n < e.num_cols; n++) for (let a = 0; a < e.num_cols; a++) i = new Pos(n, a), is_valid_move(e, i, t) && ((r = e.board_copy()).place_stone(t, i),
                        isInAtari(r, t) || null == find_ladder_move(r, curr(t), 1, true) && (o.length++, o[o.length - 1] = i));
        if (this.anti_ladder_nearest) {
            var n = Array();
            for (let r of o) {
                var a = false, h = r.neighbors().concat(r.corners());
                h = r.neighbors();
                for (let r = 0; r < h.length; r++) {
                    let i = h[r];
                    if (e.is_on_grid(i)) {
                        let r = e.grid_get_string(i);
                        if (null != r && r.color == t) {
                            a = true;
                            break
                        }
                    }
                }
                a && (n.length++, n[n.length - 1] = r)
            }
            n.length > 0 && (o = n)
        }
        return o
    }

    find_minimax_moves(const Board& board, Color color) {
        Color enemy = ~color;
        
        function l(board, curr, o, n = 1) {
            function a(board) {
                let r, i = null, curr = null;
                for (let l of e.strings) r = l.num_liberties(), l.color == color ? (null == i || i > r) && (i = r) : (null == curr || curr > r) && (curr = r);
                return i - curr
            }

            const h = board.board_copy(), _ = curr.policy_copy();
            if (h.place_stone(color, o), _.add(h, o), i.isInAtari(h, color)) return -1e3;
            if (i.ladder_depth > 0 && i.minimax_ladder && null != i.find_ladder_move(h, enemy)) return -1e3;
            const set = Array();
            let d = false;
            for (let e of h.strings) if (board.color == enemy && 1 == board.num_liberties()) {
                d = true;
                let t = board.liberties.first();
                if (i.is_move_self_capture(h, t, enemy)) return 1e3;
                set.length++, set[set.length - 1] = t
            }
            if (d && set.length > 1) return 1e3;
            if (!d) for (let e of _.list_moves()) i.is_valid_move(h, board, enemy) && !i.is_point_an_eye(h, board, enemy) && (set.length++, set[set.length - 1] = board);
            let u, c, g, m, w = 1e3;
            for (let e of set) {
                u = h.board_copy(), c = _.policy_copy(), u.place_stone(enemy, e), c.add(u, e);
                const board = i.isInAtari(u, enemy);
                let v = false;
                if (!board && i.ladder_depth > 0 && i.minimax_ladder && (v = null != i.find_ladder_move(u, color)), board) g = 1e3; else if (v) g = 1e3; else if (n == i.minimax_depth) g = a(u); else {
                    g = null;
                    const p = Array();
                    let y = false;
                    for (let e of u.strings) if (e.color == color && 1 == e.num_liberties()) {
                        y = true;
                        let r = e.liberties.first();
                        i.is_move_self_capture(u, r, color) && (g = -1e3), p.length++, p[p.length - 1] = r
                    }
                    if (y && p.length > 1 && (g = -1e3), !y) for (let e of c.list_moves()) i.is_valid_move(u, e, color) && !i.is_point_an_eye(u, e, color) && (p.length++, p[p.length - 1] = e);
                    if (null == g) for (let e of p) if (m = l(u, c, e, n + 1), (null == g || m > g) && (g = m), 1e3 == g) break
                }
                if (null != g && (g < w && (w = g), -1e3 == w)) break
            }
            return w
        }

        var o, n = -999, a = Array(), _ = Array(), set = new h(board);
        for (let r of set.list_moves()) is_valid_move(board, enemy, color) && !this.is_point_an_eye(board, enemy, color) && (_.length++, _[_.length - 1] = enemy);
        for (let t of _) (o = l(board, set, t)) > n && (n = o, a = Array()), o == n && (a.length++, a[a.length - 1] = t);
        return a
    }

    static bool is_point_an_eye(const Board& board, Pos pos, Color color) {
        if (board.getGroupPtr(pos)) return false;

        for (auto p : pos.neighbors())
            if (Board::is_pos_valid(p) && board[p]->color != color)
                return false;

        int num_corners = 0, side_corners = 0;
        bool is_center_eye = true;

        for (Pos p : pos.corners()) {
            if (Board::is_pos_valid(p)) {
                if (board[p]->color == color)
                    num_corners++;
            } else {
                is_center_eye = false;
                side_corners += 1;
            }
        }
        return is_center_eye ? num_corners >= 3 : side_corners + num_corners == 4;
    }
};


/*
        var d = Array();
onmessage = function (e) {
    const t = e.data.command;
    let r = "", i = "";
    if ("constructor" === t) {
        let t = void 0 !== e.data.cols ? e.data.cols : 9, i = void 0 !== e.data.rows ? e.data.rows : 9,
                curr = void 0 !== e.data.BotLevel ? e.data.BotLevel : 1, l = new _(t, i, curr);
        d[l.bot_id] = l, r = l.bot_id
    } else {
        var curr = e.data.gtp_id;
        if (d[curr] !== undefined && d[curr] !== null) {
            let l = d[curr];
            switch (t) {
                case "busy":
                    r = l.busy();
                    break;
                case "set_bot_level": {
                    let t = e.data.BotLevel;
                    if (0 == t) {
                        let i = e.data.params;
                        r = l.set_bot_level(t, i.mcts_visits, i.ladder_depth, i.anti_ladder_depth, i.anti_ladder_nearest, i.minimax_depth, i.minimax_ladder, i.capture_prob, i.can_resign)
                    } else r = l.set_bot_level(t)
                }
                    break;
                case "request": {
                    let t = e.data.request;
                    r = l.request(t)
                }
                    break;
                case "destructor":
                    void 0 !== d[curr] && (l.destructor(), d[curr] = null);
                    break;
                default:
                    r = null, i = "Unknown command"
            }
        } else r = null, i = "Not found GtpInterface (id:" + curr + ")"
    }
    postMessage({
                        command: t, result: r, error: i
                })
}

 */