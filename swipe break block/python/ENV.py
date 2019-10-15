from Game import SBBGame
import random, time

now_state = {}
act = -1

epsilon = 1
time_step = 0

EPSILON_DECREASE_PER_GAME = 1e-4



game = SBBGame()
#game.ui = False
game.user = False
game.start()

for _ in range(100000):
    while game.dqn_state == None: time.sleep(0.01)
    state = game.dqn_state
    
    game.dqn_state = None
    
    if epsilon > random.random():
        game.setACT(random.random())
    else:
        game.setACT(0.5)
        
    while game.dqn_data == None: time.sleep(0.01)
    result = game.dqn_data
    
    game.dqn_data = None

