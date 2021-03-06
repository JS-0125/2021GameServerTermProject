#pragma once
#include"default.h"
#include <list>

using namespace std;

class point {
public:
    point(int a = 0, int b = 0) { x = a; y = b; }
    bool operator ==(const point& o) { return o.x == x && o.y == y; }
    point operator +(const point& o) { return point(o.x + x, o.y + y); }
    int x, y;
};

class Map {
public:
    int w, h;
    vector<vector<bool>> m;

    Map() {}
    Map(int width, int height) {
        w = width;
        h = height;
        m.resize(height);
    }
    bool operator() (int x, int y) { return m[y][x]; }
};

class node {
public:
    bool operator == (const node& o) { return pos == o.pos; }
    bool operator == (const point& o) { return pos == o; }
    bool operator < (const node& o) { return dist + cost < o.dist + o.cost; }
    point pos, parent;
    int dist, cost;
};

class aStar {
public:
    Map m;
    point end, start;
    point neighbours[8];
    std::list<node> open;
    std::list<node> closed;

    aStar() {
        neighbours[0] = point(-1, -1); neighbours[1] = point(1, -1);
        neighbours[2] = point(-1, 1); neighbours[3] = point(1, 1);
        neighbours[4] = point(0, -1); neighbours[5] = point(-1, 0);
        neighbours[6] = point(0, 1); neighbours[7] = point(1, 0);
    }

    int calcDist(point& p) {
        // need a better heuristic
        int x = end.x - p.x, y = end.y - p.y;
        return(x * x + y * y);
    }

    bool isValid(point& p) {
        return (p.x > -1 && p.y > -1 && p.x < m.w&& p.y < m.h);
    }

    bool existPoint(point& p, int cost) {
        std::list<node>::iterator i;
        i = std::find(closed.begin(), closed.end(), p);
        if (i != closed.end()) {
            if ((*i).cost + (*i).dist < cost) return true;
            else { closed.erase(i); return false; }
        }
        i = std::find(open.begin(), open.end(), p);
        if (i != open.end()) {
            if ((*i).cost + (*i).dist < cost) return true;
            else { open.erase(i); return false; }
        }
        return false;
    }

    bool fillOpen(node& n) {
        int stepCost, nc, dist;
        point neighbour;

        for (int x = 0; x < 8; x++) {
            // one can make diagonals have different cost
            stepCost = x < 4 ? 1 : 1;
            neighbour = n.pos + neighbours[x];
            if (neighbour == end) return true;

            if (isValid(neighbour)) {
				if (m(neighbour.x, neighbour.y) == true) {
					nc = stepCost + n.cost;
					dist = calcDist(neighbour);
					if (!existPoint(neighbour, nc + dist)) {
						node t;
						t.cost = nc; t.dist = dist;
						t.pos = neighbour;
						t.parent = n.pos;
						open.push_back(t);
					}
				}
            }
        }
        return false;
    }

    bool search(point& s, point& e, Map& mp) {
        node n; end = e; start = s; m = mp;
        n.cost = 0; n.pos = s; n.parent = 0; n.dist = calcDist(s);
        open.push_back(n);
        while (!open.empty()) {
            //open.sort();
            node n = open.front();
            open.pop_front();
            closed.push_back(n);
            if (fillOpen(n)) return true;
        }
        return false;
    }

    int path(std::list<point>& path) {
        path.push_front(end);
        int cost = 1 + closed.back().cost;
        path.push_front(closed.back().pos);
        point parent = closed.back().parent;

        for (std::list<node>::reverse_iterator i = closed.rbegin(); i != closed.rend(); i++) {
            if ((*i).pos == parent && !((*i).pos == start)) {
                path.push_front((*i).pos);
                parent = (*i).parent;
            }
        }
        path.push_front(start);
        path.reverse();
        return cost;
    }

};