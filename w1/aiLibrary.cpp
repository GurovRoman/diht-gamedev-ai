#include "common.h"
#include "aiLibrary.h"
#include <flecs.h>
#include <bx/rng.h>

static bx::RngShr3 rng;

class AttackEnemyState : public State
{
public:
  void enter() override {}
  void exit() override {}
  void act(float/* dt*/, flecs::world &/*ecs*/, flecs::entity /*entity*/) override {}
};

template<typename Callable>
static void on_closest_enemy_pos(flecs::world &ecs, flecs::entity entity, Callable c)
{
  static auto enemiesQuery = ecs.query<const Position, const Team>();
  entity.set([&](const Position &pos, const Team &t, Action &a)
  {
    flecs::entity closestEnemy;
    float closestDist = FLT_MAX;
    Position closestPos;
    enemiesQuery.each([&](flecs::entity enemy, const Position &epos, const Team &et)
    {
      if (t.team == et.team)
        return;
      float curDist = dist(epos, pos);
      if (curDist < closestDist)
      {
        closestDist = curDist;
        closestPos = epos;
        closestEnemy = enemy;
      }
    });
    if (ecs.is_valid(closestEnemy))
      c(a, pos, closestPos);
  });
}

class MoveToEnemyState : public State
{
public:
  void enter() override {}
  void exit() override {}
  void act(float/* dt*/, flecs::world &ecs, flecs::entity entity) override
  {
    on_closest_enemy_pos(ecs, entity, [&](Action &a, const Position &pos, const Position &enemy_pos)
    {
      a.action = move_towards(pos, enemy_pos);
    });
  }
};

class FleeFromEnemyState : public State
{
public:
  FleeFromEnemyState() {}
  void enter() override {}
  void exit() override {}
  void act(float/* dt*/, flecs::world &ecs, flecs::entity entity) override
  {
    on_closest_enemy_pos(ecs, entity, [&](Action &a, const Position &pos, const Position &enemy_pos)
    {
      a.action = inverse_move(move_towards(pos, enemy_pos));
    });
  }
};

class PatrolState : public State
{
  float patrolDist;
public:
  PatrolState(float dist) : patrolDist(dist) {}
  void enter() override {}
  void exit() override {}
  void act(float/* dt*/, flecs::world &ecs, flecs::entity entity) override
  {
    entity.set([&](const Position &pos, const PatrolPos &ppos, Action &a)
    {
      if (dist(pos, ppos) > patrolDist)
        a.action = move_towards(pos, ppos); // do a recovery walk
      else
      {
        // do a random walk
        a.action = EA_MOVE_START + (rng.gen() % (EA_MOVE_END - EA_MOVE_START));
      }
    });
  }
};

class PatrolPlayerState : public State
{
  float patrolDist;
public:
  explicit PatrolPlayerState(float dist) : patrolDist(dist) {}
  void enter() override {}
  void exit() override {}
  void act(float dt, flecs::world &ecs, flecs::entity entity) override
  {
    static auto playerQuery = ecs.query_builder<const Position>().term<IsPlayer>().build();

    entity.set([&](const Position &pos, Action &a) {
      playerQuery.each([&](const Position& ppos) {
        if (dist(pos, ppos) > patrolDist)
          a.action = move_towards(pos, ppos); // do a recovery walk
        else
        {
          // do a random walk
          a.action = EA_MOVE_START + (rng.gen() % (EA_MOVE_END - EA_MOVE_START));
        }
      });
    });
  }
};

class NopState : public State
{
public:
  void enter() override {}
  void exit() override {}
  void act(float/* dt*/, flecs::world &ecs, flecs::entity entity) override {}
};

class EnemyAvailableTransition : public StateTransition
{
  float triggerDist;
public:
  EnemyAvailableTransition(float in_dist) : triggerDist(in_dist) {}
  bool isAvailable(flecs::world &ecs, flecs::entity entity) const override
  {
    static auto enemiesQuery = ecs.query<const Position, const Team>();
    bool enemiesFound = false;
    entity.get([&](const Position &pos, const Team &t)
    {
      enemiesQuery.each([&](flecs::entity enemy, const Position &epos, const Team &et)
      {
        if (t.team == et.team)
          return;
        float curDist = dist(epos, pos);
        enemiesFound |= curDist <= triggerDist;
      });
    });
    return enemiesFound;
  }
};

