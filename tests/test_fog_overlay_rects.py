#!/usr/bin/env python3
"""Compile actual old/new rendering helpers with a recording surface, not the game.
Requires Python 3, git, and a C++20 compiler (CXX, default g++).
No game headers, game executable, Windows DLL or repository scripts are executed.
"""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SOURCE = "src/Misc/Hooks.VeinholeMonster.cpp"
BASE = "949d2a83c2a8e2eb9168cfe207b97dd7b44b81a1"
old = subprocess.check_output(["git", "show", f"{BASE}:{SOURCE}"], cwd=ROOT, text=True)
new = (ROOT / SOURCE).read_text()

def definition(source, marker):
    start = source.index(marker)
    opening = source.index("{", start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]

preamble = r'''
#include <algorithm>
#include <array>
#include <cassert>
#include <iostream>
#include <random>
#include <vector>
using std::size_t;
struct RectangleStruct { int X,Y,Width,Height; };
struct ColorStruct { int R,G,B; };
using Call = std::array<int,5>;
static std::vector<Call> calls;
struct Surface { bool FillRectTrans(RectangleStruct* r, ColorStruct*, int a) {
 calls.push_back({r->X,r->Y,r->Width,r->Height,a}); return true; } };
static Surface surface;
struct DSurface { inline static Surface* Composite = &surface; };
'''
structs = ''.join(definition(new, "struct " + n) + ";\n" for n in ("OverlaySpan", "OverlayDrawRect"))
common = ["static bool TryClipRectToBounds(", "static bool DrawClippedRectTrans("]
def helpers(source, namespace, names):
    return "namespace " + namespace + " {\n" + structs + "static std::vector<OverlayDrawRect> OverlayScratchDrawRects;\n" + ''.join(definition(source, n) + "\n" for n in common + names) + "}\n"
cpp = preamble + helpers(old, "before", ["static bool DrawCoalescedRects(", "static bool DrawUnionSpans("])
cpp += helpers(new, "after", ["static void BuildCoalescedRects(", "static bool DrawPreparedRects(", "static bool DrawUnionSpans("])
cpp += "namespace lifecycle { struct HouseClass {};\n" + structs
cpp += ''.join(definition(new, "struct " + name) + ";\n" for name in ("OverlayRegionCacheKey", "OverlayRegionCacheStats", "OverlayRegionCacheEntry"))
cpp += "static OverlayRegionCacheEntry OverlayFinalRegionCache; static int OverlayRegionCacheHitStreak;\n"
cpp += definition(new, "void ResetCache()") + "\n}\n"
cpp += r'''
int main() {
 lifecycle::OverlayFinalRegionCache.Valid=true;
 lifecycle::OverlayFinalRegionCache.MainRects.push_back({1,2,3,4,5});
 lifecycle::OverlayFinalRegionCache.EdgeRects.push_back({6,7,8,9,10});
 lifecycle::OverlayFinalRegionCache.FinalRegionUnionSpans.push_back({1,2,3,4});
 lifecycle::OverlayFinalRegionCache.EdgeUnionSpanCount=5;
 lifecycle::OverlayRegionCacheHitStreak=12;
 lifecycle::ResetCache();
 assert(!lifecycle::OverlayFinalRegionCache.Valid);
 assert(lifecycle::OverlayFinalRegionCache.MainRects.empty() && lifecycle::OverlayFinalRegionCache.EdgeRects.empty());
 assert(lifecycle::OverlayFinalRegionCache.FinalRegionUnionSpans.empty());
 assert(lifecycle::OverlayFinalRegionCache.EdgeUnionSpanCount==0 && lifecycle::OverlayRegionCacheHitStreak==0);
 std::mt19937 rng(748);
 int checks=0;
 for(int scenario=0; scenario<1000; ++scenario) {
  std::vector<before::OverlaySpan> m,e;
  std::vector<after::OverlaySpan> nm,ne;
  auto add=[&](int y,int x,int w,int a,bool edge) {
   (edge?e:m).push_back({y,x,x+w,a}); (edge?ne:nm).push_back({y,x,x+w,a});
  };
  // Explicit vertical runs, clipped runs, equal keys, empty passes and gradients.
  if(scenario%5) for(int y=-5;y<30;++y) add(y,scenario%10,8,80,false);
  for(int i=0;i<scenario%80;++i) add(int(rng()%70)-20,int(rng()%80)-25,int(rng()%12)+1,int(rng()%255)+1,rng()%2);
  RectangleStruct bounds{scenario%7-3,scenario%9-4,35,27}; ColorStruct color{};
  std::vector<after::OverlayDrawRect> mainRects,edgeRects;
  after::BuildCoalescedRects(nm,mainRects); after::BuildCoalescedRects(ne,edgeRects);
  for(int budget=0;budget<100; ++budget) {
   calls.clear(); int n=0;
   bool ok=before::DrawUnionSpans(m,bounds,color,n,budget) && before::DrawUnionSpans(e,bounds,color,n,budget);
   auto expected=calls;
   calls.clear(); int nn=0;
   bool nok=after::DrawUnionSpans(nm,bounds,color,nn,budget) && after::DrawUnionSpans(ne,bounds,color,nn,budget);
   assert(ok==nok && n==nn && calls==expected);
   // Reuse cached final rectangles twice, without preparation between hits.
   for(int hit=0;hit<2;++hit) {
    calls.clear(); nn=0;
    nok=after::DrawPreparedRects(mainRects,bounds,color,nn,budget) && after::DrawPreparedRects(edgeRects,bounds,color,nn,budget);
    assert(ok==nok && n==nn && calls==expected);
   }
   ++checks;
  }
  // A rebuild with no edge geometry must not retain the previous edge.
  after::BuildCoalescedRects({},edgeRects); assert(edgeRects.empty());
 }
 std::cout << "PASS: " << checks << " old/new submission comparisons, including repeated cache hits and shared budgets\n";
}
'''
# Static integration checks complement helper execution; they are not runtime tests.
assert 'result.DisabledReason = "SoftEdge"' not in new
assert 'result.DisabledReason = "FadeIn"' in new
hit = new[new.index("if (overlayCacheHit)"):new.index("const bool overlayCacheMiss")]
assert "BuildCoalescedRects(" not in hit and "DrawUnionSpans(" not in hit
assert "OverlayFinalRegionCache.EdgeRects" in hit
assert new.index("if (regionDrawSucceeded && !edgeSpans.empty())") < new.rindex("LogOverlayPerfSummary(perfSnapshot);")
assert "PhobosFogExploredOverlay::ResetCache();" in definition((ROOT / "src/Phobos.Ext.cpp").read_text(), "DEFINE_HOOK(0x685659, Scenario_ClearClasses, 0xa)")
assert "PhobosFog_UpdateInterval { 30 }" in (ROOT / "src/Ext/Rules/Body.h").read_text()
scenario = (ROOT / "src/Ext/Scenario/Body.cpp").read_text()
assert "if (!forceRefresh && updateInterval > 1 && currentFrame % updateInterval != 0)" in scenario
with tempfile.TemporaryDirectory(prefix="phobos-overlay-test-") as tmp:
    source = Path(tmp) / "test.cpp"
    binary = Path(tmp) / "test"
    source.write_text(cpp)
    subprocess.run([os.environ.get("CXX", "g++"), "-std=c++20", "-O1", "-Wall", "-Wextra", "-fsanitize=address,undefined", "-fno-omit-frame-pointer", str(source), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
print("PASS: integration guards (cache hit, fade exclusion, logging order, default interval and force path)")
