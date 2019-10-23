import tensorflow as tf
from tensorflow import keras


class DDQN_Model:
    def __init__(self):
        model = keras.Sequential()
        model.add(keras.layers.Dense(256, input_shape=(1 + 54 + 54 + 128,)))
        model.add(keras.layers.Dense(256, activation='relu'))
        model.add(keras.layers.Dense(256, activation='relu'))
        model.add(keras.layers.Dense(256, activation='relu'))
        model.add(keras.layers.Dense(1, activation='softmax'))

        model.compile(loss=tf.losses.categorical_crossentropy, optimizer=keras.optimizers.Adadelta())

        model.fit([[0 for i in range(237)]], [[1]], epochs=1, verbose=0)

    def 

if __name__ == '__main__':
    DDQN_Model()
    print('Done')
