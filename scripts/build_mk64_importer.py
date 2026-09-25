"""Freeze the asset-free ROM importer for the current desktop platform.

Run this with the versions in mk64-importer-requirements.txt installed in an
isolated build environment. Native helpers are built from tools/ separately;
players do not need Python, CMake or a compiler. The destination must be new.
"""
from __future__ import annotations
import argparse
import hashlib
import importlib.metadata
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--contact-helper',type=Path,required=True)
    parser.add_argument('--motion-helper',type=Path,required=True)
    parser.add_argument('--build-tools',type=Path,help='Optional isolated PyInstaller installation used only for this build')
    parser.add_argument('--python-license',type=Path,
        help='License from the exact Python runtime source, for installations without a top-level LICENSE file')
    args=parser.parse_args()
    scripts=Path(__file__).resolve().parent
    output=args.output.resolve()
    if output.exists():
        raise ValueError('Preserve previous bundles: choose a new output directory')
    for helper in (args.contact_helper,args.motion_helper):
        if not helper.is_file():
            raise ValueError('Build the native helper first: '+str(helper))
    output.mkdir(parents=True)
    suffix='.exe' if sys.platform=='win32' else ''
    env=os.environ.copy()
    env['PYTHONNOUSERSITE']='1'
    env['PYTHONPATH']=str(args.build_tools.resolve()) if args.build_tools else ''
    command=[sys.executable,'-m','PyInstaller','--noconfirm','--clean','--onedir','--console',
        '--name','rr64-mk64-importer','--paths',str(scripts),
        '--distpath',str(output/'dist'),
        '--workpath',str(output/'work'),'--specpath',str(output/'spec')]
    # Existing terrain correction modules sit beside the package and resolve
    # their numeric policy recipes relative to __file__ at runtime.
    for recipe in sorted(scripts.glob('race_pack_*.json')):
        command += ['--add-data',str(recipe)+os.pathsep+'.']
    # PyInstaller evaluates collection helpers before Analysis adds --paths.
    # Explicit resources also provide an auditable metadata-only allowlist.
    recipes=sorted((scripts/'mk64_importer').glob('*.json'))
    for recipe in recipes:
        command += ['--add-data',str(recipe)+os.pathsep+'mk64_importer']
    command.append(str(scripts/'rr64_mk64_importer.py'))
    with (output/'build.log').open('w',encoding='utf-8') as log:
        subprocess.run(command,cwd=output,env=env,stdout=log,stderr=subprocess.STDOUT,check=True)
    bundle=output/'dist/rr64-mk64-importer'
    for recipe in recipes:
        bundled=bundle/'_internal/mk64_importer'/recipe.name
        if not bundled.is_file() or digest(bundled)!=digest(recipe):
            raise ValueError('Missing or stale bundled recipe: '+recipe.name)
    for source,name in ((args.contact_helper,'rr64-mk64-contact'),(args.motion_helper,'rr64-mk64-motion')):
        shutil.copy2(source,bundle/(name+suffix))
    licenses=bundle/'licenses';licenses.mkdir()
    packages={}
    # Include the full upstream notices, including NumPy's bundled math library
    # notices. PyInstaller's bootloader exception permits distribution of the
    # resulting application under the application's own terms.
    for name in ('numpy',):
        distribution=importlib.metadata.distribution(name)
        packages[name]=distribution.version
        copied=0
        for relative in distribution.files or []:
            text=str(relative).replace('\\','/')
            if '.dist-info/' not in text or not any(part.lower().startswith(('license','copying','notice')) for part in Path(text).parts):
                continue
            source=Path(distribution.locate_file(relative))
            if source.is_file():
                target=licenses/name/Path(text.split('.dist-info/',1)[1])
                target.parent.mkdir(parents=True,exist_ok=True)
                shutil.copy2(source,target);copied+=1
        if not copied:
            raise ValueError('Missing dependency license for '+name)
    freezer=next((d for d in importlib.metadata.distributions(
        **({'path':[str(args.build_tools)]} if args.build_tools else {}))
        if d.metadata['Name'].lower()=='pyinstaller'),None)
    if freezer is None:
        raise ValueError('PyInstaller distribution metadata is missing')
    packages['PyInstaller']=freezer.version
    freezer_notices=[p for p in freezer.files or [] if '.dist-info/licenses/' in str(p).replace('\\','/')]
    if not freezer_notices:
        raise ValueError('PyInstaller license/bootloader exception is missing')
    for relative in freezer_notices:
        source=Path(freezer.locate_file(relative))
        if source.is_file():
            target=licenses/'PyInstaller'/Path(relative).name
            target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,target)
    python_license=args.python_license or Path(sys.base_prefix)/'LICENSE.txt'
    if not args.python_license and not python_license.is_file():
        python_license=Path(sys.base_prefix)/'LICENSE'
    if not python_license.is_file():
        raise ValueError('Python runtime license not found')
    shutil.copy2(python_license,licenses/'PYTHON-LICENSE.txt')
    shutil.copy2(scripts/'MK64-IMPORTER-NOTICES.md',licenses/'MK64-IMPORTER-NOTICES.md')
    contact_license=scripts.parent/'native/lib/N64ModernRuntime/N64Recomp/LICENSE'
    shutil.copy2(contact_license,licenses/'N64Recomp-LICENSE.txt')
    run=subprocess.run([str(bundle/('rr64-mk64-importer'+suffix)),'--version'],
        cwd=output,env={k:v for k,v in env.items() if k!='PYTHONPATH'},capture_output=True,text=True,check=True)
    files={p.relative_to(bundle).as_posix():digest(p) for p in sorted(bundle.rglob('*')) if p.is_file()}
    forbidden={'.z64','.v64','.n64','.rom','.rgba16','.wav','.ogg','.mp3','.flac','.m64'}
    if any(Path(name).suffix.lower() in forbidden or 'race-packs/' in name for name in files):
        raise ValueError('A donor asset entered the converter bundle')
    sources=[scripts/'rr64_mk64_importer.py',Path(__file__),scripts/'MK64-IMPORTER-NOTICES.md',scripts/'mk64-importer-requirements.txt']
    sources.append(contact_license)
    # A distro/custom Python install may keep its license outside the project.
    # Record that exact supplied notice separately instead of inventing a path
    # beneath the source checkout or silently substituting another version.
    python_license_record={'path':str(python_license.resolve()),'sha256':digest(python_license)}
    sources += sorted(p for p in (scripts/'mk64_importer').rglob('*') if p.suffix in ('.py','.json'))
    sources += sorted(p for p in scripts.glob('race_pack_*') if p.suffix in ('.py','.json'))
    manifest=dict(format='rr64-mk64-importer-bundle',version=1,passed=True,
        platform=sys.platform,python=sys.version,packages=packages,converter_version=run.stdout.strip(),
        python_license=python_license_record,
        directory=str(bundle),files=files,
        sources={p.relative_to(scripts.parent).as_posix():digest(p) for p in sources},
        helpers={str(p.resolve()):digest(p) for p in (args.contact_helper,args.motion_helper)},
        contains_converted_assets=False,game_launched=False)
    (output/'bundle.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print(json.dumps({'bundle':str(bundle),'files':len(files),'version':run.stdout.strip()}))


if __name__=='__main__':
    main()
