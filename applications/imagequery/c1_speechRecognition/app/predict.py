# from __future__ import absolute_import, division, print_function

# import argparse
# import numpy as np
# import shlex
# import subprocess
# import sys
# import wave

# from deepspeech import Model, printVersions
# from timeit import default_timer as timer

# try:
#     from shhlex import quote
# except ImportError:
#     from pipes import quote


# # These constants control the beam search decoder

# # Beam width used in the CTC decoder when building candidate transcriptions
# BEAM_WIDTH = 500

# # The alpha hyperparameter of the CTC decoder. Language Model weight
# LM_ALPHA = 0.75

# # The beta hyperparameter of the CTC decoder. Word insertion bonus.
# LM_BETA = 1.85


# # These constants are tied to the shape of the graph used (changing them changes
# # the geometry of the first layer), so make sure you use the same constants that
# # were used during training

# # Number of MFCC features to use
# N_FEATURES = 26

# # Size of the context window used for producing timesteps in the input vector
# N_CONTEXT = 9


# def convert_samplerate(audio_path):
#     sox_cmd = 'sox {} --type raw --bits 16 --channels 1 --rate 16000 --encoding signed-integer --endian little --compression 0.0 --no-dither - '.format(quote(audio_path))
#     try:
#         output = subprocess.check_output(shlex.split(sox_cmd), stderr=subprocess.PIPE)
#     except subprocess.CalledProcessError as e:
#         raise RuntimeError('SoX returned non-zero status: {}'.format(e.stderr))
#     except OSError as e:
#         raise OSError(e.errno, 'SoX not found, use 16kHz files or install it: {}'.format(e.strerror))

#     return 16000, np.frombuffer(output, np.int16)


# def metadata_to_string(metadata):
#     return ''.join(item.character for item in metadata.items)


# class VersionAction(argparse.Action):
#     def __init__(self, *args, **kwargs):
#         super(VersionAction, self).__init__(nargs=0, *args, **kwargs)

#     def __call__(self, *args, **kwargs):
#         printVersions()
#         exit(0)


# def main():
#     parser = argparse.ArgumentParser(description='Running DeepSpeech inference.')
#     parser.add_argument('--model', required=True, help='Path to the model (protocol buffer binary file)')
#     parser.add_argument('--alphabet', required=True, help='Path to the configuration file specifying the alphabet used by the network')
#     parser.add_argument('--lm', nargs='?', help='Path to the language model binary file') # can be deleted
#     parser.add_argument('--trie', nargs='?', help='Path to the language model trie file created with native_client/generate_trie') # can be deleted
#     parser.add_argument('--audio', required=True, help='Path to the audio file to run (WAV format)')
#     parser.add_argument('--version', action=VersionAction, help='Print version and exits') # can be deleted
#     parser.add_argument('--extended', required=False, action='store_true', help='Output string from extended metadata') # can be deleted
#     args = parser.parse_args()

#     print('Loading model from file {}'.format(args.model), file=sys.stderr)
#     model_load_start = timer()
#     ds = Model(args.model, N_FEATURES, N_CONTEXT, args.alphabet, BEAM_WIDTH) # can be CHANGED
#     model_load_end = timer() - model_load_start
#     print('Loaded model in {:.3}s.'.format(model_load_end), file=sys.stderr)

#     # can be deleted
#     if args.lm and args.trie:
#         print('Loading language model from files {} {}'.format(args.lm, args.trie), file=sys.stderr)
#         lm_load_start = timer()
#         ds.enableDecoderWithLM(args.alphabet, args.lm, args.trie, LM_ALPHA, LM_BETA)
#         lm_load_end = timer() - lm_load_start
#         print('Loaded language model in {:.3}s.'.format(lm_load_end), file=sys.stderr)

#     fin = wave.open(args.audio, 'rb')
#     fs = fin.getframerate()
#     if fs != 16000:
#         print('Warning: original sample rate ({}) is different than 16kHz. Resampling might produce erratic speech recognition.'.format(fs), file=sys.stderr)
#         fs, audio = convert_samplerate(args.audio)
#     else:
#         audio = np.frombuffer(fin.readframes(fin.getnframes()), np.int16)

#     audio_length = fin.getnframes() * (1/16000)
#     fin.close()

