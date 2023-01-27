#include "roguelike.h"
#include "ecsTypes.h"
#include <debugdraw/debugdraw.h>
#include "stateMachine.h"
#include "aiLibrary.h"
#include "app.h"

//for scancodes
#include <GLFW/glfw3.h>

static State* create_crafter_craft_sm_state(flecs::entity entity) {
  StateMachine* sm = create_sub_sm_state();

  int goto_market = sm->addState(create_goto_closest_state<Building<BuildingType::Market>>());
  int buy = sm->addState(create_exchange_resource_state<ResourceType::Money, ResourceType::Wood>(1, 2));
  int goto_workshop = sm->addState(create_goto_closest_state<Building<BuildingType::Workshop>>());
  int craft = sm->addState(create_exchange_resource_state<ResourceType::Wood, ResourceType::WoodenPhallus>(3, 2));
  int goto_khimki = sm->addState(create_goto_closest_state<Building<BuildingType::Khimki>>());
  int sell = sm->addState(create_exchange_resource_state<ResourceType::WoodenPhallus, ResourceType::Money>(1, 3));

  sm->addTransition(create_in_range_transition<Building<BuildingType::Market>>(1.f), goto_market, buy);
  sm->addTransition(create_in_range_transition<Building<BuildingType::Workshop>>(1.f), goto_workshop, craft);
  sm->addTransition(create_in_range_transition<Building<BuildingType::Khimki>>(1.f), goto_khimki, sell);

  sm->addTransition(create_negate_transition(create_in_range_transition<Building<BuildingType::Market>>(1.f)), buy, goto_market);
  sm->addTransition(create_negate_transition(create_in_range_transition<Building<BuildingType::Workshop>>(1.f)), craft, goto_workshop);
  sm->addTransition(create_negate_transition(create_in_range_transition<Building<BuildingType::Khimki>>(1.f)), sell, goto_khimki);

  sm->addTransition(create_have_resource_transition<ResourceType::Wood>(9), buy, goto_workshop);
  sm->addTransition(create_negate_transition(create_have_resource_transition<ResourceType::Money>(1)), buy, goto_workshop);

  sm->addTransition(create_have_resource_transition<ResourceType::WoodenPhallus>(4), craft, goto_khimki);
  sm->addTransition(create_negate_transition(create_have_resource_transition<ResourceType::Wood>(3)), craft, goto_khimki);

  sm->addTransition(
    create_and_transition(
      create_have_resource_transition<ResourceType::Wood>(3),
      create_negate_transition(create_have_resource_transition<ResourceType::WoodenPhallus>(1))),
    sell,
    goto_workshop);
  sm->addTransition(create_negate_transition(create_have_resource_transition<ResourceType::WoodenPhallus>(1)), sell, goto_market);

  return sm;
}

static State* create_crafter_eat_sm_state(flecs::entity entity) {
  StateMachine* sm = create_sub_sm_state(true);

  int goto_tavern = sm->addState(create_goto_closest_state<Building<BuildingType::Tavern>>());
  int eat = sm->addState(create_exchange_resource_state<ResourceType::Money, ResourceType::Hunger>(1, 20));

  sm->addTransition(create_in_range_transition<Building<BuildingType::Tavern>>(1.f), goto_tavern, eat);

  return sm;
}

static State* create_crafter_sleep_sm_state(flecs::entity entity) {
  StateMachine* sm = create_sub_sm_state(true);

  int goto_bed = sm->addState(create_goto_closest_state<Building<BuildingType::Bed>>());
  int sleep = sm->addState(create_nop_state());

  sm->addTransition(create_in_range_transition<Building<BuildingType::Bed>>(1.f), goto_bed, sleep);

  return sm;
}

