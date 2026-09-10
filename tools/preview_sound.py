#!/usr/bin/env python3
"""Audition exported firmware synthesis followed by a real cached prediction.
The firmware synthesizes effects live; this file is only a desktop listening aid.
"""
from pathlib import Path
import array
import wave
ROOT=Path(__file__).resolve().parents[1]

def read(path):
    with wave.open(str(path),'rb') as f:
        assert (f.getnchannels(),f.getsampwidth(),f.getframerate())==(1,2,16000)
        return array.array('h',f.readframes(f.getnframes()))

def main():
    directory=ROOT/'docs/sounds'
    master=32*255**4/2**36  # M5Unified mono / magnification=16.
    gain=(160/255)**2*master
    def voice(sample):
        level=abs(sample)*2
        if level>6000:level=6000+(level-6000)*7500//(level-6000+7500)
        return int((-level if sample<0 else level)*master)
    mix=array.array('h')
    for name in ('ritual','reveal'):
        mix.extend(int(v*gain) for v in read(directory/(name+'.wav')))
    mix.extend([0]*1120)  # NarrationGate's 70ms quiet interval.
    voice_offset=len(mix)
    mix.extend(voice(sample) for sample in read(ROOT/'sdcard/ORACLE/audio/coat.wav'))
    # One navigation click during speech demonstrates the separate effect channel.
    tick=read(directory/'scroll.wav')
    for i,v in enumerate(tick):
        index=voice_offset+32000+i
        if index<len(mix): mix[index]+=int(v*gain)
    path=directory/'demo-with-voice.wav'
    with wave.open(str(path),'wb') as f:
        f.setnchannels(1);f.setsampwidth(2);f.setframerate(16000);f.writeframes(mix.tobytes())
    print(f'{path}: {len(mix)/16000:.2f}s, peak {max(abs(x) for x in mix)}')
if __name__=='__main__':main()
