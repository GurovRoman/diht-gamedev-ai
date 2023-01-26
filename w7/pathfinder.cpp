#include "pathfinder.h"
#include "dungeonUtils.h"
#include "math.h"
#include <algorithm>
#include <unordered_set>

float heuristic(IVec2 lhs, IVec2 rhs) {
  return sqrtf(sqr(float(lhs.x - rhs.x)) + sqr(float(lhs.y - rhs.y)));
};

template <typename T> static size_t coord_to_idx(T x, T y, size_t w) {
  return size_t(y) * w + size_t(x);
}

static std::vector<IVec2> reconstruct_path(std::vector<IVec2> prev, IVec2 to,
                                           size_t width) {
  IVec2 curPos = to;
  std::vector<IVec2> res = {curPos};
  while (prev[coord_to_idx(curPos.x, curPos.y, width)] != IVec2{-1, -1}) {
    curPos = prev[coord_to_idx(curPos.x, curPos.y, width)];
    res.insert(res.begin(), curPos);
  }
  return res;
}

static std::vector<IVec2> find_path_a_star(const DungeonData &dd, IVec2 from,
                                           IVec2 to, IVec2 lim_min,
                                           IVec2 lim_max) {
  if (from.x < 0 || from.y < 0 || from.x >= int(dd.width) ||
      from.y >= int(dd.height))
    return std::vector<IVec2>();
  size_t inpSize = dd.width * dd.height;

  std::vector<float> g(inpSize, std::numeric_limits<float>::max());
  std::vector<float> f(inpSize, std::numeric_limits<float>::max());
  std::vector<IVec2> prev(inpSize, {-1, -1});

  auto getG = [&](IVec2 p) -> float {
    return g[coord_to_idx(p.x, p.y, dd.width)];
  };
  auto getF = [&](IVec2 p) -> float {
    return f[coord_to_idx(p.x, p.y, dd.width)];
  };

  g[coord_to_idx(from.x, from.y, dd.width)] = 0;
  f[coord_to_idx(from.x, from.y, dd.width)] = heuristic(from, to);

  std::vector<IVec2> openList = {from};
  std::vector<IVec2> closedList;

  while (!openList.empty()) {
    size_t bestIdx = 0;
    float bestScore = getF(openList[0]);
    for (size_t i = 1; i < openList.size(); ++i) {
      float score = getF(openList[i]);
      if (score < bestScore) {
        bestIdx = i;
        bestScore = score;
      }
    }
    if (openList[bestIdx] == to)
      return reconstruct_path(prev, to, dd.width);
    IVec2 curPos = openList[bestIdx];
    openList.erase(openList.begin() + bestIdx);
    if (std::find(closedList.begin(), closedList.end(), curPos) !=
        closedList.end())
      continue;
    size_t idx = coord_to_idx(curPos.x, curPos.y, dd.width);
    closedList.emplace_back(curPos);
    auto checkNeighbour = [&](IVec2 p) {
      // out of bounds
      if (p.x < lim_min.x || p.y < lim_min.y || p.x >= lim_max.x ||
          p.y >= lim_max.y)
        return;
      size_t idx = coord_to_idx(p.x, p.y, dd.width);
      // not empty
      if (dd.tiles[idx] == dungeon::wall)
        return;
      float edgeWeight = 1.f;
      float gScore =
          getG(curPos) + 1.f * edgeWeight; // we're exactly 1 unit away
      if (gScore < getG(p)) {
        prev[idx] = curPos;
        g[idx] = gScore;
        f[idx] = gScore + heuristic(p, to);
      }
      bool found =
          std::find(openList.begin(), openList.end(), p) != openList.end();
      if (!found)
        openList.emplace_back(p);
    };
    checkNeighbour({curPos.x + 1, curPos.y + 0});
    checkNeighbour({curPos.x - 1, curPos.y + 0});
    checkNeighbour({curPos.x + 0, curPos.y + 1});
    checkNeighbour({curPos.x + 0, curPos.y - 1});
  }
  // empty path
  return std::vector<IVec2>();
}

static void find_path_between_portals(const DungeonData &dd,
                                      std::vector<PathPortal> &portals,
                                      size_t first, size_t second, IVec2 limMin,
                                      IVec2 limMax) {
  PathPortal &firstPortal = portals[first];
  PathPortal &secondPortal = portals[second];

  // check each position (to find closest dist) (could be made more optimal)
  bool noPath = false;
  size_t minDist = 0xffffffff;
  IVec2 minFrom{};
  IVec2 minTo{};
  for (size_t fromY = std::max(firstPortal.start.y, limMin.y);
       fromY <= std::min(firstPortal.end.y, limMax.y - 1) && !noPath; ++fromY) {
    for (size_t fromX = std::max(firstPortal.start.x, limMin.x);
         fromX <= std::min(firstPortal.end.x, limMax.x - 1) && !noPath;
         ++fromX) {
      for (size_t toY = std::max(secondPortal.start.y, limMin.y);
           toY <= std::min(secondPortal.end.y, limMax.y - 1) && !noPath;
           ++toY) {
        for (size_t toX = std::max(secondPortal.start.x, limMin.x);
             toX <= std::min(secondPortal.end.x, limMax.x - 1) && !noPath;
             ++toX) {
          IVec2 from{int(fromX), int(fromY)};
          IVec2 to{int(toX), int(toY)};
          std::vector<IVec2> path =
              find_path_a_star(dd, from, to, limMin, limMax);
          if (path.empty() && from != to) {
            noPath = true; // if we found that there's no path at all - we can
                           // break out
            break;
          }
          if (path.size() < minDist) {
            minDist = path.size();
            minFrom = from;
            minTo = to;
          }
        }
      }
    }
  }
  // write pathable data and length
  if (noPath)
    return;
  firstPortal.conns.push_back({second, float(minDist), minFrom, minTo});
  secondPortal.conns.push_back({first, float(minDist), minTo, minFrom});
}

