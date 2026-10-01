#!/usr/bin/env python3
"""Deterministic input-cache fixtures using extracted production helpers.
Engine reads are stubbed; this is not an in-game benchmark or full Draw integration.
"""
import os
from pathlib import Path
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parents[1]
s = (ROOT / 'src/Misc/Hooks.VeinholeMonster.cpp').read_text()
def extract(marker):
    a=s.index(marker); b=s.index('{',a); n=1; e=b+1
    while n:
        n += (s[e]=='{')-(s[e]=='}'); e+=1
    return s[a:e]
cpp=r'''
#include <algorithm>
#include <array>
#include <cassert>
#include <climits>
#include <iostream>
#include <string>
#include <vector>
using std::size_t;
struct Point2D { int X,Y; };
struct CellStruct { short X,Y; };
struct RectangleStruct { int X,Y,Width,Height; };
struct ColorStruct { int R,G,B; };
struct HouseClass { inline static std::vector<HouseClass*> Array; };
struct Unsorted { inline static int CurrentFrame=0; };
'''
for name in ('OverlayCellKind','DiamondEdge'):
    cpp+=extract('enum class '+name)+';\n'
for name in ('CliffCoverEdge','FrontierInfo','OverlaySpan','OverlaySpanEvent','OverlayDrawRect','HeightFaceStats','OverlayRegionCacheKey','OverlayRegionCacheStats','OverlayGeometryInput','OverlayVisibilityInput','OverlayHouseInput','OverlayRegionCacheEntry'):
    cpp+=extract('struct '+name)+';\n'
