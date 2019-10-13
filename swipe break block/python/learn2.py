import numpy as np
import tensorflow as tf
import pandas as pd
import os.path
import random

EPOCH = 100000
BATCH_SIZE = 500

dataset = pd.read_csv('1.csv', sep=',', header=None).iloc

#dataX = np.array(dataset[:,1:1 + 3 + 108 - 1], dtype=np.float32)#(110,)
#dataA = np.array(dataset[:,111:111 + 130 - 1], dtype=np.float32)#(129,)
#dataY = np.array(dataset[:,0], dtype=np.float32) #()

dataset = tf.data.Dataset.from_tensor_slices(dataset[:,:111 + 130 - 1])
dataset = dataset.shuffle(100000).repeat().padded_batch(BATCH_SIZE, padded_shapes=[None])

itr = dataset.make_one_shot_iterator()

sess = tf.Session()

X = tf.placeholder(tf.float32, shape=[None, 239])
Y = tf.placeholder(tf.float32, shape=[None, 1])

global_step = tf.Variable(0, trainable=False, name='global_step')

rng = np.sqrt(6.0 / (239 + 256))
W0 = tf.Variable(tf.random_uniform([239, 256], -rng, rng))
rng = np.sqrt(3.0 / (239 + 256))
B = tf.Variable(tf.random_uniform([256], -rng, rng))


rng = np.sqrt(6.0 / (256 + 256))
W1 = tf.Variable(tf.random_uniform([256, 256], -rng, rng))
W2 = tf.Variable(tf.random_uniform([256, 256], -rng, rng))
W3 = tf.Variable(tf.random_uniform([256, 256], -rng, rng))
W4 = tf.Variable(tf.random_uniform([256, 256], -rng, rng))
W5 = tf.Variable(tf.random_uniform([256, 256], -rng, rng))
W6 = tf.Variable(tf.random_uniform([256, 256], -rng, rng))
W7 = tf.Variable(tf.random_uniform([256, 256], -rng, rng))
W8 = tf.Variable(tf.random_uniform([256, 256], -rng, rng))
rng = np.sqrt(6.0 / (256 + 1))
W9 = tf.Variable(tf.random_uniform([256, 1], -rng, rng))

L0 = tf.add(tf.matmul(X, W0), B)
L1 = tf.matmul(L0, W1)
L2 = tf.matmul(L1, W2)
L3 = tf.matmul(L2, W3)
L4 = tf.matmul(L3, W4)
L5 = tf.matmul(L4, W5)
L6 = tf.matmul(L5, W6)
L7 = tf.matmul(L6, W7)
L8 = tf.matmul(L7, W8)
model = tf.matmul(L8, W9)

cost = tf.reduce_mean(tf.square(model - Y))

opt = tf.train.AdamOptimizer(0.0001)
train_op = opt.minimize(cost)

saver = tf.train.Saver(tf.global_variables())
merged = tf.summary.merge_all()
writer = tf.summary.FileWriter('/log', sess.graph)

sess.run(tf.global_variables_initializer())

count = 0
for _ in range(EPOCH):
    
    datas = sess.run(itr.get_next())

    loss, what = sess.run([cost, train_op], feed_dict = {X : datas[:, 1:240], Y : datas[:, 0:1]})

    #writer.add_summary(summary, global_step=global_step)
    print('Take #%d : %12.6f' % (count, loss))

    if _ % 100 == 0:
        prec = tf.argmax(model, 1)
        target = tf.argmax(Y, 1)
    
        acc = tf.reduce_mean(tf.cast(tf.equal(target, prec), tf.float32))
        print('Accuract : %12.6f' % sess.run(acc * 100, feed_dict = {X : datas[:, 1:240], Y : datas[:, 0:1]}))
    
    count += 1
        
print('done')
saver.save(sess, 'dnn.ckpt', global_step=global_step)
