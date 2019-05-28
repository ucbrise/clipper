# Reference: https://github.com/jiaaro/pydub

import os
os.chdir("/container/data/dataset3/recordings")

import glob

wav_files = []
for file in glob.glob("./*.wav"):
    wav_files.append(file)

# print(wav_files)

# print("audio_files successfully generated!")

from pydub import AudioSegment

# print("pydub successfully imported!")

for wav_file in wav_files:
    original = AudioSegment.from_wav(wav_file)
    start_time = 0
    end_time = 5 * 1000
    truncated = original[start_time:end_time]
    truncated.export(wav_file, format="wav")
  
print("audio files successfully truncated!")