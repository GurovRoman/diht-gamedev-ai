#pragma once
#include <flecs.h>
#include <vector>
#include "math.h"

struct PortalConnection
{
  size_t connIdx;
  float score;
  IVec2 from;
  IVec2 to;
};

struct PathPortal
{
  IVec2 start;
  IVec2 end;
  std::vector<PortalConnection> conns;
};

struct DungeonPortals
{
  size_t tileSplit;
  std::vector<PathPortal> portals;
  std::vector<std::vector<size_t>> tilePortalsIndices;
};

void prebuild_map(flecs::world &ecs);

void reset_path_visualizations(flecs::world &ecs);
void find_and_visualize_path(flecs::world &ecs, IVec2 from, IVec2 to);
