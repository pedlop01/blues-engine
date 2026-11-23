"""Read-only data paths for classic Blues regressions after the directory move."""
import argparse
from functools import lru_cache
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
@lru_cache(maxsize=1)
def game_root():
 parser=argparse.ArgumentParser(description='Classic Blues regression with external DOS/Amiga data')
 parser.add_argument('--orig-games',type=Path,default=ROOT.parent/'orig-games')
 return parser.parse_args().orig_games.resolve()
def data_path(mode):
 for name in ('blues-'+mode,mode):
  directory=game_root()/name
  if directory.is_dir():return directory
 raise SystemExit(f'Missing Blues {mode} data under {game_root()}')
