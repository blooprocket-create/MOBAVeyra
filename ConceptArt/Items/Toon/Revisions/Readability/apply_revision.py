import json, shutil
from pathlib import Path
HERE = Path(__file__).resolve().parent
TOON = HERE.parent.parent
changes = json.loads((HERE/'prompts.json').read_text())
record = json.loads((TOON/'generation.json').read_text())
repairs = json.loads((TOON/'repairs.json').read_text())
for change in changes:
    item = next(i for i in record['items'] if i['id'] == change['id'])
    item.setdefault('prompt_history', []).append({'prompt':item['prompt'], 'subject':item['subject']})
    item['prompt'] = change['prompt']
    item['subject'] = change['spec']
    shutil.copyfile(HERE/(change['id']+'.png'), TOON/'Sources'/(change['id']+'.png'))
    repairs.append(change)
(TOON/'generation.json').write_text(json.dumps(record,indent=2)+'\n')
(TOON/'repairs.json').write_text(json.dumps(repairs,indent=2)+'\n')
