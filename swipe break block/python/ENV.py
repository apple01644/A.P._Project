from Game import SBBGame
import random


game = SBBGame()
game.ui = False
game.user = False

now_state = {}
act = -1

epsilon = 1

EPSILON_DECREASE_FACTOR = 1e-4

def callback_initialize(self, state):
    now_state = state
    return

def shoot_degreed(self): # 0 ~ 128
    if epsilon > random.random():
        return random.random() * 128
    else:
        return act

def callback_result(self, data):
    if epsilon > 0:
        epsilon -= EPSILON_DECREASE_FACTOR
    elif epsilon == 0:
        game.ui = True
        epsilon = -1

game.callback_shootdegreed = shoot_degreed
game.callback_init = callback_initialize
game.callback_result = callback_result


game.start()
