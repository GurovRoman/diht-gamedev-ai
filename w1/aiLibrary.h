#pragma once

#include "ecsTypes.h"
#include "stateMachine.h"

// states
State *create_attack_enemy_state();
State *create_move_to_enemy_state();
State *create_flee_from_enemy_state();
State *create_patrol_state(float patrol_dist);
State *create_patrol_player_state(float patrol_dist);
State *create_nop_state();
StateMachine* create_sub_sm_state(bool resetting_on_start = false);

template<ResourceType Give, ResourceType Take>
State* create_exchange_resource_state(uint32_t give_per_try, uint32_t take_per_try);

template <class Tag>
State* create_goto_closest_state();

struct TargetSelf {};

template <class Tag>
State* create_heal_closest_state(float heal_amount, uint32_t heal_charge_turns);

// transitions
StateTransition *create_enemy_available_transition(float dist);
StateTransition *create_enemy_reachable_transition();
StateTransition *create_hitpoints_less_than_transition(float thres);
StateTransition *create_player_hitpoints_less_than_transition(float thres);
StateTransition *create_negate_transition(StateTransition *in);
StateTransition *create_and_transition(StateTransition *lhs, StateTransition *rhs);
StateTransition* create_chance_transition(float chance);

template<ResourceType ResType>
StateTransition* create_have_resource_transition(uint32_t amount);

template<class Tag>
StateTransition* create_in_range_transition(float triggerDist);


#include "aiLibrary.tpp"