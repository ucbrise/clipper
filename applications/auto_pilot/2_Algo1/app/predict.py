import tensorflow as tf
import model

sess = tf.InteractiveSession()
saver = tf.train.Saver()
saver.restore(sess, "/container/model.ckpt")

def predict(image):
    degrees = model.y.eval(feed_dict={model.x: [image], model.keep_prob: 1.0})[0][0] * 180.0 / scipy.pi
    print(degrees)
    print("HELLO")
    return str(degrees)
