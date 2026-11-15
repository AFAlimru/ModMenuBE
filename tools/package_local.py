"""Create a new immutable local-only package. No install or publication."""
from pathlib import Path
from datetime import datetime
import hashlib,json,struct,subprocess,zipfile
from pe_audit import audit_runtime_dependencies

ROOT=Path(__file__).resolve().parent.parent
def digest(data):return hashlib.sha256(data).hexdigest().upper()
def pe_exports(data):
    nt=struct.unpack_from('<I',data,0x3c)[0]
    assert data[nt:nt+4]==b'PE\0\0'
    machine,count=struct.unpack_from('<HH',data,nt+4)
    assert machine==0x8664
    size=struct.unpack_from('<H',data,nt+20)[0];opt=nt+24
    assert struct.unpack_from('<H',data,opt)[0]==0x20b
    sections=[struct.unpack_from('<8sIIII',data,opt+size+40*i) for i in range(count)]
    def offset(rva):
        for _,virtual,at,raw,ptr in sections:
            if at<=rva<at+min(virtual,raw):return ptr+rva-at
        raise ValueError(f'Unmapped RVA {rva:x}')
    export_rva=struct.unpack_from('<I',data,opt+112)[0];ex=offset(export_rva)
    names=struct.unpack_from('<I',data,ex+24)[0];table=offset(struct.unpack_from('<I',data,ex+32)[0])
    result=[]
    for i in range(names):
        at=offset(struct.unpack_from('<I',data,table+i*4)[0]);end=data.index(0,at);result.append(data[at:end].decode('ascii'))
    return result

def main():
    dll=(ROOT/'build/windows/x64/release/ModMenuBE.dll').read_bytes()
    exports=pe_exports(dll)
    assert exports==['ModMenu_GetApi'],exports
    imports=audit_runtime_dependencies(dll)
    assert not any('Workspace' in x for x in exports)
    for test in ['DefinitionTests','NativeTests','Native51VariantsTests','ExternalSettingsTests','SettingsTransactionTests','TextEditorTests','LocalIntegrationTests','D3D12RuntimeTests','LoaderDependencyTests']:
        for mode in ['debug','release']:
            p=ROOT/f'artifacts/verification/{mode}-{test}.txt';assert p.is_file(),str(p)
    verification={}
    for mode in ['debug','release']:
        report=json.loads((ROOT/f'artifacts/verification/{mode}-summary.json').read_text(encoding='utf-8-sig'))
        checked=(ROOT/f'build/windows/x64/{mode}/ModMenuBE.dll').read_bytes()
        assert report['dll_sha256']==digest(checked) and report['dll_bytes']==len(checked),f'Stale {mode} verification'
        assert report['executable_suites']==8 and report['dependency_audit']=='passed'
        verification[mode]=report
    original_api=subprocess.check_output(['git','show','c80be256019543eac8852d1caf49f0e9553fd4b9:include/modmenu/ModMenuApi.h'],cwd=ROOT)
    assert original_api.replace(b'\r\n',b'\n')==(ROOT/'include/modmenu/ModMenuApi.h').read_bytes().replace(b'\r\n',b'\n')
    for token in ['Workspace.cpp','Workspace.h','MMB_Workspace']:
        assert not any(token in p.name for p in (ROOT/'src').rglob('*'))
    version=json.loads((ROOT/'manifest.json').read_text(encoding='utf-8'))['version']
    out=ROOT/'releases'/f'{version}-{datetime.now():%Y%m%d-%H%M%S}'
    out.mkdir(parents=True,exist_ok=False)
    files={'mods/ModMenuBE/ModMenuBE.dll':dll}
    for name in ['manifest.json','LICENSE','THIRD_PARTY_NOTICES.txt']:
        files['mods/ModMenuBE/'+name]=(ROOT/name).read_bytes()
    files['mods/ModMenuBE/icon.png']=(ROOT/'assets/icon.png').read_bytes()
    for p in (ROOT/'adapters').rglob('*'):
        if p.is_file():files['mods/ModMenuBE/'+p.relative_to(ROOT).as_posix()]=p.read_bytes()
    for name in ['TESTING-LOCAL.md','plugin-migration-plan.md','local-integration-design.md','PROVENANCE.md','plugin-source-inventory.json','VERIFICATION-LOCAL.md','LOADER-DEPENDENCY.md']:
        files['docs/'+name]=(ROOT/'docs'/name).read_bytes()
    files['TESTING.md']=(ROOT/'docs/TESTING-LOCAL.md').read_bytes()
    records={name:{'bytes':len(data),'sha256':digest(data)} for name,data in sorted(files.items())}
    meta={'version':version,'scope':'local, no Workspace, no provider updates','manual_gameplay':'pending','dll':records['mods/ModMenuBE/ModMenuBE.dll'],'exports':exports,'imports':imports,'verification':verification,'files':records}
    files['AUDIT.json']=(json.dumps(meta,indent=2,ensure_ascii=False)+'\n').encode()
    for name,data in files.items():
        p=out/'package'/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(data)
    def archive(path,items):
        with zipfile.ZipFile(path,'x',zipfile.ZIP_DEFLATED) as z:
            for name,data in sorted(items.items()):z.writestr(name,data)
        with zipfile.ZipFile(path) as z:
            assert z.testzip() is None
            for name,data in items.items():assert z.read(name)==data
    runtime=out/f'ModMenuBE-{version}-local-test.zip';archive(runtime,files)
    sources={}
    for folder in ['src','include','tests','tools','integration','docs','assets','adapters']:
        for p in (ROOT/folder).rglob('*'):
            if p.is_file() and '__pycache__' not in p.parts:sources[p.relative_to(ROOT).as_posix()]=p.read_bytes()
    for name in ['xmake.lua','manifest.json','README.md','CHANGELOG.md','LICENSE','THIRD_PARTY_NOTICES.txt','.gitignore']:
        sources[name]=(ROOT/name).read_bytes()
    source_zip=out/f'ModMenuBE-{version}-source.zip';archive(source_zip,sources)
    checks={p.name:{'bytes':p.stat().st_size,'sha256':digest(p.read_bytes())} for p in [runtime,source_zip]}
    checks['ModMenuBE.dll']=meta['dll']
    (out/'SHA256.json').write_text(json.dumps(checks,indent=2)+'\n',encoding='utf-8')
    (out/'SHA256SUMS.txt').write_text(''.join(f'{v["sha256"]}  {k}\n' for k,v in checks.items()),encoding='ascii')
    print(json.dumps({'directory':str(out),'artifacts':checks},indent=2))
if __name__=='__main__':main()
