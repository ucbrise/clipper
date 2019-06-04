from __future__ import print_function
import os
from pocketsphinx import Pocketsphinx, get_model_path, get_data_path

model_path = get_model_path()
data_path = get_data_path()

config = {
    'hmm': os.path.join(model_path, 'en-us'),
    'lm': os.path.join(model_path, 'en-us.lm.bin'),
    'dict': os.path.join(model_path, 'cmudict-en-us.dict')
}

ps = Pocketsphinx(**config)
ps.decode(
    # add your audio file here
    audio_file=os.path.join(data_path, '/container/c1_speechRecognition/data/dataset3/recordings/1.wav'),
    buffer_size=2048,
    no_search=False,
    full_utt=False
)

print(ps.hypothesis())
