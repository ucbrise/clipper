import tensorflow as tf
# This file is necessary, because the code in model is in earlier version of tensorflow 
# but normaly the tensorflow on your device is of a higher version. Thus, this file is 
# needed to convert the depreciated features in original model to updated features. 
# The renaming is already done for tensorflow 1.13, the renamed model is newmodel.ckt-2000000.

def rename_ckpt():
	# 由于 TensorFlow 的版本不同，所以要根据具体错误信息进行修改
    # vars_to_rename might depend on the specific version you are using
    vars_to_rename = {
        "lstm/BasicLSTMCell/Linear/Bias": "lstm/basic_lstm_cell/bias",
        "lstm/BasicLSTMCell/Linear/Matrix": "lstm/basic_lstm_cell/kernel"
    }
    new_checkpoint_vars = {}
    
    reader = tf.train.NewCheckpointReader(
        "C:/Users/musicman/Desktop/official/im2txt/model/model.ckpt-2000000" # the filepath to the old model 
    )
    for old_name in reader.get_variable_to_shape_map():
        if old_name in vars_to_rename:
            new_name = vars_to_rename[old_name]
        else:
            new_name = old_name
        new_checkpoint_vars[new_name] = tf.Variable(
            reader.get_tensor(old_name))

    init = tf.global_variables_initializer()
    saver = tf.train.Saver(new_checkpoint_vars)

    with tf.Session() as sess:
        sess.run(init)
        saver.save(
            sess,
            "C:/Users/musicman/Desktop/official/im2txt/model/newmodel.ckpt-2000000" # the filepath to the renamed model
        )
    print("checkpoint file rename successful... ")


if __name__ == '__main__':
    rename_ckpt()