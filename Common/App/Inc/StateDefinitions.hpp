# pragma once

#include <stdint.h>
#include <stdio.h>

#include "HardwareActions.hpp"
#include "SystemState.hpp"

#include "CStateMachine.hpp"

// ---- GLOBAL STATE MACHINE ----
enum class State : uint32_t {
    IDLE            = SystemState::IDLE,
    CONNECTED       = SystemState::CONNECTED,
    ARMED           = SystemState::ARMED,
    MISSION         = SystemState::MISSION,
    CONFIG          = SystemState::CONFIG,
    SOFT_STOP       = SystemState::SOFT_STOP,
    HARD_STOP       = SystemState::HARD_STOP,
    ERROR           = SystemState::ERROR,
    EMERGENCY       = SystemState::EMERGENCY,
};

inline const char* stateToString(State s) {
    switch (s) {
        case State::IDLE:             return "IDLE";
        case State::CONNECTED:        return "CONNECTED";
        case State::ARMED:            return "ARMED";
        case State::MISSION:          return "MISSION";
        case State::CONFIG:           return "CONFIG";
        case State::SOFT_STOP:        return "SOFT_STOP";
        case State::HARD_STOP:        return "HARD_STOP";
        case State::ERROR:            return "ERROR";
        case State::EMERGENCY:        return "EMERGENCY";
        
        default:
            return "UNKNOWN STATE";
    }
}

enum class Trigger : uint32_t {
    // Triggers ros system
    REBOOT        = HardwareActions::REBOOT,
    ARM           = HardwareActions::ARM,
    DISARM        = HardwareActions::DISARM,
    ENTER_CONFIG  = HardwareActions::ENTER_CONFIG,
    EXIT_CONFIG   = HardwareActions::EXIT_CONFIG,
    START_MISSION = HardwareActions::START_MISSION,
    END_MISSION   = HardwareActions::END_MISSION,

    // Triggers SMT
    SOFT_STOP_COMPLETE = HardwareActions::SOFT_STOP_COMPLETE,
    HARD_STOP_COMPLETE = HardwareActions::HARD_STOP_COMPLETE,

    // Triggers common
    SOFT_STOP          = HardwareActions::SOFT_STOP,
    HARD_STOP          = HardwareActions::HARD_STOP,
    ENTER_ERROR        = HardwareActions::ENTER_ERROR,
    ENTER_EMERGENCY    = HardwareActions::ENTER_EMERGENCY,

    // Triggers internal
    CONNECT       = 101,
    DISCONNECT    = 102,
};

inline const char* triggerToString(Trigger t) {
    static char unknown_str[32];
    switch (t) {
        case Trigger::ARM:                return "ARM";
        case Trigger::DISARM:             return "DISARM";
        case Trigger::ENTER_CONFIG:       return "ENTER_CONFIG";
        case Trigger::EXIT_CONFIG:        return "EXIT_CONFIG";
        case Trigger::START_MISSION:      return "START_MISSION";
        case Trigger::END_MISSION:        return "END_MISSION";

        case Trigger::SOFT_STOP:          return "SOFT_STOP";
        case Trigger::HARD_STOP:          return "HARD_STOP";
        case Trigger::SOFT_STOP_COMPLETE: return "SOFT_STOP_COMPLETE";
        case Trigger::HARD_STOP_COMPLETE: return "HARD_STOP_COMPLETE";
        case Trigger::ENTER_ERROR:        return "ENTER_ERROR";
        case Trigger::ENTER_EMERGENCY:    return "ENTER_EMERGENCY";

        case Trigger::REBOOT:             return "REBOOT";

        case Trigger::CONNECT:            return "CONNECT";
        case Trigger::DISCONNECT:         return "DISCONNECT";
        default:
            snprintf(unknown_str, sizeof(unknown_str), "UNKNOWN_TRIGGER %d", (int)t);
            return unknown_str;
    }
}