#     print('Running inference.', file=sys.stderr)
#     inference_start = timer()
#     if args.extended:
#         print(metadata_to_string(ds.sttWithMetadata(audio, fs)))
#     else:
#         print(ds.stt(audio, fs)) # stt means Speech To Text
#     inference_end = timer() - inference_start
#     print('Inference took %0.3fs for %0.3fs audio file.' % (inference_end, audio_length), file=sys.stderr)

# if __name__ == '__main__':
#     main()

##########################################
#### preloading version of predict.py ####
##########################################
from __future__ import absolute_import, division, print_function

import numpy as np
import shlex
import subprocess
import sys
import wave

from deepspeech import Model, printVersions
from timeit import default_timer as timer

try:
    from shhlex import quote
except ImportError:
    from pipes import quote


# These constants control the beam search decoder

# Beam width used in the CTC decoder when building candidate transcriptions
BEAM_WIDTH = 500

# The alpha hyperparameter of the CTC decoder. Language Model weight
LM_ALPHA = 0.75

# The beta hyperparameter of the CTC decoder. Word insertion bonus.
LM_BETA = 1.85


# These constants are tied to the shape of the graph used (changing them changes
# the geometry of the first layer), so make sure you use the same constants that
# were used during training

# Number of MFCC features to use
N_FEATURES = 26

# Size of the context window used for producing timesteps in the input vector
N_CONTEXT = 9


def convert_samplerate(audio_path):
    sox_cmd = 'sox {} --type raw --bits 16 --channels 1 --rate 16000 --encoding signed-integer --endian little --compression 0.0 --no-dither - '.format(quote(audio_path))
    try:
        output = subprocess.check_output(shlex.split(sox_cmd))
    except subprocess.CalledProcessError as e:
        raise RuntimeError('SoX returned non-zero status: {}'.format(e.stderr))
    except OSError as e:
        raise OSError(e.errno, 'SoX not found, use 16kHz files or install it: {}'.format(e.strerror))

    return 16000, np.frombuffer(output, np.int16)


model_path = "/container/models/output_graph.pbmm"
alphabet_path = "/container/models/alphabet.txt"
print('Loading model from ' + model_path)
ds = Model(model_path, N_FEATURES, N_CONTEXT, alphabet_path, BEAM_WIDTH)
print("Model successfully loaded!")


def predict(audio_file_index):

    audio_file_index = int(audio_file_index)
    if audio_file_index < 0 or audio_file_index > 1000:
        return "Invalid image index! Only index between 1 to 1000 is allowed! Exiting..."

    dataset_index = 3

    if dataset_index == 1:
        # dataset1: CMU arctic
        audio_file_path = "/container/data/dataset1/cmu_us_awb_arctic/wav/" + str(audio_file_index) + ".wav"
    elif dataset_index == 2:
        # dataset2: Flicker: different scripts but with overlapping
        audio_file_path = "/container/data/dataset2/flickr_audio/wavs/" + str(audio_file_index) + ".wav"
    elif dataset_index == 3:
        # dataset3: speech-accent-archive: different people reading the same script
        audio_file_path = "/container/data/dataset3/recordings/" + str(audio_file_index) + ".wav"
    else:
        return "Invalid dataset index!"

    print(audio_file_path)


    # reading audio file
    fin = wave.open(audio_file_path, 'rb')
    fs = fin.getframerate() # fs: sampling frequency
    if fs != 16000:
        print('Warning: original sample rate ({}) is different than 16kHz. Resampling might produce erratic speech recognition.'.format(fs), file=sys.stderr)
        fs, audio = convert_samplerate(audio_file_path)
    else:
        audio = np.frombuffer(fin.readframes(fin.getnframes()), np.int16)
    audio_length = fin.getnframes() * (1/16000)
    fin.close()

    # run inference
    print('Running inference.')
    inference_start = timer()
    print(ds.stt(audio, fs)) # stt means Speech To Text
    inference_end = timer() - inference_start
    print('Inference took %0.3fs for %0.3fs audio file.' % (inference_end, audio_length))  

# if __name__ == "__main__":
#     predict(1)
#     predict(2)
#     predict(3)

""" 
Usage: 
python3 predict.py --model /container/models/output_graph.pbmm --alphabet /container/models/alphabet.txt --lm /container/models/lm.binary --trie /container/models/trie --audio /container/data/dataset1/cmu_us_awb_arctic/wav/0001.wav

cmu_us_awb_arctic/wav/" + str(audio_file_index) + ".wav"
"""