void prebuild_map(flecs::world &ecs) {
  auto mapQuery = ecs.query<const DungeonData>();

  constexpr size_t splitTiles = 10;
  ecs.defer([&]() {
    mapQuery.each([&](flecs::entity e, const DungeonData &dd) {
      // go through each super tile
      const size_t width = dd.width / splitTiles;
      const size_t height = dd.height / splitTiles;

      auto check_border = [&](size_t xx, size_t yy, size_t dir_x, size_t dir_y,
                              int offs_x, int offs_y,
                              std::vector<PathPortal> &portals) {
        int spanFrom = -1;
        int spanTo = -1;
        for (size_t i = 0; i < splitTiles; ++i) {
          size_t x = xx * splitTiles + i * dir_x;
          size_t y = yy * splitTiles + i * dir_y;
          size_t nx = x + offs_x;
          size_t ny = y + offs_y;
          if (dd.tiles[y * dd.width + x] != dungeon::wall &&
              dd.tiles[ny * dd.width + nx] != dungeon::wall) {
            if (spanFrom < 0)
              spanFrom = i;
            spanTo = i;
          } else if (spanFrom >= 0) {
            // write span
            portals.push_back(
                {.start = {static_cast<int>(xx * splitTiles + spanFrom * dir_x +
                                            offs_x),
                           static_cast<int>(yy * splitTiles + spanFrom * dir_y +
                                            offs_y)},
                 .end = {static_cast<int>(xx * splitTiles + spanTo * dir_x),
                         static_cast<int>(yy * splitTiles + spanTo * dir_y)}});
            spanFrom = -1;
          }
        }
        if (spanFrom >= 0) {
          portals.push_back(
              {.start = {static_cast<int>(xx * splitTiles + spanFrom * dir_x +
                                          offs_x),
                         static_cast<int>(yy * splitTiles + spanFrom * dir_y +
                                          offs_y)},
               .end = {static_cast<int>(xx * splitTiles + spanTo * dir_x),
                       static_cast<int>(yy * splitTiles + spanTo * dir_y)}});
        }
      };

      std::vector<PathPortal> portals;
      std::vector<std::vector<size_t>> tilePortalsIndices;

      auto push_portals = [&](size_t x, size_t y, int offs_x, int offs_y,
                              const std::vector<PathPortal> &new_portals) {
        for (const PathPortal &portal : new_portals) {
          size_t idx = portals.size();
          portals.push_back(portal);
          tilePortalsIndices[y * width + x].push_back(idx);
          tilePortalsIndices[(y + offs_y) * width + x + offs_x].push_back(idx);
        }
      };
      for (size_t y = 0; y < height; ++y)
        for (size_t x = 0; x < width; ++x) {
          tilePortalsIndices.push_back(std::vector<size_t>{});
          // check top
          if (y > 0) {
            std::vector<PathPortal> topPortals;
            check_border(x, y, 1, 0, 0, -1, topPortals);
            push_portals(x, y, 0, -1, topPortals);
          }
          // left
          if (x > 0) {
            std::vector<PathPortal> leftPortals;
            check_border(x, y, 0, 1, -1, 0, leftPortals);
            push_portals(x, y, -1, 0, leftPortals);
          }
        }
      for (size_t tidx = 0; tidx < tilePortalsIndices.size(); ++tidx) {
        const std::vector<size_t> &indices = tilePortalsIndices[tidx];
        size_t x = tidx % width;
        size_t y = tidx / width;
        IVec2 limMin{int((x + 0) * splitTiles), int((y + 0) * splitTiles)};
        IVec2 limMax{int((x + 1) * splitTiles), int((y + 1) * splitTiles)};
        for (size_t i = 0; i < indices.size(); ++i) {
          for (size_t j = i + 1; j < indices.size(); ++j) {
            // check path from i to j
            find_path_between_portals(dd, portals, indices[i], indices[j],
                                      limMin, limMax);
          }
        }
      }
      e.set(DungeonPortals{splitTiles, portals, tilePortalsIndices});
    });
  });
}

