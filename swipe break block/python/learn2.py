import numpy as np
import tensorflow as tf
import pandas as pd
import os.path
import random

dataset = pd.read_csv('1.csv', sep=',', header=None).iloc

#dataX = np.array(dataset[:,1:1 + 3 + 108 - 1], dtype=np.float32)#(110,)
#dataA = np.array(dataset[:,111:111 + 130 - 1], dtype=np.float32)#(129,)
#dataY = np.array(dataset[:,0], dtype=np.float32) #()

#print(dataset[0][0]) #<점수>
#print(dataset[0][1:4]) #<공의 갯수>, <발사 위치.x>, <발사 위치.y>

#print(dataset[0][4:58]) #<블럭의 갯수>
#print(dataset[0][58:112]) #<초록 공인지 여부>

#print(dataset[0][112:241]) #<발사 각도>

S = tf.placeholder(tf.float32, [None, 1], 'S')
I = tf.placeholder(tf.float32, [None, 3], 'I')
A = tf.placeholder(tf.float32, [None, 129], 'A')
NM = tf.placeholder(tf.float32, [None, 6, 9, 1], 'NM')
TM = tf.placeholder(tf.float32, [None, 6, 9, 1], 'TM')
is_training = tf.placeholder(tf.bool)


sess = tf.Session()
sess.run(tf.global_variables_initializer())


dataset_size = len(dataset[:][0])

BATCH_SIZE = 3
EPOCH = 100

foo = tf.add(S, S)

#model
L1 = tf.layers.conv2d(NM, 32, [3, 3], activation=tf.nn.relu)
L1 = tf.layers.max_pooling2d(L1, [2, 2], [2, 2])
L1 = tf.layers.dropout(L1, 0.7, is_training)

L2 = tf.contrib.layers.flatten(L1)
L2 = tf.layers.dense(L2, 256, activation=tf.nn.relu)
L2 = tf.layers.dropout(L2, 0.5, is_training)

End_NM = tf.layers.dense(L2, 256, activation=None)


L1 = tf.layers.conv2d(TM, 32, [3, 3], activation=tf.nn.relu)
L1 = tf.layers.max_pooling2d(L1, [2, 2], [2, 2])
L1 = tf.layers.dropout(L1, 0.7, is_training)

L2 = tf.contrib.layers.flatten(L1)
L2 = tf.layers.dense(L2, 256, activation=tf.nn.relu)
L2 = tf.layers.dropout(L2, 0.5, is_training)

End_TM = tf.layers.dense(L2, 256, activation=None)


c1 = tf.constant([[1, 3, 5]])

c2 = tf.constant([[1, 3, 7]])

merge = tf.add(tf.add(End_NM, End_TM), tf.add(tf.layers.dense(I, 256, activation=None), tf.layers.dense(A, 256, activation=None)))
merge = tf.layers.dense(merge, 256, activation=None)
merge = tf.layers.dense(merge, 256, activation=None)
model = tf.layers.dense(merge, 1, activation=None)

cost = tf.square(tf.reduce_mean(model - S))
opt = tf.train.AdamOptimizer(0.001).minimize(cost)

for _ in range(EPOCH):
    indexs = []
    for __ in range(BATCH_SIZE):
        indexs.append(random.randint(0, dataset_size))

    score = np.array([dataset[index][0] for index in indexs], dtype=np.float32).reshape(BATCH_SIZE, 1)
    
    info = np.array([dataset[index][1:4] for index in indexs], dtype=np.float32).reshape(BATCH_SIZE, 3)
    
    act = np.array([dataset[index][112:241] for index in indexs], dtype=np.float32).reshape(BATCH_SIZE, 129)
    
    num_map = np.array([[
        dataset[index][ 4:10],
        dataset[index][10:16],
        dataset[index][16:22],
        dataset[index][22:28],
        dataset[index][28:34],
        dataset[index][34:40],
        dataset[index][40:46],
        dataset[index][46:52],
        dataset[index][52:58]]  for index in indexs], dtype=np.float32).reshape(BATCH_SIZE, 6, 9, 1)

    type_map = np.array([[
        dataset[index][ 58: 64],
        dataset[index][ 64: 70],
        dataset[index][ 70: 76],
        dataset[index][ 76: 82],
        dataset[index][ 82: 88],
        dataset[index][ 88: 94],
        dataset[index][ 94:100],
        dataset[index][100:106],
        dataset[index][106:112]] for index in indexs], dtype=np.float32).reshape(BATCH_SIZE, 6, 9, 1)

    action = np.array([dataset[index][112:241] for index in indexs], dtype=np.float32)

    
    sess.run(End_TM, feed_dict={S : score, I : info, A : act, NM : num_map, TM : type_map, is_training : True})
    
    print(_)