static constexpr CStateMachine<State, Trigger>::Transition transitions[] = {
    // Standard workflow
    {State::IDLE,      Trigger::CONNECT,            State::CONNECTED},
    {State::CONNECTED, Trigger::DISCONNECT,         State::IDLE},
    {State::CONNECTED, Trigger::ENTER_CONFIG,       State::CONFIG},
    {State::CONFIG,    Trigger::EXIT_CONFIG,        State::CONNECTED},
    {State::CONNECTED, Trigger::ARM,                State::ARMED},
    {State::ARMED,     Trigger::DISARM,             State::CONNECTED},
    {State::ARMED,     Trigger::START_MISSION,      State::MISSION},
    {State::MISSION,   Trigger::END_MISSION,        State::ARMED},

    // STOPS
    {State::MISSION,   Trigger::SOFT_STOP,          State::SOFT_STOP},
    {State::SOFT_STOP, Trigger::SOFT_STOP_COMPLETE, State::ARMED},

    {State::MISSION,   Trigger::HARD_STOP,          State::HARD_STOP},
    {State::HARD_STOP, Trigger::HARD_STOP_COMPLETE, State::ARMED},

    // Global Error
    {State::IDLE,      Trigger::ENTER_ERROR,        State::ERROR},
    {State::CONNECTED, Trigger::ENTER_ERROR,        State::ERROR},
    {State::ARMED,     Trigger::ENTER_ERROR,        State::ERROR},
    {State::MISSION,   Trigger::ENTER_ERROR,        State::ERROR},
    {State::CONFIG,    Trigger::ENTER_ERROR,        State::ERROR},
    {State::SOFT_STOP, Trigger::ENTER_ERROR,        State::ERROR},
    {State::HARD_STOP, Trigger::ENTER_ERROR,        State::ERROR},

    // Global EMERGENCY
    {State::IDLE,      Trigger::ENTER_EMERGENCY,    State::EMERGENCY},
    {State::CONNECTED, Trigger::ENTER_EMERGENCY,    State::EMERGENCY},
    {State::ARMED,     Trigger::ENTER_EMERGENCY,    State::EMERGENCY},
    {State::MISSION,   Trigger::ENTER_EMERGENCY,    State::EMERGENCY},
    {State::CONFIG,    Trigger::ENTER_EMERGENCY,    State::EMERGENCY},
    {State::SOFT_STOP, Trigger::ENTER_EMERGENCY,    State::EMERGENCY},
    {State::HARD_STOP, Trigger::ENTER_EMERGENCY,    State::EMERGENCY},
    {State::ERROR,     Trigger::ENTER_EMERGENCY,    State::EMERGENCY}
};

static constexpr uint32_t transitionCount = sizeof(transitions) / sizeof(CStateMachine<State, Trigger>::Transition);


// ---- CONTROLLER STATE MACHINE ----
enum class MissionSubState : uint32_t {
    OFF                  = 1,
    HOLDING_POSITION     = 2,
    EXECUTING_TRAJECTORY = 3,
    TRAJECTORY_FINISHED  = 4,
};

enum class MissionTrigger : uint32_t {
    START               = 1,
    STOP                = 2,
    START_TRAJECTORY    = 5,
    MOTION_COMPLETE     = 6,
    END_TRAJECTORY      = 7,
};

inline const char* MissionTriggerToString(MissionTrigger t) {
    switch (t) {
        case MissionTrigger::START:               return "START";
        case MissionTrigger::STOP:                return "STOP";
        case MissionTrigger::START_TRAJECTORY:    return "START_TRAJECTORY";
        case MissionTrigger::MOTION_COMPLETE:     return "MOTION_COMPLETE";
        case MissionTrigger::END_TRAJECTORY:      return "END_TRAJECTORY";
        default:                                     return "UNKNOWN_INT_TRIGGER";
    }
}

static constexpr CStateMachine<MissionSubState, MissionTrigger>::Transition MissionTransitions[] = {
    // Standard workflow
    {MissionSubState::OFF,                  MissionTrigger::START,            MissionSubState::HOLDING_POSITION},
    {MissionSubState::HOLDING_POSITION,     MissionTrigger::START_TRAJECTORY, MissionSubState::EXECUTING_TRAJECTORY},
    {MissionSubState::EXECUTING_TRAJECTORY, MissionTrigger::MOTION_COMPLETE,  MissionSubState::TRAJECTORY_FINISHED},
    {MissionSubState::TRAJECTORY_FINISHED,  MissionTrigger::END_TRAJECTORY,   MissionSubState::HOLDING_POSITION},

    // STOPs
    {MissionSubState::OFF,                  MissionTrigger::STOP,            MissionSubState::OFF},
    {MissionSubState::HOLDING_POSITION,     MissionTrigger::STOP,            MissionSubState::OFF},
    {MissionSubState::EXECUTING_TRAJECTORY, MissionTrigger::STOP,            MissionSubState::OFF},
    {MissionSubState::TRAJECTORY_FINISHED,  MissionTrigger::STOP,            MissionSubState::OFF},

    {MissionSubState::EXECUTING_TRAJECTORY, MissionTrigger::END_TRAJECTORY,  MissionSubState::HOLDING_POSITION},
};