static std::vector<PortalConnection>
find_portal_path_a_star(const std::vector<PathPortal> &portals, size_t from_idx,
                        size_t to_idx) {
  size_t inpSize = portals.size();

  std::vector<float> g(inpSize, std::numeric_limits<float>::max());
  std::vector<float> f(inpSize, std::numeric_limits<float>::max());
  std::vector<std::pair<size_t, PortalConnection>> prev(
      inpSize, {static_cast<size_t>(-1), {}});

  auto reconstructPath = [&](size_t to) {
    std::vector<PortalConnection> res;
    size_t curIdx = to;
    while (prev[curIdx].first != -1) {
      res.push_back(prev[curIdx].second);
      curIdx = prev[curIdx].first;
    }
    std::reverse(res.begin(), res.end());
    return res;
  };

  auto portal_heuristic = [&](size_t fromIdx, size_t toIdx) {
    auto from = portals[fromIdx];
    const auto b = portals[toIdx].start;
    auto dx = std::max(0, std::max(from.start.x - b.x, b.x - from.end.x));
    auto dy = std::max(0, std::max(from.start.y - b.y, b.y - from.end.y));
    return heuristic({0, 0}, {dx, dy});
  };

  g[from_idx] = 0;
  f[from_idx] = portal_heuristic(from_idx, to_idx);

  std::vector<size_t> openList = {from_idx};
  std::unordered_set<size_t> closedSet;

  while (!openList.empty()) {
    size_t bestIdx = 0;
    float bestScore = f[openList[0]];
    for (size_t i = 1; i < openList.size(); ++i) {
      float score = f[openList[i]];
      if (score < bestScore) {
        bestIdx = i;
        bestScore = score;
      }
    }
    if (openList[bestIdx] == to_idx)
      return reconstructPath(to_idx);
    size_t curIdx = openList[bestIdx];
    openList.erase(openList.begin() + bestIdx);
    if (closedSet.contains(curIdx))
      continue;
    closedSet.insert(curIdx);

    for (const auto &conn : portals[curIdx].conns) {
      float gScore = g[curIdx] + conn.score;
      if (gScore < g[conn.connIdx]) {
        prev[conn.connIdx] = {curIdx, conn};
        g[conn.connIdx] = gScore;
        f[conn.connIdx] = gScore + portal_heuristic(conn.connIdx, to_idx);
      }
      bool found = std::find(openList.begin(), openList.end(), conn.connIdx) !=
                   openList.end();
      if (!found)
        openList.emplace_back(conn.connIdx);
    }
  }

  return {};
}

void reset_path_visualizations(flecs::world &ecs) {
  static auto pathQuery = ecs.query_builder().term<IsPathPoint>().build();
  ecs.defer([&] {
    pathQuery.each([&](flecs::entity entity) { entity.destruct(); });
  });
}

void find_and_visualize_path(flecs::world &ecs, IVec2 from, IVec2 to) {
  static auto mapQuery = ecs.query<const DungeonData, const DungeonPortals>();

  mapQuery.each([&](const DungeonData &dd, const DungeonPortals &dp) {
    auto portals = dp.portals;
    auto tilePortalsIndices = dp.tilePortalsIndices;

    auto addFakePortal = [&](IVec2 pos) {
      portals.push_back(PathPortal{
          .start = {pos.x, pos.y},
          .end = {pos.x, pos.y},
      });
      const size_t cur_idx = portals.size() - 1;

      size_t x = pos.x / dp.tileSplit;
      size_t y = pos.y / dp.tileSplit;
      IVec2 limMin{int((x + 0) * dp.tileSplit), int((y + 0) * dp.tileSplit)};
      IVec2 limMax{int((x + 1) * dp.tileSplit), int((y + 1) * dp.tileSplit)};

      const size_t width = dd.width / dp.tileSplit;
      const size_t tidx = y * width + x;
      std::vector<size_t>& indices = tilePortalsIndices[tidx];
      for (size_t to_idx : indices) {
        find_path_between_portals(dd, portals, cur_idx, to_idx, limMin, limMax);
      }

      indices.push_back(cur_idx);

      return cur_idx;
    };

    auto from_idx = addFakePortal(from);
    auto to_idx = addFakePortal(to);

    std::vector<PortalConnection> portalPath =
        find_portal_path_a_star(portals, from_idx, to_idx);

    flecs::entity prev_vert;
    auto addVertexToPathVis = [&](const IVec2 &pos) {
      auto next = ecs.entity().set(TilePosition{pos}).add<IsPathPoint>();
      if (prev_vert)
        prev_vert.add<PathPointsTo>(next);
      prev_vert = next;
    };

    for (size_t i = 0; i < portalPath.size(); ++i) {
      const auto conn = portalPath[i];
      const auto path =
          find_path_a_star(dd, conn.from, conn.to, {0, 0}, {99999, 99999});
      for (const auto &tile : path) {
        addVertexToPathVis(tile);
      }
      if (i < portalPath.size() - 1) {
        const auto nextConn = portalPath[i + 1];
        addVertexToPathVis({conn.to.x, nextConn.from.y});
      }
    }
  });
}
