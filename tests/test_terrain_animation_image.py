#!/usr/bin/env python3
"""Execute the actual terrain length helper and hook body with engine stubs.
Does not execute the DLL/game or claim to repair missing art resources.
"""
import os
from pathlib import Path
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[1]
s=(ROOT/'src/Ext/TerrainType/Hooks.cpp').read_text()
def extract(marker):
 a=s.index(marker);b=s.index('{',a);depth=1;e=b+1
 while depth:
  depth+=(s[e]=='{')-(s[e]=='}');e+=1
 return s[a:e]
helper=extract('static bool TryGetAnimationLength(')
hook=extract('DEFINE_HOOK(0x71C84D, TerrainClass_AI_Animated, 0x6)')
assert 'enum { SkipGameCode = 0x71C8D5 };' in hook
assert hook.count('return SkipGameCode;')==2
assert 'return 0;' not in hook
cpp=r'''
#include <cassert>
#include <cstdio>
#include <cstring>
#include <iostream>
struct SHPStruct { short Frames; };
struct NullableInt {bool set=false;int value=0;bool isset() const {return set;}int Get() const {return value;}};
struct TerrainTypeClass {const char* ID="MISSING_TERRAIN";bool IsAnimated=true,SpawnsTiberium=true;SHPStruct* image=nullptr;int reads=0;SHPStruct* GetImage(){++reads;return image;}};
struct Cell {int spreads=0;void SpreadTiberium(bool){++spreads;}} cell;
struct TerrainClass {TerrainTypeClass* Type;struct Anim {int Value=0,starts=0;void Start(int){++starts;}} Animation;int Location=0;Cell* GetCell(){return &cell;}};
struct TerrainTypeExt {struct ExtData {NullableInt AnimationLength;bool HasDamagedFrames=false;int SpawnsTiberium_Particle=-1;int GetCellsPerAnim()const{return 1;}};struct Map {ExtData value;ExtData* Find(TerrainTypeClass*){return &value;}};static Map ExtMap;};
TerrainTypeExt::Map TerrainTypeExt::ExtMap;
namespace TerrainTypeTemp {TerrainTypeClass* pCurrentType=nullptr;TerrainTypeExt::ExtData* pCurrentExt=nullptr;}
struct ParticleSystemClass {void SpawnParticle(int,int){}};
struct ParticleTypeClass {inline static int Array[1]={0};};
template<class T>T Make_Global(unsigned){return nullptr;}
struct REGISTERS {TerrainClass* terrain;};
#define GET(type,name,reg) type name=R->terrain
static int warnings=0;static char warningText[512];
struct Debug {template<class...A>static void Log(const char* f,A...a){++warnings;std::snprintf(warningText,sizeof(warningText),f,a...);}};
'''
cpp+='namespace TerrainAnimation {\n'+helper+'\n}\n'
cpp+=hook.replace('DEFINE_HOOK(0x71C84D, TerrainClass_AI_Animated, 0x6)','static int TerrainClass_AI_Animated(REGISTERS* R)')
cpp+=r'''
int main(){
 TerrainTypeClass type;TerrainClass object{};object.Type=&type;REGISTERS regs{&object};auto& ext=TerrainTypeExt::ExtMap.value;
 int length=123;assert(!TerrainAnimation::TryGetAnimationLength(&type,&ext,length)&&length==123);
 type.reads=0;
 assert(TerrainClass_AI_Animated(&regs)==0x71C8D5);
 assert(object.Animation.Value==0&&object.Animation.starts==0&&cell.spreads==0);
 assert(type.reads==1&&warnings==1&&std::strstr(warningText,"MISSING_TERRAIN"));
 for(int i=0;i<100;++i)assert(TerrainClass_AI_Animated(&regs)==0x71C8D5);
 assert(warnings==1&&object.Animation.starts==0&&cell.spreads==0);
 // Explicit length must not consult the missing image, preserving invisible-spawner configurations.
 ext.AnimationLength={true,6};object.Animation.Value=6;type.reads=0;
 assert(TerrainClass_AI_Animated(&regs)==0x71C8D5);
 assert(type.reads==0&&object.Animation.Value==0&&object.Animation.starts==1&&cell.spreads==1);
 assert(TerrainTypeTemp::pCurrentType==nullptr&&TerrainTypeTemp::pCurrentExt==nullptr);
 // Valid auto length, damaged frames, and old zero-frame behavior are unchanged.
 ext.AnimationLength={};SHPStruct image{12};type.image=&image;object.Animation.Value=6;
 assert(TerrainClass_AI_Animated(&regs)==0x71C8D5&&object.Animation.starts==2&&cell.spreads==2);
 ext.HasDamagedFrames=true;object.Animation.Value=3;
 assert(TerrainClass_AI_Animated(&regs)==0x71C8D5&&object.Animation.starts==3&&cell.spreads==3);
 image.Frames=0;object.Animation.Value=0;
 assert(TerrainClass_AI_Animated(&regs)==0x71C8D5&&object.Animation.starts==4&&cell.spreads==4);
 type.IsAnimated=false;type.image=nullptr;type.reads=0;
 assert(TerrainClass_AI_Animated(&regs)==0x71C8D5&&type.reads==0&&object.Animation.starts==4);
 std::cout<<"PASS: actual terrain length helper and hook body; null/explicit/valid/damaged/zero/nonanimated paths; one diagnostic; unchanged return target\n";
}
'''
with tempfile.TemporaryDirectory(prefix='terrain-image-test-') as tmp:
 p=Path(tmp)/'test.cpp';binary=Path(tmp)/'test';p.write_text(cpp)
 subprocess.run([os.environ.get('CXX','g++'),'-std=c++20','-O1','-Wall','-Wextra','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(p),'-o',str(binary)],check=True)
 subprocess.run([str(binary)],check=True)
