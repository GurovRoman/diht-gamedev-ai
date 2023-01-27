#include "common.h"

template <class Tag>
static flecs::entity findClosest(flecs::world& ecs, Position pos, const flecs::entity& to_ignore = {}) {
  static auto candidatesQuery = ecs.query_builder<const Position>().term<Tag>().build();

  flecs::entity closest;
  float closestDist = FLT_MAX;
  Position closestPos;
  candidatesQuery.each([&](flecs::entity candidate, const Position& epos) {
    if (candidate == to_ignore)
      return;
    float curDist = dist(epos, pos);
    if (curDist < closestDist) {
      closestDist = curDist;
      closestPos = epos;
      closest = candidate;
    }
  });

  return closest;
}

template <ResourceType Give, ResourceType Take>
class ExchangeResource : public State
{
  uint32_t give_per_try;
  uint32_t take_per_try;
public:
  ExchangeResource(uint32_t give_per_try, uint32_t take_per_try)
      : give_per_try(give_per_try), take_per_try(take_per_try)
  {}

  void enter() override {}
  void exit() override {}
  void act(float/* dt*/, flecs::world &ecs, flecs::entity entity) override
  {
    entity.set([this](Resource<Give>& give, Resource<Take>& take) {
      if (give.amount >= give_per_try) {
        give.amount -= give_per_try;
        take.amount += take_per_try;
      }
    });
  }
};

template <class Tag>
class GotoClosestState : public State
{
public:
  void enter() override {}
  void exit() override {}
  void act(float/* dt*/, flecs::world &ecs, flecs::entity entity) override
  {
    entity.set([&](const Position& pos, Action& a) {
      flecs::entity closest = findClosest<Tag>(ecs, pos, entity);
      Position closestPos = *closest.get<Position>();

      if (ecs.is_valid(closest))
        a.action = move_towards(pos, closestPos);
    });
  }
};

template <typename Tag>
class HealClosestState : public State
{
  const float heal_amount;
  const uint32_t heal_charge_turns;
  uint32_t current_heal_charge = 0;
public:
  HealClosestState(float heal_amount, uint32_t heal_charge_turns)
      : heal_amount(heal_amount), heal_charge_turns(heal_charge_turns) {}
  void enter() override {}
  void exit() override {}
  void act(float dt, flecs::world &ecs, flecs::entity entity) override
  {
    if (current_heal_charge < heal_charge_turns) {
      current_heal_charge += 1;
      return;
    }

    current_heal_charge = 0;

    flecs::entity target;
    if constexpr (std::is_same_v<Tag, TargetSelf>) {
      target = entity;
    } else {
      target = findClosest<Tag>(ecs, *entity.get<Position>(), entity);
    }

    target.set([&](Hitpoints& hp) {
      hp.hitpoints += heal_amount;
    });
  }
};

template <ResourceType ResType>
class HaveResourceTransition : public StateTransition
{
  uint32_t amount;
public:
  explicit HaveResourceTransition(uint32_t amount) : amount(amount) {}

  bool isAvailable(flecs::world &ecs, flecs::entity entity) const override
  {
    bool result = false;
    entity.get([&](const Resource<ResType>& res) {
      result = res.amount >= amount;
    });
    return result;
  }
};

template<class Tag>
class InRangeTransition : public StateTransition
{
  float triggerDist;
public:
  explicit InRangeTransition(float in_dist) : triggerDist(in_dist) {}
  bool isAvailable(flecs::world &ecs, flecs::entity entity) const override
  {
    static auto query = ecs.query_builder<const Position>().term<Tag>().build();
    bool found = false;
    entity.get([&](const Position& pos) {
      query.each([&](flecs::entity other, const Position& epos) {
        if (entity == other)
          return;
        float curDist = dist(epos, pos);
        found |= curDist <= triggerDist;
      });
    });
    return found;
  }
};


template<ResourceType Give, ResourceType Take>
State* create_exchange_resource_state(uint32_t give_per_try, uint32_t take_per_try) {
  return new ExchangeResource<Give, Take>(give_per_try, take_per_try);
}

template<class Tag>
State* create_goto_closest_state() {
  return new GotoClosestState<Tag>();
}

template<class Tag>
State* create_heal_closest_state(float heal_amount, uint32_t heal_charge_turns)
{
  return new HealClosestState<Tag>(heal_amount, heal_charge_turns);
}


template<ResourceType ResType>
StateTransition* create_have_resource_transition(uint32_t amount) {
  return new HaveResourceTransition<ResType>(amount);
}

template<class Tag>
StateTransition* create_in_range_transition(float triggerDist) {
  return new InRangeTransition<Tag>(triggerDist);
}
