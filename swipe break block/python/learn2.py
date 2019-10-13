import numpy as np
import tensorflow as tf
import pandas as pd
import os.path
import random

dataset = pd.read_csv('1.csv', sep=',', header=None).iloc

dataset_size = len(dataset[0])

S = tf.placeholder(tf.float32, [None, 1], 'S') # 보상
I = tf.placeholder(tf.float32, [None, 2], 'I') # 사전정보(발사 위치x, 발사 위치y)
A = tf.placeholder(tf.float32, [None, 129], 'A') # 행동(발사 각도)
NM = tf.placeholder(tf.float32, [None, 6, 9, 1], 'NM') # Number Map
TM = tf.placeholder(tf.float32, [None, 6, 9, 1], 'TM') # Type Map

BATCH_SIZE = 1
EPOCH = 100


for _ in range(EPOCH):
    indexs = []
    for __ in range(BATCH_SIZE):
        indexs.append(random.randint(0, dataset_size))

    score = np.array([dataset[index][0] for index in indexs], dtype=np.float32).reshape(BATCH_SIZE, 1)
    
    info = np.array([dataset[index][1:3] for index in indexs], dtype=np.float32).reshape(BATCH_SIZE, 2)
    
    act = np.array([dataset[index][111:240] for index in indexs], dtype=np.float32).reshape(BATCH_SIZE, 129)
    
    num_map = np.array([[
        dataset[index][ 3: 9],
        dataset[index][ 9:15],
        dataset[index][15:21],
        dataset[index][21:27],
        dataset[index][27:33],
        dataset[index][33:39],
        dataset[index][39:45],
        dataset[index][45:51],
        dataset[index][51:57]]  for index in indexs], dtype=np.float32).reshape(BATCH_SIZE, 6, 9, 1)

    type_map = np.array([[
        dataset[index][ 57: 63],
        dataset[index][ 63: 69],
        dataset[index][ 69: 75],
        dataset[index][ 75: 81],
        dataset[index][ 81: 87],
        dataset[index][ 87: 93],
        dataset[index][ 93: 99],
        dataset[index][ 99:105],
        dataset[index][105:111]] for index in indexs], dtype=np.float32).reshape(BATCH_SIZE, 6, 9, 1)

    print(score)
    input('that is score')
    
    print(info)
    input('that is info')
    
    print(act)
    input('that is act')
    
    print(num_map)
    input('that is num_map')
    
    print(type_map)
    input('that is type_map')
