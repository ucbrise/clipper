# Reference: https://github.com/Steven-Hewitt/QA-with-Tensorflow
import tensorflow as tf
import numpy as np
import urllib
import sys
import os

current_path = '/container/'
glove_vectors_file = "glove.6B.50d.txt"
snli_dev_file = "snli_1.0_dev.txt"

glove_wordmap = {}
with open(current_path + glove_vectors_file, "r", encoding='utf-8') as glove:
    for line in glove:
        name, vector = tuple(line.split(" ", 1))
        glove_wordmap[name] = np.fromstring(vector, sep=" ")

def sentence2sequence(sentence):
    tokens = sentence.lower().split(" ")
    rows = []
    words = []
    # Greedy search for tokens
    for token in tokens:
        i = len(token)
        while len(token) > 0 and i > 0:
            word = token[:i]
            if word in glove_wordmap:
                rows.append(glove_wordmap[word])
                words.append(word)
                token = token[i:]
                i = len(token)
            else:
                i = i-1
    return rows, words

# Constants setup
rnn_size = 64
rnn = tf.contrib.rnn.BasicRNNCell(rnn_size)
max_hypothesis_length, max_evidence_length = 30, 30
batch_size, vector_size, hidden_size = 128, 50, 64
lstm_size = hidden_size
weight_decay = 0.0001
learning_rate = 1
input_p, output_p = 0.5, 0.5
training_iterations_count = 1000
display_step = 10

def score_setup(row):
    convert_dict = {
        'entailment': 0,
        'neutral': 1,
        'contradiction': 2
    }
    score = np.zeros((3,))
    for x in range(1, 6):
        tag = row["label"+str(x)]
        if tag in convert_dict:
            score[convert_dict[tag]] += 1
    return score / (1.0*np.sum(score))

def fit_to_size(matrix, shape):
    res = np.zeros(shape)
    slices = [slice(0, min(dim, shape[e]))
              for e, dim in enumerate(matrix.shape)]
    res[slices] = matrix[slices]
    return res

def split_data_into_scores():
    import csv
    with open(current_path + snli_dev_file, "r") as data:
        train = csv.DictReader(data, delimiter='\t')
        evi_sentences = []
        hyp_sentences = []
        labels = []
        scores = []
        for row in train:
            hyp_sentences.append(np.vstack(
                sentence2sequence(row["sentence1"].lower())[0]))
            evi_sentences.append(np.vstack(
                sentence2sequence(row["sentence2"].lower())[0]))
            labels.append(row["gold_label"])
            scores.append(score_setup(row))

        hyp_sentences = np.stack([fit_to_size(x, (max_hypothesis_length, vector_size))
                                  for x in hyp_sentences])
        evi_sentences = np.stack([fit_to_size(x, (max_evidence_length, vector_size))
                                  for x in evi_sentences])

        return (hyp_sentences, evi_sentences), labels, np.array(scores)

