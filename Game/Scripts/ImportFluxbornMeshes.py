"""Import the Fluxborn kit using the validated editor-only art pipeline."""
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from ImportGreyboxMeshKit import import_kit

import_kit(Path(__file__).resolve().parents[1], "Fluxborn", "FluxbornKit.json",
           "/Game/Veyra/Flux/Fluxborn/Greybox", "FluxbornKit", "VEYRA_FLUXBORN_IMPORT_PASSED")
