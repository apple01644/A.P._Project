from Game import SBBGame
from Model import DDQN_Model
import random, time

now_state = {}
act = 0.5

epsilon = 1
game_attempt = 0

EPSILON_DECREASE_PER_GAME = 1e-4
EPOCH = 100000

brain = DDQN_Model()

game = SBBGame()
#game.ui = False
game.user = False
game.start()

for _ in range(EPOCH):
    while game.dqn_state == None: time.sleep(0.01)
    state = game.dqn_state
    game.dqn_state = None

    #set action from state
    game.guide_line = act
    if epsilon > random.random():
        game.setACT(random.random())
    else:
        game.setACT(act)
        
    while game.dqn_data == None: time.sleep(0.01)
    result = game.dqn_data
    if result[2] == 1:
        game_attempt += 1
        pass #died
    
    game.dqn_data = None