def answerQuery( evidences , hypotheses):
    data_feature_list, correct_values, correct_scores = split_data_into_scores()

    l_h, l_e = max_hypothesis_length, max_evidence_length
    N, D, H = batch_size, vector_size, hidden_size
    l_seq = l_h + l_e

    tf.reset_default_graph()
    lstm = tf.contrib.rnn.BasicLSTMCell(lstm_size)
    lstm_drop = tf.contrib.rnn.DropoutWrapper(lstm, input_p, output_p)

    # N: The number of elements in each of our batches,
    #   which we use to train subsets of data for efficiency's sake.
    # l_h: The maximum length of a hypothesis, or the second sentence.  This is
    #   used because training an RNN is extraordinarily difficult without
    #   rolling it out to a fixed length.
    # l_e: The maximum length of evidence, the first sentence.  This is used
    #   because training an RNN is extraordinarily difficult without
    #   rolling it out to a fixed length.
    # D: The size of our used GloVe or other vectors.
    hyp = tf.placeholder(tf.float32, [N, l_h, D], 'hypothesis')
    evi = tf.placeholder(tf.float32, [N, l_e, D], 'evidence')
    y = tf.placeholder(tf.float32, [N, 3], 'label')
    # hyp: Where the hypotheses will be stored during training.
    # evi: Where the evidences will be stored during training.
    # y: Where correct scores will be stored during training.

    # lstm_size: the size of the gates in the LSTM,
    #    as in the first LSTM layer's initialization.
    lstm_back = tf.contrib.rnn.BasicLSTMCell(lstm_size)
    # lstm_back:  The LSTM used for looking backwards
    #   through the sentences, similar to lstm.

    # input_p: the probability that inputs to the LSTM will be retained at each
    #   iteration of dropout.
    # output_p: the probability that outputs from the LSTM will be retained at
    #   each iteration of dropout.
    lstm_drop_back = tf.contrib.rnn.DropoutWrapper(lstm_back, input_p, output_p)
    # lstm_drop_back:  A dropout wrapper for lstm_back, like lstm_drop.

    fc_initializer = tf.random_normal_initializer(stddev=0.1)
    # fc_initializer: initial values for the fully connected layer's weights.
    # hidden_size: the size of the outputs from each lstm layer.
    #   Multiplied by 2 to account for the two LSTMs.
    fc_weight = tf.get_variable('fc_weight', [2*hidden_size, 3], initializer=fc_initializer)
    # fc_weight: Storage for the fully connected layer's weights.
    fc_bias = tf.get_variable('bias', [3])
    # fc_bias: Storage for the fully connected layer's bias.

    # tf.GraphKeys.REGULARIZATION_LOSSES:  A key to a collection in the graph
    #   designated for losses due to regularization.
    #   In this case, this portion of loss is regularization on the weights
    #   for the fully connected layer.
    tf.add_to_collection(tf.GraphKeys.REGULARIZATION_LOSSES, tf.nn.l2_loss(fc_weight))

    x = tf.concat([hyp, evi], 1)  # N, (Lh+Le), d
    # Permuting batch_size and n_steps
    x = tf.transpose(x, [1, 0, 2])  # (Le+Lh), N, d
    # Reshaping to (n_steps*batch_size, n_input)
    x = tf.reshape(x, [-1, vector_size])  # (Le+Lh)*N, d
    # Split to get a list of 'n_steps' tensors of shape (batch_size, n_input)
    x = tf.split(x, l_seq,)

    # tf.contrib.rnn.static_bidirectional_rnn: Runs the input through
    #   two recurrent networks, one that runs the inputs forward and one
    #   that runs the inputs in reversed order, combining the outputs.
    rnn_outputs, _, _ = tf.contrib.rnn.static_bidirectional_rnn(lstm, lstm_back,
                                                                x, dtype=tf.float32)
    # rnn_outputs: the list of LSTM outputs, as a list.
    #   What we want is the latest output, rnn_outputs[-1]

    classification_scores = tf.matmul(rnn_outputs[-1], fc_weight) + fc_bias
    # The scores are relative certainties for how likely the output matches a certain entailment:
    #     0: Positive entailment
    #     1: Neutral entailment
    #     2: Negative entailment

    with tf.variable_scope('Accuracy'):
        predicts = tf.cast(tf.argmax(classification_scores, 1), 'int32')
        y_label = tf.cast(tf.argmax(y, 1), 'int32')
        corrects = tf.equal(predicts, y_label)
        num_corrects = tf.reduce_sum(tf.cast(corrects, tf.float32))
        accuracy = tf.reduce_mean(tf.cast(corrects, tf.float32))

    with tf.variable_scope("loss"):
        cross_entropy = tf.nn.softmax_cross_entropy_with_logits(
            logits=classification_scores, labels=y)
        loss = tf.reduce_mean(cross_entropy)
        total_loss = loss + weight_decay * tf.add_n(
            tf.get_collection(tf.GraphKeys.REGULARIZATION_LOSSES))

    optimizer = tf.train.GradientDescentOptimizer(learning_rate)
    opt_op = optimizer.minimize(total_loss)

    # Initialize variables
    init = tf.global_variables_initializer()

    # Use TQDM if installed
    tqdm_installed = False
    try:
        from tqdm import tqdm
        tqdm_installed = True
    except:
        pass

    with tf.Session() as sess:
        # Restore pre-trained model
        saver = tf.train.Saver()
        saver.restore(sess, tf.train.latest_checkpoint(current_path + 'models/'))

        # the two inputs to this method will be string, so we need to first put the input into array
        evidences = [evidences]
        hypotheses = [hypotheses]

        sentence1 = [fit_to_size(np.vstack(sentence2sequence(
            evidence)[0]), (30, 50)) for evidence in evidences]
        sentence2 = [fit_to_size(np.vstack(sentence2sequence(hypothesis)[
                                0]), (30, 50)) for hypothesis in hypotheses]
        prediction = sess.run(classification_scores, feed_dict={hyp: (
            sentence1 * N), evi: (sentence2 * N), y: [[0, 0, 0]]*N})
        
        entailmentNature = ["Positive", "Neutral", "Negative"][np.argmax(prediction[0])]
        rightOrWrong =  ["correct", "neutral", "wrong"][np.argmax(prediction[0])]
        answer = entailmentNature + " entailment: the hypothesis '" + hypotheses[0] + "' is " + rightOrWrong + "."
        return answer


def predict(evidenceString, hypothesisString):
    return answerQuery(evidenceString, hypothesisString)
