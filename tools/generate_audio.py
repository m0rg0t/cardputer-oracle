#!/usr/bin/env python3
"""Pre-generate Russian narration. API key stays in the environment. Safe to resume by content hash."""
import argparse
import array
import hashlib
import json
import os
from pathlib import Path
import sys
import time
import urllib.error
import urllib.request
import wave

ROOT=Path(__file__).resolve().parents[1]
VOICE='ELWVgJ5Mo9lF5Tha9ahW'  # Arina, available Russian narrative voice.

def main():
    p=argparse.ArgumentParser(); p.add_argument('--limit',type=int,help='Maximum cards to process; defaults to the entire deck'); p.add_argument('--voice',default=VOICE)
    p.add_argument('--pause-seconds',type=float,default=0,
                   help='Pause before the first API call and between new recordings; cached files are skipped immediately')
    args=p.parse_args()
    if args.limit is not None and args.limit<1:
        p.error('--limit must be positive')
    if not 0 <= args.pause_seconds <= 3600:
        p.error('--pause-seconds must be between 0 and 3600')
    key=os.environ.get('ELEVENLABS_API_KEY')
    if not key: raise SystemExit('Set ELEVENLABS_API_KEY locally; never paste it into source or chat.')
    deck_path=ROOT/'content/deck.json'; deck=json.loads(deck_path.read_text())
    out=ROOT/'sdcard/ORACLE/audio'; out.mkdir(parents=True,exist_ok=True)
    last_request_done=time.monotonic()
    for c in deck['cards'][:args.limit]:
        payload={'text':c['text'],'model_id':'eleven_multilingual_v2','voice_settings':{'stability':0.6,'similarity_boost':0.75,'style':0.15,'use_speaker_boost':True}}
        digest=hashlib.sha256(json.dumps({'voice':args.voice,'payload':payload},sort_keys=True,ensure_ascii=False).encode()).hexdigest()
        path=out/(c['id']+'.wav'); meta=out/(c['id']+'.json')
        cached=path.exists() and meta.exists() and json.loads(meta.read_text()).get('sha256')==digest
        if not cached:
            wait=args.pause_seconds-(time.monotonic()-last_request_done)
            if wait>0:
                print(f'Pausing {wait:.0f}s before {c["id"]}',flush=True)
                deadline=time.monotonic()+wait
                while time.monotonic()<deadline:
                    time.sleep(min(60,max(0,deadline-time.monotonic())))
            request=urllib.request.Request(f'https://api.elevenlabs.io/v1/text-to-speech/{args.voice}?output_format=pcm_16000',
                data=json.dumps(payload,ensure_ascii=False).encode(),headers={'xi-api-key':key,'Content-Type':'application/json'},method='POST')
            try:
                with urllib.request.urlopen(request,timeout=55) as response: pcm=response.read()
            except urllib.error.HTTPError as e:
                # Only structured service error fields, never response headers or credentials.
                body=e.read().decode('utf-8','replace').replace(key,'[redacted]')
                try: detail=json.loads(body).get('detail',{})
                except ValueError: detail={'status':'non_json_response','message':body[:300]}
                raise SystemExit(f'ElevenLabs HTTP {e.code}: {detail.get("status","error")}: {detail.get("message","")}; stopped at {c["id"]}.')
            if not pcm or len(pcm)%2 or pcm.startswith((b'{',b'RIFF')):
                raise SystemExit(f'Unexpected PCM response for {c["id"]}')
            samples=array.array('h',pcm)
            if sys.byteorder!='little': samples.byteswap()
            peak=max(abs(s) for s in samples)
            gain=min(2.5,26000/max(peak,1))
            normalized=array.array('h',(int(max(-32768,min(32767,s*gain))) for s in samples))
            if sys.byteorder!='little': normalized.byteswap()
            tmp=path.with_suffix('.tmp')
            with wave.open(str(tmp),'wb') as wav:
                wav.setnchannels(1);wav.setsampwidth(2);wav.setframerate(16000);wav.writeframes(normalized.tobytes())
            tmp.replace(path)
            meta.write_text(json.dumps({'sha256':digest,'voice_id':args.voice,'voice_name':'Arina' if args.voice==VOICE else args.voice,'model':'eleven_multilingual_v2','text':c['text'],'seconds':len(pcm)/32000,'normalization_gain':gain},ensure_ascii=False,indent=2)+'\n')
            last_request_done=time.monotonic()
        c['audio']='/ORACLE/audio/'+path.name
        # Persist progress per card; a later failure doesn't invalidate completed media.
        deck_path.write_text(json.dumps(deck,ensure_ascii=False,indent=2)+'\n')
        print(('Cached' if cached else 'Generated')+' '+c['id'],flush=True)

if __name__=='__main__': main()