static void add_crafter_sm(flecs::entity entity)
{
  entity
    .set(Resource<ResourceType::Hunger>{50})
    .set(Resource<ResourceType::Wood>{0})
    .set(Resource<ResourceType::WoodenPhallus>{0})
    .set(Resource<ResourceType::Money>{10});

  entity.get([&](StateMachine& sm) {
    int wander = sm.addState(create_patrol_state(30.f));
    int craft = sm.addState((create_crafter_craft_sm_state(entity)));
    int eat = sm.addState((create_crafter_eat_sm_state(entity)));
    int sleep = sm.addState((create_crafter_sleep_sm_state(entity)));

    sm.addTransition(create_negate_transition(create_have_resource_transition<ResourceType::Money>(20)), wander, craft);
    sm.addTransition(create_negate_transition(create_have_resource_transition<ResourceType::Hunger>(20)), wander, craft);

    sm.addTransition(
      create_and_transition(
        create_negate_transition(create_have_resource_transition<ResourceType::Hunger>(20)),
        create_have_resource_transition<ResourceType::Money>(3)),
      craft,
      eat);

    sm.addTransition(create_have_resource_transition<ResourceType::Hunger>(100), eat, craft); // well-fed
    sm.addTransition(
      create_and_transition(
        create_have_resource_transition<ResourceType::Hunger>(50),
        create_negate_transition(create_have_resource_transition<ResourceType::Money>(5))),
      eat,
      craft); // kinda fed but low on money
    sm.addTransition(
      create_negate_transition(create_have_resource_transition<ResourceType::Money>(3)),
      eat,
      craft); // don't cross the poverty line

    sm.addTransition(
      create_and_transition(create_have_resource_transition<ResourceType::Hunger>(80), create_chance_transition(0.04f)),
      craft,
      sleep); // fed and (possibly) tired => sleep time
    sm.addTransition(create_chance_transition(0.04f), sleep, craft); // spurious wakeup => guess we should to back to work
    sm.addTransition(
      create_negate_transition(create_have_resource_transition<ResourceType::Hunger>(5)),
      sleep,
      eat); // we overslept and are going to die of starvation

    sm.addTransition(
      create_and_transition(
        create_have_resource_transition<ResourceType::Money>(20),
        create_have_resource_transition<ResourceType::Hunger>(100)),
      craft,
      wander); // we're rich! let's wander and show off our supreme wealth
  });
}

static void add_berserker_sm(flecs::entity entity)
{
  // my love for you is like a truck, berserker
  entity.set(Hitpoints{140.f});
  entity.get([](StateMachine& sm) {
    int patrol = sm.addState(create_patrol_state(3.f));
    int moveToEnemy = sm.addState(create_move_to_enemy_state());
    int fleeFromEnemy = sm.addState(create_flee_from_enemy_state());
    int pre_rage = sm.addState(create_heal_closest_state<TargetSelf>(100, 0));
    int rage = sm.addState(create_move_to_enemy_state());

    sm.addTransition(create_enemy_available_transition(3.f), patrol, moveToEnemy);
    sm.addTransition(create_negate_transition(create_enemy_available_transition(5.f)), moveToEnemy, patrol);

    sm.addTransition(
      create_and_transition(create_hitpoints_less_than_transition(120.f), create_enemy_available_transition(5.f)),
      moveToEnemy,
      fleeFromEnemy);
    sm.addTransition(
      create_and_transition(create_hitpoints_less_than_transition(120.f), create_enemy_available_transition(3.f)),
      patrol,
      fleeFromEnemy);

    sm.addTransition(create_negate_transition(create_enemy_available_transition(7.f)), fleeFromEnemy, patrol);

    sm.addTransition(
      create_hitpoints_less_than_transition(100.f),
      fleeFromEnemy,
      pre_rage); // we buff up
    sm.addTransition(
      create_negate_transition(create_hitpoints_less_than_transition(450.f)),
      pre_rage,
      rage); // and we obliterate
  });
}

static void add_self_healer_sm(flecs::entity entity)
{
  entity.get([](StateMachine &sm)
  {
    int patrol = sm.addState(create_patrol_state(3.f));
    int fleeFromEnemy = sm.addState(create_flee_from_enemy_state());
    int healing = sm.addState(create_heal_closest_state<TargetSelf>(50.f, 4));

    sm.addTransition(create_enemy_available_transition(3.f), patrol, fleeFromEnemy);
    sm.addTransition(create_negate_transition(create_enemy_available_transition(5.f)), fleeFromEnemy, patrol);

    sm.addTransition(create_hitpoints_less_than_transition(100.f), patrol, healing);
    sm.addTransition(create_negate_transition(create_hitpoints_less_than_transition(100.f)), healing, patrol);
  });
}

