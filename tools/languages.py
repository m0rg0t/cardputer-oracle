"""Shared build-time language choices. English is the public default."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LANGUAGES = ('en', 'ru', 'zh')
VOICES = {'en': ('pFZP5JQG7iQjIQuC4Bku', 'Lily'),
          'ru': ('ELWVgJ5Mo9lF5Tha9ahW', 'Arina'),
          'zh': ('SyNyPD84lTuHqi1HONfV', 'Willow')}

def deck_path(language):
    if language not in LANGUAGES: raise ValueError('Unsupported language')
    return ROOT / ('content/deck.json' if language == 'ru' else f'content/deck.{language}.json')

def pack_root(language):
    if language not in LANGUAGES: raise ValueError('Unsupported language')
    return ROOT / 'build/packs' / language

def firmware_env(language):
    if language not in LANGUAGES: raise ValueError('Unsupported language')
    return 'oracle-adv' if language == 'en' else 'oracle-adv-'+language