cpp+=r'''
static int OverlayFrameGeometryProbes=0,OverlayFrameVisibilityQueries=0;
static std::vector<OverlayDrawRect> OverlayScratchDrawRects;
static const std::vector<OverlayGeometryInput>* ActiveOverlayGeometryInputs=nullptr;
static int ActiveOverlayGeometryMinX=1,ActiveOverlayGeometryMinY=1,ActiveOverlayGeometryWidth=6,ActiveOverlayGeometryHeight=6;
struct Cell { bool valid=true; int height=0,last=-1; OverlayCellKind kind=OverlayCellKind::ExploredOverlay; };
static Cell grid[16][16];
static int camera=0;
static HouseClass* viewer=nullptr;
struct Ext { unsigned PhobosFog_StateVersion=1; bool full=false; bool IsPhobosFogFullMapHardVisible() const {return full;} } ext;
struct HouseExt { struct Map { Ext* TryFind(HouseClass*) {return &ext;} }; inline static Map ExtMap; };
bool IsEligibleViewerOrAllyHouse(HouseClass*,HouseClass*) {return true;}
struct MapClass { Cell* TryGetCellAt(CellStruct c) {return c.X>=0&&c.X<16&&c.Y>=0&&c.Y<16&&grid[c.Y][c.X].valid ? &grid[c.Y][c.X] : nullptr;} static MapClass Instance; };
MapClass MapClass::Instance;
bool TryGetExploredOverlayCellIndex(CellStruct c,int& index) {if(!MapClass::Instance.TryGetCellAt(c)) return false;index=c.Y*16+c.X;return true;}
OverlayCellKind GetOverlayCellKindForViewerAndAllies(HouseClass*,int i) {++OverlayFrameVisibilityQueries;return ext.full?OverlayCellKind::Visible:grid[i/16][i%16].kind;}
int GetLastVisibleFrameForViewerAndAllies(HouseClass*,int i) {++OverlayFrameVisibilityQueries;return grid[i/16][i%16].last;}
std::pair<Point2D,bool> ProjectOverlayCellClient(CellStruct c,bool heightAware,int offset) {
 ++OverlayFrameGeometryProbes;auto p=MapClass::Instance.TryGetCellAt(c);
 return {{c.X*10+c.Y*3+camera,c.Y*8+offset-(heightAware&&p?p->height:0)},true};
}
using Call=std::array<int,5>;static std::vector<Call> calls;
struct Surface {bool FillRectTrans(RectangleStruct* r,ColorStruct*,int a) {calls.push_back({r->X,r->Y,r->Width,r->Height,a});return true;}} surface;
struct DSurface {inline static Surface* Composite=&surface;};
'''
functions=[
'static int ClampAlpha(', 'static int AbsInt(', 'static int ComputeOverlayFadeAlpha(', 'static int NextOverlayAlphaChangeFrame(',
'static bool IsExploredOverlayCell(', 'static OverlayCellKind GetOverlayCellKindForCell(', 'static FrontierInfo ClassifyExploredOverlayFrontier(',
'static int StableCellNoise(', 'static int ComputeCellAlpha(', 'static void BuildOverlayHouseInputs(',
'static void BuildOverlayGeometryInputs(', 'static void BuildOverlayVisibilityInputs(', 'static bool CanReuseOverlayVisibilityInputs(',
'static bool OverlayRegionCacheKeysEqual(', 'static std::pair<Point2D, bool> GetOverlayCellClient(',
'static bool TryClipRectToBounds(', 'static bool DrawClippedRectTrans(', 'static bool TryGetDiamondSpan(',
'static bool TryGetDiamondEdgePoints(', 'static bool TryBuildClippedSpan(', 'static bool TryAddClippedSpan(',
'static void AddClippedSpan(', 'static void AddRectSpans(', 'static void AddCliffFaceStripSpans(',
'static void BuildCoalescedRects(', 'static bool DrawPreparedRects(', 'static void MergeSpansToUnionSpans(',
'static int ScaleOverlayAlpha(', 'static void GenerateDownDilationSpans(', 'static void MergeRegionSpansWithCloseGap(',
'static void AddRectangularSoftEdgeSpans(', 'static DiamondEdge GetDiamondEdgeTowardNeighbor(',
'static void NormalizeEdgeEndpointOrder(', 'static bool TryAddHeightDiscontinuityFaceSpans(',
'static void AddDiamondSoftEdgeSideSpans(', 'static void AddDiamondSoftEdgeTowardNeighborSpans(']
cpp+='\n'.join(extract(f) for f in functions)
cpp+=r'''
static RectangleStruct bounds{0,0,100,85};
static int at(int x,int y) {return (y-1)*6+(x-1);}
static void prepare(const std::vector<OverlayVisibilityInput>& vis,int shape,std::vector<OverlayDrawRect>& main,std::vector<OverlayDrawRect>& edge) {
 std::vector<OverlaySpan> m,e,faces,mu,eu,dilation,all,final; HeightFaceStats stats{};
 for(int y=2;y<=5;++y) for(int x=2;x<=5;++x) {
  auto v=vis[at(x,y)]; if(v.Kind!=OverlayCellKind::ExploredOverlay) continue;
  CellStruct c{short(x),short(y)};
  const bool diamond=shape==2||(shape==1&&ClassifyExploredOverlayFrontier(viewer,c,8).IsFrontier);
  auto p=GetOverlayCellClient(c,false,0).first;
  RectangleStruct rect{p.X-8,p.Y-6,16,12};
  if(diamond) {for(int row=rect.Y;row<rect.Y+rect.Height;++row) {int l,r;if(TryGetDiamondSpan(rect,row,l,r)) AddClippedSpan(m,bounds,row,l,r,v.MainAlpha);}}
  else AddRectSpans(m,rect,bounds,v.MainAlpha);
  TryAddHeightDiscontinuityFaceSpans(faces,bounds,v.MainAlpha,c,{1,0},0,16,12,0,0,4,64,stats);
  TryAddHeightDiscontinuityFaceSpans(faces,bounds,v.MainAlpha,c,{0,1},0,16,12,0,0,4,64,stats);
  const int dx[]={0,0,-1,1},dy[]={-1,1,0,0};
  for(int d=0;d<4;++d) if(vis[at(x+dx[d],y+dy[d])].Kind==OverlayCellKind::Visible) {
   auto q=GetOverlayCellClient({short(x+dx[d]),short(y+dy[d])},false,0).first;
   RectangleStruct nr{q.X-8,q.Y-6,16,12};
   if(diamond) AddDiamondSoftEdgeTowardNeighborSpans(e,nr,bounds,v.EdgeAlpha,3,q,p);
   else AddRectangularSoftEdgeSpans(e,nr,bounds,v.EdgeAlpha,3,d^1);
  }
 }
 int gaps=0;MergeRegionSpansWithCloseGap(m,mu,2,gaps);
 GenerateDownDilationSpans(mu,bounds,2,1,1,dilation);
 all=mu;all.insert(all.end(),faces.begin(),faces.end());all.insert(all.end(),dilation.begin(),dilation.end());
 MergeRegionSpansWithCloseGap(all,final,2,gaps);MergeSpansToUnionSpans(e,eu);
 BuildCoalescedRects(final,main);BuildCoalescedRects(eu,edge);
}
static bool submit(const std::vector<OverlayDrawRect>& m,const std::vector<OverlayDrawRect>& e,int budget) {
 calls.clear();ColorStruct color{};int count=0;return DrawPreparedRects(m,bounds,color,count,budget)&&DrawPreparedRects(e,bounds,color,count,budget);
}
int main() {
 HouseClass house;viewer=&house;HouseClass::Array={&house};int frames=0;
 for(const auto& name: {"mature_fade6","active_fade6","future_timestamp","offscreen_mass_move","overlapping_visibility","onscreen_frontier","halo_change","diagonal_halo_change","height_without_heightaware","camera_scroll","instant_fullmap"}) {
  for(int shape:{0,1,2}) for(int budget:{0,1,5,10000}) {
   for(auto& row:grid) for(auto& c:row)c=Cell{};
   for(int y=1;y<=6;++y)grid[y][6].kind=OverlayCellKind::Visible;
   ext={};camera=0;OverlayRegionCacheEntry cache{};
   int hits=0,rebuilds=0,queries=0,probes=0,submitted=0;
   for(int f=0;f<24;++f) {
    Unsorted::CurrentFrame=100+f;
    if(std::string(name)=="active_fade6")grid[3][3].last=100;
    if(std::string(name)=="future_timestamp")grid[3][3].last=110;
    if(std::string(name)=="offscreen_mass_move"||std::string(name)=="overlapping_visibility") {
     // Synthetic sight-provider movement, not execution of the game's unit logic.
     for(auto& row:grid)for(auto& c:row)c.kind=OverlayCellKind::ExploredOverlay;
     for(int y=1;y<=6;++y)grid[y][6].kind=OverlayCellKind::Visible;
     for(int unit=0;unit<2048;++unit) {
      int x=std::string(name)=="offscreen_mass_move"?9+(unit+f)%6:7+(unit+f)%2;
      int y=std::string(name)=="offscreen_mass_move"?9+(unit/6+f)%6:2+(unit/2+f)%4;
      for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx) {
       int xx=x+dx,yy=y+dy;if(xx>=0&&xx<16&&yy>=0&&yy<16)grid[yy][xx].kind=OverlayCellKind::Visible;
      }
     }
     ++ext.PhobosFog_StateVersion;
    }
    if(std::string(name)=="onscreen_frontier") {grid[3][3].kind=f%2?OverlayCellKind::Visible:OverlayCellKind::ExploredOverlay;++ext.PhobosFog_StateVersion;}
    if(std::string(name)=="halo_change") {grid[3][6].kind=f%2?OverlayCellKind::Visible:OverlayCellKind::Unknown;++ext.PhobosFog_StateVersion;}
    if(std::string(name)=="diagonal_halo_change") {grid[1][1].kind=f%2?OverlayCellKind::Visible:OverlayCellKind::ExploredOverlay;++ext.PhobosFog_StateVersion;}
    if(std::string(name)=="height_without_heightaware")grid[4][4].height=(f%2)*12;
    if(std::string(name)=="camera_scroll")++camera;
    if(std::string(name)=="instant_fullmap")ext.full=f%2; // No raw version change.
    OverlayFrameVisibilityQueries=OverlayFrameGeometryProbes=0;
    std::vector<OverlayGeometryInput> geometry;std::vector<OverlayHouseInput> houses;std::vector<OverlayVisibilityInput> vis;
    BuildOverlayHouseInputs(&house,houses);BuildOverlayGeometryInputs(2,5,2,5,0,geometry);
    ActiveOverlayGeometryInputs=&geometry;
    OverlayRegionCacheKey key{};key.ViewerHouse=&house;key.OverlayViewportHash=camera;key.OverlayConfigHash=shape;
    bool gm=cache.Valid&&OverlayRegionCacheKeysEqual(cache.Key,key)&&cache.GeometryInputs==geometry;
    bool reuse=gm&&CanReuseOverlayVisibilityInputs(cache,houses,Unsorted::CurrentFrame);
    int next=cache.NextAlphaChangeFrame;
    if(!reuse)BuildOverlayVisibilityInputs(&house,2,5,2,5,96,5,70,6,vis,next);
    bool hit=gm&&(reuse||cache.VisibilityInputs==vis);
    if(hit)++hits;else{++rebuilds;prepare(vis,shape,cache.MainRects,cache.EdgeRects);cache.GeometryInputs=geometry;cache.VisibilityInputs=vis;}
    cache.Key=key;cache.HouseInputs=houses;cache.NextAlphaChangeFrame=next;cache.LastValidatedFrame=Unsorted::CurrentFrame;
    assert(OverlayFrameGeometryProbes==72); // Rebuild helpers reuse all captured projections.
    cache.Valid=submit(cache.MainRects,cache.EdgeRects,budget);auto actual=calls;bool ok=cache.Valid;
    queries+=OverlayFrameVisibilityQueries;probes+=OverlayFrameGeometryProbes;submitted+=calls.size();
    // Independent uncached input reads every frame, using the same real geometry/union/draw helpers.
    std::vector<OverlayVisibilityInput> reference;int ignored=0;
    BuildOverlayVisibilityInputs(&house,2,5,2,5,96,5,70,6,reference,ignored);
    std::vector<OverlayDrawRect> rm,re;prepare(reference,shape,rm,re);
    assert(submit(rm,re,budget)==ok&&calls==actual);
    ActiveOverlayGeometryInputs=nullptr;++frames;
   }
   if(budget==10000) {
    if(std::string(name)=="offscreen_mass_move"||std::string(name)=="overlapping_visibility"||std::string(name)=="mature_fade6")assert(rebuilds==1&&hits==23);
    if(std::string(name)=="active_fade6"||std::string(name)=="future_timestamp")assert(rebuilds>1&&hits>0);
    if(std::string(name)=="camera_scroll"||std::string(name)=="height_without_heightaware"||std::string(name)=="instant_fullmap")assert(rebuilds==24);
    std::cout<<name<<" shape="<<shape<<" frames=24 hits="<<hits<<" rebuilds="<<rebuilds<<" geometry_probes="<<probes<<" visibility_queries="<<queries<<" submitted_rectangles="<<submitted<<"\n";
   }
  }
 }
 // Quantized equal-alpha frames must still advance the next-change deadline.
 assert(ComputeOverlayFadeAlpha(1,101,100,60)==ComputeOverlayFadeAlpha(1,102,100,60));
 assert(NextOverlayAlphaChangeFrame(101,100,60)==102&&NextOverlayAlphaChangeFrame(102,100,60)==103);
 assert(NextOverlayAlphaChangeFrame(100,110,6)==110);
 assert(ComputeOverlayFadeAlpha(96,109,110,6)==96&&ComputeOverlayFadeAlpha(96,110,110,6)==0);
 OverlayRegionCacheEntry t{};t.LastValidatedFrame=100;t.NextAlphaChangeFrame=105;
 assert(!CanReuseOverlayVisibilityInputs(t,{},99)&&!CanReuseOverlayVisibilityInputs(t,{},105));
 t.NextAlphaChangeFrame=0;t.TemporalCacheFirstInvalidFrame=104;assert(!CanReuseOverlayVisibilityInputs(t,{},104));
 std::cout<<"PASS: "<<frames<<" deterministic frame/budget comparisons; actual input, alpha, union and draw helpers\n";
}
'''
with tempfile.TemporaryDirectory(prefix='phobos-input-test-') as tmp:
    source=Path(tmp)/'test.cpp';binary=Path(tmp)/'test';source.write_text(cpp)
    subprocess.run([os.environ.get('CXX','g++'),'-std=c++20','-O1','-Wall','-Wextra','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(source),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