static void add_friend_paladin_sm(flecs::entity entity)
{
  entity.set(Team{0});
  entity.get([](StateMachine &sm)
  {
    int followPlayer = sm.addState(create_patrol_player_state(2.f));
    int attack = sm.addState(create_move_to_enemy_state());
    int healPlayer = sm.addState(create_heal_closest_state<IsPlayer>(50, 10));

    sm.addTransition(
      create_and_transition(
        create_enemy_available_transition(4.f),
        create_and_transition(
          create_negate_transition(create_player_hitpoints_less_than_transition(30.f)),
          create_in_range_transition<IsPlayer>(8.f))),
      followPlayer,
      attack);
    sm.addTransition(create_negate_transition(create_enemy_available_transition(4.f)), attack, followPlayer);
    sm.addTransition(create_negate_transition(create_in_range_transition<IsPlayer>(8.f)), attack, followPlayer);
    sm.addTransition(create_player_hitpoints_less_than_transition(30.f), attack, followPlayer);
    // if player is badly hurt we stop attacking and follow him

    sm.addTransition(
      create_and_transition(create_player_hitpoints_less_than_transition(100.f), create_in_range_transition<IsPlayer>(4.f)),
      followPlayer,
      healPlayer);

    sm.addTransition(create_negate_transition(create_player_hitpoints_less_than_transition(100.f)), healPlayer, followPlayer);
    sm.addTransition(create_negate_transition(create_in_range_transition<IsPlayer>(4.f)), healPlayer, followPlayer);
  });
}

static flecs::entity create_monster(flecs::world &ecs, int x, int y, uint32_t color)
{
  return ecs.entity()
    .set(Position{x, y})
    .set(MovePos{x, y})
    .set(PatrolPos{x, y})
    .set(Hitpoints{100.f})
    .set(Action{EA_NOP})
    .set(Color{color})
    .set(StateMachine{})
    .set(Team{1})
    .set(NumActions{1, 0})
    .set(MeleeDamage{20.f});
}

static void create_player(flecs::world &ecs, int x, int y)
{
  ecs.entity("player")
    .set(Position{x, y})
    .set(MovePos{x, y})
    .set(Hitpoints{100.f})
    .set(Color{0xffeeeeee})
    .set(Action{EA_NOP})
    .add<IsPlayer>()
    .set(Team{0})
    .set(PlayerInput{})
    .set(NumActions{2, 0})
    .set(MeleeDamage{50.f});
}

static void create_heal(flecs::world &ecs, int x, int y, float amount)
{
  ecs.entity()
    .set(Position{x, y})
    .set(HealAmount{amount})
    .set(Color{0xff4444ff});
}

static void create_powerup(flecs::world &ecs, int x, int y, float amount)
{
  ecs.entity()
    .set(Position{x, y})
    .set(PowerupAmount{amount})
    .set(Color{0xff00ffff});
}

template <BuildingType BType>
static void create_building(flecs::world &ecs, int x, int y)
{
  ecs.entity()
    .set(Position{x, y})
    .add<Building<BType>>()
    .set(buildingTypeToColor(BType));
}

static void register_roguelike_systems(flecs::world &ecs)
{
  ecs.system<PlayerInput, Action, const IsPlayer>()
    .each([&](PlayerInput &inp, Action &a, const IsPlayer)
    {
      bool left = app_keypressed(GLFW_KEY_LEFT);
      bool right = app_keypressed(GLFW_KEY_RIGHT);
      bool up = app_keypressed(GLFW_KEY_UP);
      bool down = app_keypressed(GLFW_KEY_DOWN);
      if (left && !inp.left)
        a.action = EA_MOVE_LEFT;
      if (right && !inp.right)
        a.action = EA_MOVE_RIGHT;
      if (up && !inp.up)
        a.action = EA_MOVE_UP;
      if (down && !inp.down)
        a.action = EA_MOVE_DOWN;
      inp.left = left;
      inp.right = right;
      inp.up = up;
      inp.down = down;
    });
  ecs.system<const Position, const Color>()
    .each([&](const Position &pos, const Color color)
    {
      DebugDrawEncoder dde;
      dde.begin(0);
      dde.push();
        dde.setColor(color.color);
        dde.drawQuad(bx::Vec3(0, 0, 1), bx::Vec3(pos.x, pos.y, 0.f), 1.f);
      dde.pop();
      dde.end();
    });

  ecs.system<const Position, const Hitpoints>().term<Color>().each([&](flecs::entity e, const Position& pos, const Hitpoints& hp_) {
    auto hp = hp_.hitpoints;
    uint32_t color;

    if (hp <= 100) {
      float t = hp / 100;
      uint8_t red = 255 * (1 - t);
      uint8_t green = 255 * t;
      color = 0xFF000000 + (green << 8) + (red);
    } else {
      float t = std::min(1.f, (hp - 100) / 350);
      uint8_t green = 255 * (1 - t);
      uint8_t blue = 255 * t;
      color = 0xFF000000 + (green << 8) + (blue << 16);
    }

    DebugDrawEncoder dde;
    dde.begin(0);
    dde.push();
      dde.setColor(color);
      dde.drawQuad(bx::Vec3(0, 0, 1), bx::Vec3(pos.x + 0.5f, pos.y + 0.5f, -0.01f), 0.2f);
    dde.pop();
    dde.end();
  });
}


