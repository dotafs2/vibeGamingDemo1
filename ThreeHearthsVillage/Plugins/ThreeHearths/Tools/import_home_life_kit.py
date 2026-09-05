"""Native HomeLifeKit import, using the shared preserved-origin art verifier."""
from pathlib import Path
import sys
sys.path.insert(0,str(Path(__file__).resolve().parent))
from import_resident_kit import import_kit

if __name__=='__main__':import_kit('HomeLifeKit',include_examples=True)