static constexpr uint32_t MissionTransitionCount = sizeof(MissionTransitions) / sizeof(CStateMachine<MissionSubState, MissionTrigger>::Transition);

// ---- STREAM/ROS STATE MACHINE ----
enum class IntStreamState : uint32_t {
    IDLE                    = 0,
    CONNECTED               = 1,
    TRAJECTORY_INIT         = 2,
    TRAJECTORY_STREAMING    = 3,
    TRAJECTORY_FINISHED     = 4
};

enum class IntStreamTrigger : uint32_t {
    CONNECT              = 1,
    DISCONNECT           = 2,
    RECEIVE_START_FRAME  = 3,
    STREAM_ESTABLISHED   = 5,
    STREAM_COMPLETE      = 6,
    ACKNOWLEDGE_FINISHED = 7,
    STREAM_ERROR         = 8
};

inline const char* intStreamTriggerToString(IntStreamTrigger t) {
    switch (t) {
        case IntStreamTrigger::CONNECT:              return "CONNECT";
        case IntStreamTrigger::DISCONNECT:           return "DISCONNECT";
        case IntStreamTrigger::RECEIVE_START_FRAME:  return "RECEIVE_START_FRAME";
        case IntStreamTrigger::STREAM_ESTABLISHED:   return "STREAM_ESTABLISHED";
        case IntStreamTrigger::STREAM_COMPLETE:      return "STREAM_COMPLETE";
        case IntStreamTrigger::ACKNOWLEDGE_FINISHED: return "ACKNOWLEDGE_FINISHED";
        case IntStreamTrigger::STREAM_ERROR:         return "STREAM_ERROR";
        default:                                     return "UNKNOWN_STREAM_TRIGGER";
    }
}

static constexpr CStateMachine<IntStreamState, IntStreamTrigger>::Transition IntStreamTransitions[] = {
    // Connection management
    {IntStreamState::IDLE,                  IntStreamTrigger::CONNECT,             IntStreamState::CONNECTED},
    {IntStreamState::CONNECTED,             IntStreamTrigger::DISCONNECT,          IntStreamState::IDLE},

    // Trajectory Streaming
    {IntStreamState::CONNECTED,             IntStreamTrigger::RECEIVE_START_FRAME, IntStreamState::TRAJECTORY_INIT},
    {IntStreamState::TRAJECTORY_INIT,       IntStreamTrigger::STREAM_ESTABLISHED,  IntStreamState::TRAJECTORY_STREAMING},
    {IntStreamState::TRAJECTORY_INIT,       IntStreamTrigger::STREAM_COMPLETE,     IntStreamState::TRAJECTORY_FINISHED},
    {IntStreamState::TRAJECTORY_STREAMING,  IntStreamTrigger::STREAM_COMPLETE,     IntStreamState::TRAJECTORY_FINISHED},
    {IntStreamState::TRAJECTORY_STREAMING,  IntStreamTrigger::ACKNOWLEDGE_FINISHED,IntStreamState::CONNECTED},
    {IntStreamState::TRAJECTORY_FINISHED,   IntStreamTrigger::ACKNOWLEDGE_FINISHED,IntStreamState::CONNECTED},
    
    // STREAM_ERROR Handling
    {IntStreamState::TRAJECTORY_INIT,       IntStreamTrigger::STREAM_ERROR,        IntStreamState::CONNECTED},
    {IntStreamState::TRAJECTORY_STREAMING,  IntStreamTrigger::STREAM_ERROR,        IntStreamState::CONNECTED},
    {IntStreamState::TRAJECTORY_FINISHED,   IntStreamTrigger::STREAM_ERROR,        IntStreamState::CONNECTED},
    {IntStreamState::CONNECTED,             IntStreamTrigger::STREAM_ERROR,        IntStreamState::CONNECTED},

    // Global Disconnect 
    {IntStreamState::CONNECTED,             IntStreamTrigger::DISCONNECT,          IntStreamState::IDLE},
    {IntStreamState::TRAJECTORY_INIT,       IntStreamTrigger::DISCONNECT,          IntStreamState::IDLE},
    {IntStreamState::TRAJECTORY_STREAMING,  IntStreamTrigger::DISCONNECT,          IntStreamState::IDLE},
    {IntStreamState::TRAJECTORY_FINISHED,   IntStreamTrigger::DISCONNECT,          IntStreamState::IDLE}
};

static constexpr uint32_t IntStreamTransitionCount = sizeof(IntStreamTransitions) / sizeof(CStateMachine<IntStreamState, IntStreamTrigger>::Transition);