void init_roguelike(flecs::world &ecs)
{
  register_roguelike_systems(ecs);

  add_berserker_sm(create_monster(ecs, 10, -5, 0xff00ff00));
  add_berserker_sm(create_monster(ecs, 5, 5, 0xff00ff00));
  add_self_healer_sm(create_monster(ecs, 5, -5, 0xff44ff00));
  add_self_healer_sm(create_monster(ecs, -5, -5, 0xff44ff00));
  add_friend_paladin_sm(create_monster(ecs, -5, 5, 0xff111111));
  add_friend_paladin_sm(create_monster(ecs, -5, 3, 0xff111111));

  add_crafter_sm(create_monster(ecs, -10, 0, 0xffff00aa));

  create_player(ecs, 0, 0);

  create_powerup(ecs, 7, 7, 10.f);
  create_powerup(ecs, 10, -6, 10.f);
  create_powerup(ecs, 10, -4, 10.f);

  create_heal(ecs, -5, -5, 50.f);
  create_heal(ecs, -5, 5, 50.f);

  create_building<BuildingType::Market>(ecs, -14, -5);
  create_building<BuildingType::Workshop>(ecs, -14, -2);
  create_building<BuildingType::Khimki>(ecs, -14, 1);
  create_building<BuildingType::Tavern>(ecs, -14, 4);
  create_building<BuildingType::Bed>(ecs, -14, 7);
}

static bool is_player_acted(flecs::world &ecs)
{
  static auto processPlayer = ecs.query<const IsPlayer, const Action>();
  bool playerActed = false;
  processPlayer.each([&](const IsPlayer, const Action &a)
  {
    playerActed = a.action != EA_NOP;
  });
  playerActed |= app_keypressed(GLFW_KEY_SPACE);
  return playerActed;
}

static bool upd_player_actions_count(flecs::world &ecs)
{
  static auto updPlayerActions = ecs.query<const IsPlayer, NumActions>();
  bool actionsReached = false;
  updPlayerActions.each([&](const IsPlayer, NumActions &na)
  {
    na.curActions = (na.curActions + 1) % na.numActions;
    actionsReached |= na.curActions == 0;
  });
  return actionsReached;
}

static Position move_pos(Position pos, int action)
{
  if (action == EA_MOVE_LEFT)
    pos.x--;
  else if (action == EA_MOVE_RIGHT)
    pos.x++;
  else if (action == EA_MOVE_UP)
    pos.y++;
  else if (action == EA_MOVE_DOWN)
    pos.y--;
  return pos;
}

