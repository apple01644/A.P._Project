
import tensorflow as tf
import numpy as np
import pandas as pd
import random
import matplotlib.pyplot as plt

from DQN import DQN
from Game import SBBGame

tf.app.flags.DEFINE_boolean("train", False,
                                "학습모드. 게임화면에 보여주지 않습니다.")

FLAGS = tf.app.flags.FLAGS

config = tf.ConfigProto()
config.gpu_options.allow_growth = True
tf.keras.backend.set_session(tf.Session(config=config))

MAX_EPISODE = 10000
TARGET_UPDATE_INTERVAL = 1000
TRAIN_INTERVAL = 4
OBSERVE = 100

NUM_ACTION = 129
SCREEN_WIDTH = 6
SCREEN_HEIGHT = 9

game = SBBGame()
game.user = False

def callback_initialize(self, state):
    self.dqn_state = state
    return

def shoot_degreed(self): # 0 ~ 128
    return random.random() * 128

def callback_result(self, data):
    self.dqn_data = data
    return

game.callback_shootdegreed = shoot_degreed
game.callback_init = callback_initialize
game.callback_result = callback_result

game.start()


def train():
    print("학습! 시작합니다!")
    sess = tf.Session()

    brain = DQN(sess, SCREEN_WIDTH, SCREEN_HEIGHT, NUM_ACTION)

    rewards = tf.placeholder(tf.float32, [None])
    tf.summary.scalar('avg.reward/ep', tf.reduce_mean(rewards))

    saver = tf.train.Saver()
    sess.run(tf.global_variables_initializer())

    writer = tf.summary.FileWriter('logs', sess.graph)
    summary_merged = tf.summary.merge_all()

    reward_mean = tf.reduce_mean(rewards)
    logs = open('logs/log.dat', "a")

    brain.update_target_network()

    epsilon = 1.0
    time_step = 0
    total_reward_list = []
    total_reward_mean = []
    ui_on = False

    for episode in range(MAX_EPISODE):
        terminal = False
        total_reward = 0

        state = game.dqn_state['num_map']
        loc = 50
        brain.init_state(state, loc)

        while not terminal:
            if random.random() < epsilon:
                action = random.randrange(NUM_ACTION)
            else:
                action = brain.get_action()

            if episode > OBSERVE:
                epsilon -= 1 / 1000

            game.setACT(action)
            while game.dqn_data == None: pass
            state, reward, terminal = game.dqn_data[0]['num_map'], game.dqn_data[1], game.dqn_data[2] != 0
            loc = game.dqn_data[0]['pos']['x']
            total_reward += reward

            game.dqn_data = None

            brain.remember(state, action, reward, terminal, loc)

            if time_step > OBSERVE and time_step % TRAIN_INTERVAL == 0:
                brain.train()

            if time_step % TARGET_UPDATE_INTERVAL == 0:
                brain.update_target_network()

            time_step += 1

        print('게임횟수 : {0}, 점수: {1}'.format(episode + 1, total_reward))

        total_reward_list.append(total_reward)
        
        if episode > 600 and (not ui_on):
            game.user = True
            ui_on = True
        if episode % 10 == 0:
            plt.clf()
            summary = sess.run(summary_merged,
                                feed_dict={rewards: total_reward_list})
            rm = sess.run(reward_mean,
                                feed_dict={rewards: total_reward_list})
            total_reward_mean.append(rm)
            plt.plot(total_reward_mean)
            plt.draw()
            writer.add_summary(summary, time_step)
            total_reward_list = []

            logs.write(str(rm)+"/")

        if episode % 100 == 0:
            saver.save(sess, 'model/dqn.ckpt', global_step=time_step)

def replay():
    print('시작')
    sess = tf.Session()

    game = Game()
    brain = DQN(sess, SCREEN_WIDTH, SCREEN_HEIGHT, NUM_ACTION)

    saver = tf.train.Saver()
    ckpt = tf.train.get_checkpoint_state('model')
    saver.restore(sess, ckpt.model_checkpoint_path)

    for episode in range(MAX_EPISODE):
        terminal = False
        total_reward = 0

        state = game.reset()
        brain.init_state(state)


        while not terminal:
            action = brain.get_action()

            state, reward, terminal = game.action(action)
            total_reward += reward

            brain.remember(state, action, reward, terminal)

        print('게임횟수 : {0}, 점수: {1}'.format(episode + 1, total_reward))


def main():
    train()

if __name__ == '__main__':
    train()
