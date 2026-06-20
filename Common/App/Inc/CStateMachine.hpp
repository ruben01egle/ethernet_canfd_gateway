#pragma once

#include <stdint.h>

template <typename T_State, typename T_Trigger>
class CStateMachine 
{
public:
    struct Transition {
        T_State from;
        T_Trigger trigger;
        T_State to;
    };

public:
    CStateMachine(const Transition* transitions, uint32_t count, T_State initialState)
        : mTransitions(transitions), mTransitionCount(count), mState(initialState) {}

    T_State getCurrentState()
    {
        return mState;
    }

    bool mayTransition(T_Trigger t)
    {
        for (uint32_t i = 0; i < mTransitionCount; ++i) {
            if (mTransitions[i].from == mState && mTransitions[i].trigger == t) {
                return true;
            }
        }
        return false; 
    }

    T_State getNextState(T_Trigger t) const {
        for (uint32_t i = 0; i < mTransitionCount; ++i) {
            if (mTransitions[i].from == mState && mTransitions[i].trigger == t) {
                return mTransitions[i].to;
            }
        }
        return mState;
    }

    bool transition(T_Trigger t)
    {
        for (uint32_t i = 0; i < mTransitionCount; ++i) {
            if (mTransitions[i].from == mState && mTransitions[i].trigger == t) {
                mState = mTransitions[i].to;
                return true;
            }
        }
        return false;
    }

private:
    const Transition* mTransitions;
    uint32_t mTransitionCount;
    T_State mState;
};