static void process_actions(flecs::world &ecs)
{
  static auto processActions = ecs.query<Action, Position, MovePos, const MeleeDamage, const Team>();
  static auto checkAttacks = ecs.query<const MovePos, Hitpoints, const Team>();
  // Process all actions
  ecs.defer([&]
  {
    processActions.each([&](flecs::entity entity, Action &a, Position &pos, MovePos &mpos, const MeleeDamage &dmg, const Team &team)
    {
      Position nextPos = move_pos(pos, a.action);
      bool blocked = false;
      checkAttacks.each([&](flecs::entity enemy, const MovePos &epos, Hitpoints &hp, const Team &enemy_team)
      {
        if (entity != enemy && epos == nextPos)
        {
          blocked = true;
          if (team.team != enemy_team.team)
            hp.hitpoints -= dmg.damage;
        }
      });
      if (blocked)
        a.action = EA_NOP;
      else
        mpos = nextPos;
    });
    // now move
    processActions.each([&](flecs::entity entity, Action &a, Position &pos, MovePos &mpos, const MeleeDamage &, const Team&)
    {
      pos = mpos;
      a.action = EA_NOP;
    });
  });

  static auto deleteAllDead = ecs.query<const Hitpoints>();
  ecs.defer([&]
  {
    deleteAllDead.each([&](flecs::entity entity, const Hitpoints &hp)
    {
      if (hp.hitpoints <= 0.f)
        entity.destruct();
    });
  });

  static auto playerPickup = ecs.query<const IsPlayer, const Position, Hitpoints, MeleeDamage>();
  static auto healPickup = ecs.query<const Position, const HealAmount>();
  static auto powerupPickup = ecs.query<const Position, const PowerupAmount>();
  ecs.defer([&]
  {
    playerPickup.each([&](const IsPlayer&, const Position &pos, Hitpoints &hp, MeleeDamage &dmg)
    {
      healPickup.each([&](flecs::entity entity, const Position &ppos, const HealAmount &amt)
      {
        if (pos == ppos)
        {
          hp.hitpoints += amt.amount;
          entity.destruct();
        }
      });
      powerupPickup.each([&](flecs::entity entity, const Position &ppos, const PowerupAmount &amt)
      {
        if (pos == ppos)
        {
          dmg.damage += amt.amount;
          entity.destruct();
        }
      });
    });
  });

  static auto hungerQuery = ecs.query<Resource<ResourceType::Hunger>, Hitpoints>();
  hungerQuery.each([&](Resource<ResourceType::Hunger>& hunger, Hitpoints& hp) {
    if (hunger.amount > 0) {
      hunger.amount -= 1;
      hp.hitpoints += 1;
    } else {
      hp.hitpoints -= 2;
    }
  });
}

void process_turn(flecs::world &ecs)
{
  static auto stateMachineAct = ecs.query<StateMachine>();
  if (is_player_acted(ecs))
  {
    if (upd_player_actions_count(ecs))
    {
      // Plan action for NPCs
      ecs.defer([&]
      {
        stateMachineAct.each([&](flecs::entity e, StateMachine &sm)
        {
          sm.act(0.f, ecs, e);
        });
      });
    }
    process_actions(ecs);
  }
}

void print_stats(flecs::world &ecs)
{
  bgfx::dbgTextClear();
  static auto playerStatsQuery = ecs.query<const IsPlayer, const Hitpoints, const MeleeDamage>();
  playerStatsQuery.each([&](const IsPlayer &, const Hitpoints &hp, const MeleeDamage &dmg)
  {
    bgfx::dbgTextPrintf(0, 1, 0x0f, "hp: %d", (int)hp.hitpoints);
    bgfx::dbgTextPrintf(0, 2, 0x0f, "power: %d", (int)dmg.damage);
  });

  static auto crafterStatsQuery = ecs.query<
    const Hitpoints,
    const Resource<ResourceType::Hunger>,
    const Resource<ResourceType::Money>,
    const Resource<ResourceType::Wood>,
    const Resource<ResourceType::WoodenPhallus>>();
  crafterStatsQuery.each([&](
                           const Hitpoints& hp,
                           const Resource<ResourceType::Hunger>& honger,
                           const Resource<ResourceType::Money>& mone,
                           const Resource<ResourceType::Wood>& wud,
                           const Resource<ResourceType::WoodenPhallus>& peni) {
    bgfx::dbgTextPrintf(0, 4, 0x0f, "crafter:");
    bgfx::dbgTextPrintf(0, 5, 0x0f, "health: %d", (int)hp.hitpoints);
    bgfx::dbgTextPrintf(0, 6, 0x0f, "hunger: %d", (int)honger.amount);
    bgfx::dbgTextPrintf(0, 7, 0x0f, "money: %d", (int)mone.amount);
    bgfx::dbgTextPrintf(0, 8, 0x0f, "wood: %d", (int)wud.amount);
    bgfx::dbgTextPrintf(0, 9, 0x0f, "wooden_phalluses: %d", (int)peni.amount);
  });
}

