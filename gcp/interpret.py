from __future__ import division

in_language_code  = "ja-JP"
src_language      = "ja"
tgt_language      = "en"
out_language_code = "en-US"

import asyncio
import os
import re
import six
import sys
import time

from google.cloud import speech, texttospeech, translate_v2 as translate

import pyaudio
from six.moves import queue
from playsound import playsound

import datetime

# Audio recording parameters
RATE = 16000
CHUNK = int(RATE / 10)  # 100ms

class MicrophoneStream(object):
 def __init__(self, rate, chunk):
  self._rate = rate
  self._chunk = chunk
  self._buff = queue.Queue()
  self.closed = True

 def __enter__(self):
  self._audio_interface = pyaudio.PyAudio()
  self._audio_stream = self._audio_interface.open(format=pyaudio.paInt16, channels=1, rate=self._rate, input=True, frames_per_buffer=self._chunk, stream_callback=self._fill_buffer)
  self.closed = False
  return self

 def __exit__(self, type, value, traceback):
  self._audio_stream.stop_stream()
  self._audio_stream.close()
  self.closed = True
  self._buff.put(None)
  self._audio_interface.terminate()

 def _fill_buffer(self, in_data, frame_count, time_info, status_flags):
  self._buff.put(in_data)
  return None, pyaudio.paContinue

 def generator(self):
  while not self.closed:
   chunk = self._buff.get()
   if chunk is None:
    return
   data = [chunk]

   while True:
    try:
     chunk = self._buff.get(block=False)
     if chunk is None:
      return
     data.append(chunk)
    except queue.Empty:
     break

   yield b"".join(data)

def translate_and_speech(text, translate_client, speech_client, mp3_num, now_str):
 filename_prefix = os.path.splitext(os.path.basename(__file__))[0] + "_"
 translation_filename = filename_prefix + "translation_" + now_str + ".txt"
 mp3_filename         = filename_prefix + "out_" + now_str + "_" + str(mp3_num) + ".mp3"
 playstate_filename   = filename_prefix + "play_" + now_str + ".txt"

 if isinstance(text, six.binary_type):
  text = text.decode("utf-8")
 translation = translate_client.translate(text, source_language="ja", target_language="en", format_="text")

 with open(translation_filename, 'a') as f:
  print(str(mp3_num), file=f)
  print(text, file=f)
  print(translation["translatedText"], file=f)
 print(translation["translatedText"])

 synthesis_input = texttospeech.SynthesisInput(text=translation["translatedText"])
 voice = texttospeech.VoiceSelectionParams(language_code=out_language_code, ssml_gender=texttospeech.SsmlVoiceGender.NEUTRAL)
 audio_config = texttospeech.AudioConfig(audio_encoding=texttospeech.AudioEncoding.MP3)
 out_speech = speech_client.synthesize_speech(input=synthesis_input, voice=voice, audio_config=audio_config)

 with open(mp3_filename, "wb") as out:
  out.write(out_speech.audio_content)
 if os.path.isfile(playstate_filename):
  while True:
   f = open(playstate_filename, 'r')
   play_count = f.read()
   f.close()
   if int(play_count) == mp3_num:
    break
   time.sleep(1)
 with open(playstate_filename, "w") as out:
  out.write(str(mp3_num))
 playsound(mp3_filename)
 with open(playstate_filename, "w") as out:
  out.write(str(mp3_num + 1))

def interpret(responses, translate_client, speech_client, loop):
 now_str = datetime.datetime.now().strftime("%y%m%d-%H%M%S")
 mp3_num = 0
 for response in responses:
  if not response.results:
   continue
  result = response.results[0]
  if not result.alternatives:
   continue
  transcript = result.alternatives[0].transcript

  if result.is_final:
   print(transcript)
   loop.run_in_executor(None, translate_and_speech, transcript, translate_client, speech_client, mp3_num, now_str)
   mp3_num += 1

def main():
 client = speech.SpeechClient()
 config = speech.RecognitionConfig(encoding=speech.RecognitionConfig.AudioEncoding.LINEAR16, sample_rate_hertz=RATE, language_code=in_language_code)
 streaming_config = speech.StreamingRecognitionConfig(config=config, interim_results=True)
 translate_client = translate.Client()
 speech_client = texttospeech.TextToSpeechClient()
 loop = asyncio.get_event_loop()

 with MicrophoneStream(RATE, CHUNK) as stream:
  audio_generator = stream.generator()
  requests = (speech.StreamingRecognizeRequest(audio_content=content) for content in audio_generator)
  responses = client.streaming_recognize(streaming_config, requests)
  interpret(responses, translate_client, speech_client, loop)

if __name__ == "__main__":
 main()