class HitpointsLessThanTransition : public StateTransition
{
  float threshold;
public:
  HitpointsLessThanTransition(float in_thres) : threshold(in_thres) {}
  bool isAvailable(flecs::world &ecs, flecs::entity entity) const override
  {
    bool hitpointsThresholdReached = false;
    entity.get([&](const Hitpoints &hp)
    {
      hitpointsThresholdReached |= hp.hitpoints < threshold;
    });
    return hitpointsThresholdReached;
  }
};

class PlayerHitpointsLessThanTransition : public StateTransition
{
  float threshold;
public:
  PlayerHitpointsLessThanTransition(float in_thres) : threshold(in_thres) {}
  bool isAvailable(flecs::world &ecs, flecs::entity entity) const override
  {
    static auto playerQuery = ecs.query_builder<const Hitpoints>().term<IsPlayer>().build();

    bool hitpointsThresholdReached = false;
    playerQuery.each([&](const Hitpoints& hp) { hitpointsThresholdReached |= hp.hitpoints < threshold; });
    return hitpointsThresholdReached;
  }
};

class EnemyReachableTransition : public StateTransition
{
public:
  bool isAvailable(flecs::world &ecs, flecs::entity entity) const override
  {
    return false;
  }
};

class NegateTransition : public StateTransition
{
  const StateTransition *transition; // we own it
public:
  NegateTransition(const StateTransition *in_trans) : transition(in_trans) {}
  ~NegateTransition() override { delete transition; }

  bool isAvailable(flecs::world &ecs, flecs::entity entity) const override
  {
    return !transition->isAvailable(ecs, entity);
  }
};

class AndTransition : public StateTransition
{
  const StateTransition *lhs; // we own it
  const StateTransition *rhs; // we own it
public:
  AndTransition(const StateTransition *in_lhs, const StateTransition *in_rhs) : lhs(in_lhs), rhs(in_rhs) {}
  ~AndTransition() override
  {
    delete lhs;
    delete rhs;
  }

  bool isAvailable(flecs::world &ecs, flecs::entity entity) const override
  {
    return lhs->isAvailable(ecs, entity) && rhs->isAvailable(ecs, entity);
  }
};

class ChanceTransition : public StateTransition
{
  const float chance;
public:
  explicit ChanceTransition(float chance) : chance(chance) {}

  bool isAvailable(flecs::world &ecs, flecs::entity entity) const override
  {
    return bx::frnd(&rng) <= chance;
  }
};

// states
State *create_attack_enemy_state()
{
  return new AttackEnemyState();
}
State *create_move_to_enemy_state()
{
  return new MoveToEnemyState();
}

State *create_flee_from_enemy_state()
{
  return new FleeFromEnemyState();
}


State *create_patrol_state(float patrol_dist)
{
  return new PatrolState(patrol_dist);
}

State* create_patrol_player_state(float patrol_dist)
{
  return new PatrolPlayerState(patrol_dist);
}

State *create_nop_state()
{
  return new NopState();
}

StateMachine* create_sub_sm_state(bool resetting_on_start) {
  return new StateMachine(resetting_on_start);
}


// transitions
StateTransition *create_enemy_available_transition(float dist)
{
  return new EnemyAvailableTransition(dist);
}

StateTransition *create_enemy_reachable_transition()
{
  return new EnemyReachableTransition();
}

StateTransition *create_hitpoints_less_than_transition(float thres)
{
  return new HitpointsLessThanTransition(thres);
}

StateTransition *create_player_hitpoints_less_than_transition(float thres)
{
  return new PlayerHitpointsLessThanTransition(thres);
}

StateTransition *create_negate_transition(StateTransition *in)
{
  return new NegateTransition(in);
}
StateTransition *create_and_transition(StateTransition *lhs, StateTransition *rhs)
{
  return new AndTransition(lhs, rhs);
}

StateTransition* create_chance_transition(float chance) {
  return new ChanceTransition(chance);
}
