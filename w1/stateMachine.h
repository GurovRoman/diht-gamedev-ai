#pragma once
#include <vector>
#include <flecs.h>

class State
{
public:
  virtual void enter() = 0;
  virtual void exit() = 0;
  virtual void act(float dt, flecs::world &ecs, flecs::entity entity) = 0;
};

class StateTransition
{
public:
  virtual ~StateTransition() {}
  virtual bool isAvailable(flecs::world &ecs, flecs::entity entity) const = 0;
};

class StateMachine : public State
{
  int curStateIdx = 0;
  bool resetting_on_start;
  std::vector<State*> states;
  std::vector<std::vector<std::pair<StateTransition*, int>>> transitions;
public:
  explicit StateMachine(bool resetting_on_start = false);
  StateMachine(const StateMachine &sm) = default;
  StateMachine(StateMachine &&sm) = default;

  ~StateMachine();

  StateMachine &operator=(const StateMachine &sm) = default;
  StateMachine &operator=(StateMachine &&sm) = default;

  void enter() override;
  void exit() override;
  void act(float dt, flecs::world& ecs, flecs::entity entity) override;

  int addState(State *st);
  void addTransition(StateTransition *trans, int from, int to);
};

