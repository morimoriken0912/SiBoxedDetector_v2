// makeSiViewer.cc
//
// Reads SiDetector.root (Geant4 output) and visualizes the Si detector array in 3D.
// Generates a standalone HTML file (SiViewer.html).
//
// Usage 1: Run as a ROOT macro
//   root -l -b -q 'makeSiViewer.cc("SiDetector.root","SiViewer.html")'
//
// Usage 2: Compile and run
//   g++ -O2 -std=c++17 makeSiViewer.cc -o makeSiViewer $(root-config --cflags --libs)
//   ./makeSiViewer SiDetector.root SiViewer.html

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "TFile.h"
#include "TTree.h"
#include "TTreeReader.h"
#include "TTreeReaderValue.h"

// ---------------------------------------------------------------------------
// Detector geometry parameters. Must match those in DetectorConstruction.cc
// ---------------------------------------------------------------------------
static const int kNPix = 8;  // 8 x 8 pixels per face
static const double kPitch = 8.0;  // Pixel pitch [mm]
static const double kPixSize = 7.5;  // Pixel size [mm]
static const double kSensSize = 7.05;  // Sensitive area size [mm]
static const double kPixThick = 0.15;  // Pixel thickness [mm]
static const double kBinWidth = 0.1;   // Default histogram bin width [MeV]

// ---------------------------------------------------------------------------
// HTML Template. Data will be embedded in place of /*DATA_JSON*/null.
// ---------------------------------------------------------------------------
static const char kHtml[] = R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Si Detector Array Viewer</title>
<style>
:root{
  --bg:#ffffff; --fg:#1f2328; --muted:#5f6b7a; --panel:#f3f5f8; --border:#d0d7de;
  --cbg:#dfe3e8; --imp:#e8710a; --dec:#0d9488; --sel:rgba(70,110,255,.20);
}
@media (prefers-color-scheme: dark){
  :root{
    --bg:#101216; --fg:#e7e9ec; --muted:#9aa4b0; --panel:#181b21; --border:#2c323b;
    --cbg:#262b33; --sel:rgba(110,150,255,.28);
  }
}
*{box-sizing:border-box}
body{margin:0;padding:12px 16px;background:var(--bg);color:var(--fg);
  font:14px/1.55 system-ui,-apple-system,"Hiragino Sans","Yu Gothic UI","Noto Sans JP",sans-serif}
h1{font-size:18px;margin:0 0 2px}
.sub{color:var(--muted);font-size:12px;margin-bottom:10px}
.controls{display:grid;grid-template-columns:repeat(auto-fit,minmax(260px,1fr));gap:8px;margin-bottom:10px}
fieldset{border:1px solid var(--border);border-radius:8px;padding:6px 10px 8px;background:var(--panel);margin:0}
legend{font-size:12px;color:var(--muted);padding:0 4px}
label{margin-right:8px;white-space:nowrap;font-size:13px;}
input[type=number]{width:80px}
button{cursor:pointer;margin:2px 4px 2px 0; padding: 2px 8px;}
.main{display:grid;grid-template-columns:minmax(0,1.25fr) minmax(0,1.25fr);gap:12px}
@media (max-width:1100px){.main{grid-template-columns:1fr}}
.views{display:flex;gap:8px}
.vp{flex:1;min-width:0;border:1px solid var(--border);border-radius:8px;overflow:hidden}
canvas{display:block;width:100%}
.legend{display:flex;align-items:center;gap:8px;margin:8px 0;font-size:12px}
.swatch{width:18px;height:14px;border:1px solid var(--border);background:#fff}
.bar{flex:1;max-width:320px;height:14px;border:1px solid var(--border)}
.panel{border:1px solid var(--border);border-radius:8px;padding:8px;background:var(--panel)}
.stats{font-size:13px;margin-top:6px}
.note{font-size:12px;color:var(--muted);margin-top:8px}
#tip{position:fixed;display:none;pointer-events:none;background:rgba(20,22,28,.92);color:#fff;
  padding:5px 8px;border-radius:6px;font-size:12px;line-height:1.5;z-index:10;white-space:pre}
</style>
</head>
<body>
<h1>Si Detector Array Viewer</h1>
<div class="sub" id="info"></div>

<div class="controls">
  <!-- Row 1 -->
  <fieldset>
    <legend>Display Mode & Threshold</legend>
    <div style="margin-bottom:6px;">
      <label><input type="radio" name="mode" value="imp">Implantation</label>
      <label><input type="radio" name="mode" value="dec">Decay</label>
      <label><input type="radio" name="mode" value="all" checked>Both (Sum)</label>
      <label><input type="radio" name="mode" value="split">Side-by-Side</label>
    </div>
    <div style="border-top: 1px solid var(--border); padding-top: 6px;">
      <input type="number" id="thr" min="0" step="1" value="100"> MeV or more as "Implantation"
    </div>
  </fieldset>
  
  <!-- Row 2 -->
  <fieldset>
    <legend>3D Display Settings</legend>
    <div style="margin-bottom:4px;">
      Faces:
      <label><input type="checkbox" class="face" value="0" checked>+z</label>
      <label><input type="checkbox" class="face" value="1" checked>+x</label>
      <label><input type="checkbox" class="face" value="2" checked>-x</label>
      <label><input type="checkbox" class="face" value="3" checked>+y</label>
      <label><input type="checkbox" class="face" value="4" checked>-y</label>
    </div>
    <div style="margin-bottom:4px;">
      <label>Opacity <input type="range" id="alpha" min="15" max="100" value="100" style="width:80px; vertical-align:middle;"></label>
      <label><input type="checkbox" id="logc">Log Color Scale</label>
    </div>
    <div>
      Viewpoint:
      <button data-view="in">Front</button>
      <button data-view="obl">Oblique</button>
      <button data-view="back">Back</button>
    </div>
  </fieldset>

  <!-- Particle Selection -->
  <fieldset style="border-color:#8b5cf6; background:rgba(139,92,246,0.05);">
    <legend style="color:#8b5cf6; font-weight:bold;">Particle Selection & Efficiency</legend>
    <div style="margin-bottom:6px; display:flex; flex-wrap:wrap; gap:4px;">
      <label><input type="radio" name="ptype" value="all" checked> All</label>
      <label><input type="radio" name="ptype" value="alpha"> Alpha</label>
      <label><input type="radio" name="ptype" value="gamma"> Gamma</label>
      <label><input type="radio" name="ptype" value="electron"> Beta (e±)</label>
      <label><input type="radio" name="ptype" value="heavy"> Heavy Ion</label>
    </div>
    <div style="margin-bottom:6px; font-size:12px;">
      <label><input type="checkbox" id="chkCluster" checked> <b>Enable Clustering</b> (Merge simultaneous multi-pixel hits into one)</label>
    </div>
    <div style="border-top: 1px solid var(--border); padding-top: 4px; line-height:1.3; font-size:12px;" id="effDisplay">
      Efficiency: ---
    </div>
  </fieldset>

  <!-- Event & Pixel Selection -->
  <fieldset style="border-color:#0d9488; background:rgba(13,148,136,0.05);">
    <legend style="color:#0d9488; font-weight:bold;">Event & Pixel Selection</legend>
    <div style="display: grid; grid-template-columns: 1fr 1fr; gap: 16px;">
      <div>
        <label><input type="checkbox" id="chkAllEv" checked> <b>Sum All Events</b></label>
        <div id="evControls" style="opacity: 0.4; pointer-events: none; margin-top: 8px; display:flex; flex-direction:column; gap:6px;">
          <div>
            eventID: <input type="number" id="evNum" min="0" max="0" value="0">
          </div>
          <div style="display:flex; align-items:center; gap:4px; font-size:12px;">
            ID: <input type="range" id="evSlider" min="0" max="0" value="0" style="flex:1;">
          </div>
        </div>
      </div>
      <div>
        <label><input type="checkbox" id="chkAllPix" checked> <b>Sum All Pixels</b></label>
        <div id="pixControls" style="opacity: 0.4; pointer-events: none; margin-top: 8px; display:flex; flex-direction:column; gap:6px;">
          <div>
            pixelID (0-319): <input type="number" id="pixNum" min="0" max="319" value="0">
          </div>
          <div style="display:flex; align-items:center; gap:4px; font-size:12px;">
            ID: <input type="range" id="pixSlider" min="0" max="319" value="0" style="flex:1;">
          </div>
        </div>
      </div>
    </div>
  </fieldset>
</div>

<div class="main">
  <div>
    <div class="views">
      <div class="vp" id="vp0"><canvas id="cv0"></canvas></div>
      <div class="vp" id="vp1" style="display:none"><canvas id="cv1"></canvas></div>
    </div>
    <div class="legend">
      <span class="swatch"></span><span>0 (No Hit)</span>
      <span>1</span><div class="bar" id="bar"></div><span id="lgmax">Max</span>
      <span id="lgnote"></span>
    </div>
    <div class="note">3D: Drag to rotate / Wheel to zoom / Double-click to reset view / Hover over pixels for details.</div>
  </div>
  
  <div style="display:flex; flex-direction:column; gap:12px;">
    <!-- Energy Histogram -->
    <div class="panel">
      <div style="display:flex; justify-content:space-between; align-items:flex-end; margin-bottom:6px;">
        <div style="font-weight:bold;">Energy Distribution</div>
        <div style="font-size:12px;">
          <label style="margin-right:8px;">Bin Width: <input type="number" id="binw" min="0.1" step="0.1" value="0.1" style="width:64px;"> MeV</label>
          <button id="btnLogY" style="margin:0;">Y-axis: Log</button>
        </div>
      </div>
      <canvas id="hist"></canvas>
      <div class="stats" id="stats"></div>

      <fieldset style="border-color:#5f6b7a; background:rgba(95,107,122,0.05); margin-top:8px;">
        <legend style="color:#5f6b7a; font-weight:bold;">Energy Range Filter</legend>
        <div style="display:flex; align-items:center; flex-wrap:wrap; gap:8px;">
          <div>
            <input type="number" id="emin" min="0" step="0.25" style="width:64px;"> -
            <input type="number" id="emax" min="0" step="0.25" style="width:64px;"> MeV
          </div>
          <div>
            <button id="btnAll">All Range</button>
            <button id="btnDec">Decay Range</button>
            <button id="btnImp">Implantation Range</button>
            <button id="btnZoom">Zoom Selection</button>
          </div>
        </div>
      </fieldset>

      <div class="note" style="margin-top:6px;">* Drag on the histogram to select energy range / Wheel to zoom X-axis / Dashed line is threshold.</div>
    </div>

    <!-- Time Histogram -->
    <div class="panel">
      <div style="display:flex; justify-content:space-between; align-items:flex-end; margin-bottom:6px;">
        <div style="font-weight:bold;">Time Distribution</div>
        <div style="font-size:12px;">
          <button id="btnLogXTime" style="margin:0 4px 0 0;">X-axis: Linear</button>
          <button id="btnLogYTime" style="margin:0;">Y-axis: Log</button>
        </div>
      </div>
      <canvas id="histTime"></canvas>
      
      <fieldset style="border-color:#e8710a; background:rgba(232,113,10,0.05); margin-top:12px;">
        <legend style="color:#e8710a; font-weight:bold;">Time Filter & Playback</legend>
        <div style="display:flex; align-items:center; flex-wrap:wrap; gap:8px;">
          <div>
            Time Window:
            <input type="number" id="tmin" step="0.1"> -
            <input type="number" id="tmax" step="0.1"> µs
          </div>
          <button id="btnAllTime">Reset Time</button>
          <button id="btnZoomTime">Zoom Selection</button>
          <span class="note" id="timeInfo" style="margin:0;"></span>
        </div>
        <div style="margin-top:8px; display:flex; align-items:center; gap:8px;">
          <button id="btnPlayTime" style="width:60px; font-weight:bold; color:#e8710a;">Play</button>
          <input type="range" id="timeSlider" min="0" max="1000" value="1000" style="flex:1; cursor:pointer;">
          <span style="font-variant-numeric: tabular-nums; width: 140px; text-align:right;">
            Current: <b id="timeCurrentDisplay" style="color:#e8710a;">0.00</b> µs
          </span>
        </div>
        <div class="note" style="margin-top:6px;">* Drag on the histogram to select time range / Wheel to zoom time axis. Slider moves on a logarithmic scale.</div>
      </fieldset>
    </div>
  </div>
</div>
<div class="note" id="caveat" style="margin-top:10px;">
  * Hits with energy &ge; threshold are classified as 'Implantation' (including decay energy in the same pixel). Hits &lt; threshold are 'Decay'.<br>
  * When a specific particle is selected, counts and efficiency are strictly based on hits from that particle.
</div>
<div id="tip"></div>

<script>
(function(){
'use strict';
var DATA = /*DATA_JSON*/null;

var G = DATA.geom;
var EV = DATA.ev, PIX = DATA.pix, EN = DATA.e, T = DATA.t, PDG = DATA.pdg;
var N = PIX.length;
var NPIX = G.nPix, NPF = NPIX * NPIX, NFACE = 5, NTOT = NFACE * NPF;
var FACE_NAMES = ['+z (Back)', '+x (Side)', '-x (Side)', '+y (Side)', '-y (Side)'];
var VH = 520;
var HH = 220;
var HL = 64, HR = 12, HT = 14, HB = 34;
var DEAD = '#b9bfc8';

function $(id){ return document.getElementById(id); }
function clamp(v, a, b){ return Math.max(a, Math.min(b, v)); }
function css(name){ return getComputedStyle(document.documentElement).getPropertyValue(name).trim(); }

function fmt(v){ 
  if (v === 0) return "0";
  var absV = Math.abs(v);
  if (absV >= 10000 || (absV > 0 && absV < 0.001)) {
    return v.toExponential(1).replace('e', 'E').replace('+', '+'); 
  }
  return (absV >= 100 || v === Math.round(v)) ? String(Math.round(v * 100) / 100) : v.toFixed(2); 
}

// ---- Data Ranges ----
var rawEmax = 0, MAXEV = 0, rawTmin = Infinity, rawTmax = -Infinity, i;
for (i = 0; i < N; i++) { 
  if (EN[i] > rawEmax) rawEmax = EN[i]; 
  if (EV[i] > MAXEV) MAXEV = EV[i]; 
  if (T[i] < rawTmin) rawTmin = T[i];
  if (T[i] > rawTmax) rawTmax = T[i];
}
if (!(rawEmax > 0)) rawEmax = 1;
if (rawTmin === Infinity) { rawTmin = 0; rawTmax = 100; }

// Add 5% margin to ranges for better visibility
var eMargin = rawEmax * 0.05; 
rawEmax += eMargin;

var tMargin = (rawTmax - rawTmin) * 0.05; 
if (tMargin <= 0) tMargin = 1.0;
rawTmin = Math.max(0, rawTmin - tMargin); 
rawTmax = rawTmax + tMargin;

var BW = DATA.binWidth || 0.1;
$('binw').value = BW;
var EMAX = rawEmax;
var NB = 1;

$('evSlider').max = MAXEV;
$('evNum').max = MAXEV;
$('pixNum').max = NTOT - 1;
$('pixSlider').max = NTOT - 1;

var S = {
  ptype: 'all', 
  clustering: true,
  showAllEv: true, evID: 0,
  showAllPix: true, pixID: 0,
  mode: 'all', thr: 100, emin: 0, emax: EMAX, vmin: 0, vmax: EMAX,
  tmin: rawTmin, tmax: rawTmax, tcurrent: rawTmax,
  vtmin: rawTmin, vtmax: rawTmax, 
  logxTime: false, logyTime: true,
  yaw: 190 * Math.PI / 180, pitch: 10 * Math.PI / 180, zoom: 1,
  faceOn: [true, true, true, true, true], alpha: 1, logc: false, logy: true
};
var YAW0 = S.yaw, PITCH0 = S.pitch;
var VIEWS = {'in': [190, 10], 'obl': [155, 22], 'back': [-25, 20]};

var T_EPS = 1e-3;
function timeToSlider(t) {
  var lmin = Math.log(S.tmin + T_EPS);
  var lmax = Math.log(S.tmax + T_EPS);
  if (lmax <= lmin) return 1000;
  var lt = Math.log(t + T_EPS);
  var v = (lt - lmin) / (lmax - lmin) * 1000;
  return clamp(v, 0, 1000);
}
function sliderToTime(v) {
  var lmin = Math.log(S.tmin + T_EPS);
  var lmax = Math.log(S.tmax + T_EPS);
  if (lmax <= lmin) return S.tmax;
  var lt = lmin + (v / 1000) * (lmax - lmin);
  return clamp(Math.exp(lt) - T_EPS, S.tmin, S.tmax);
}

// Particle ID Helper
function getParticleCategory(pdg) {
  if (pdg === 1000020040) return 'alpha';
  if (pdg === 22) return 'gamma';
  if (pdg === 11 || pdg === -11) return 'electron';
  if (pdg === 1000862040) return 'heavy'; // 2204Rn Ion
  return 'other';
}

function matchParticleFilter(pdg) {
  if (S.ptype === 'all') return true;
  var cat = getParticleCategory(pdg);
  return cat === S.ptype;
}

function quad(c, a, b, h){
  var s = [[-1,-1],[1,-1],[1,1],[-1,1]], out = [], k;
  for (k = 0; k < 4; k++) {
    var p = s[k][0], q = s[k][1];
    out.push([c[0] + (a[0]*p + b[0]*q)*h, c[1] + (a[1]*p + b[1]*q)*h, c[2] + (a[2]*p + b[2]*q)*h]);
  }
  return out;
}
function buildElements(){
  var off = NPIX * G.pitch / 2 + G.thick / 2;
  var els = [], id;
  for (id = 0; id < NTOT; id++) {
    var face = Math.floor(id / NPF), r = id % NPF;
    var ix = Math.floor(r / NPIX), iy = r % NPIX;
    var u = (ix - (NPIX - 1) / 2) * G.pitch, v = (iy - (NPIX - 1) / 2) * G.pitch;
    var c, a, b;
    if (face === 0)      { c = [u, v, off];  a = [1,0,0]; b = [0,1,0]; }
    else if (face === 1) { c = [off, u, v];  a = [0,1,0]; b = [0,0,1]; }
    else if (face === 2) { c = [-off, u, v]; a = [0,1,0]; b = [0,0,1]; }
    else if (face === 3) { c = [u, off, v];  a = [1,0,0]; b = [0,0,1]; }
    else                 { c = [u, -off, v]; a = [1,0,0]; b = [0,0,1]; }
    els.push({id: id, face: face, ix: ix, iy: iy, c: c,
              ce: quad(c, a, b, G.size / 2), cs: quad(c, a, b, G.sens / 2)});
  }
  return els;
}
var ELS = buildElements();

var C = {imp: new Int32Array(NTOT), dec: new Int32Array(NTOT), detCounts: 0};
var EVSEEN = {imp: new Uint8Array(MAXEV + 1), dec: new Uint8Array(MAXEV + 1)};
var HIST_IMP = null, HIST_DEC = null;

function computeCounts(){
  C.imp.fill(0); C.dec.fill(0); C.detCounts = 0;
  EVSEEN.imp.fill(0); EVSEEN.dec.fill(0);
  
  var uniqueHits = {}; 

  var k;
  for (k = 0; k < N; k++) {
    var pdgCode = PDG ? PDG[k] : 0;
    if (!matchParticleFilter(pdgCode)) continue;

    if (!S.showAllEv && EV[k] !== S.evID) continue;
    var p = PIX[k];
    if (!S.showAllPix && p !== S.pixID) continue;

    var e = EN[k], t = T[k];
    if (e < S.emin || e > S.emax) continue;
    if (t < S.tmin || t > S.tcurrent) continue; 
    
    if (p < 0 || p >= NTOT) continue;

    // Clustering: merge hits from the same particle, event, and time (0.01us resolution)
    if (S.clustering) {
      var tKey = Math.floor(t * 100); 
      var uniqKey = EV[k] + "_" + pdgCode + "_" + tKey;
      if (!uniqueHits[uniqKey]) {
        uniqueHits[uniqKey] = true;
        C.detCounts++; 
      }
    } else {
      C.detCounts++;
    }

    if (e >= S.thr) { C.imp[p]++; EVSEEN.imp[EV[k]] = 1; }
    else            { C.dec[p]++; EVSEEN.dec[EV[k]] = 1; }
  }
}

function buildHist(){
  BW = Math.max(0.01, parseFloat($('binw').value) || DATA.binWidth || 0.1);
  EMAX = Math.ceil(rawEmax / BW) * BW;
  NB = Math.round(EMAX / BW) + 1;
  HIST_IMP = new Float64Array(NB); HIST_DEC = new Float64Array(NB);
  var k;
  for (k = 0; k < N; k++) {
    var pdgCode = PDG ? PDG[k] : 0;
    if (!matchParticleFilter(pdgCode)) continue;

    if (!S.showAllEv && EV[k] !== S.evID) continue;
    if (!S.showAllPix && PIX[k] !== S.pixID) continue;
    
    var t = T[k];
    if (t < S.tmin || t > S.tcurrent) continue; 
    
    var b = Math.min(NB - 1, Math.max(0, Math.floor(EN[k] / BW)));
    if (EN[k] >= S.thr) HIST_IMP[b]++; else HIST_DEC[b]++;
  }
}
function countFor(cat, id){
  if (cat === 'imp') return C.imp[id];
  if (cat === 'dec') return C.dec[id];
  return C.imp[id] + C.dec[id];
}
function maxCountFor(cats){
  var m = 0, id, j;
  for (id = 0; id < NTOT; id++) for (j = 0; j < cats.length; j++) {
    var v = countFor(cats[j], id);
    if (v > m) m = v;
  }
  return m;
}

function colorOf(n, max){
  if (n <= 0) return '#ffffff';
  var t = 0;
  if (max > 1) t = S.logc ? Math.log(n) / Math.log(max) : (n - 1) / (max - 1);
  t = clamp(t, 0, 1);
  return 'hsl(' + (240 * (1 - t)).toFixed(1) + ',90%,50%)';
}

function fit(cv, cssH){
  var dpr = window.devicePixelRatio || 1;
  var w = Math.max(120, Math.floor(cv.getBoundingClientRect().width));
  var pw = Math.round(w * dpr), ph = Math.round(cssH * dpr);
  if (cv.width !== pw || cv.height !== ph) { cv.width = pw; cv.height = ph; }
  cv.style.height = cssH + 'px';
  var ctx = cv.getContext('2d');
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  return {ctx: ctx, w: w, h: cssH};
}
function path(ctx, pts){
  ctx.beginPath(); ctx.moveTo(pts[0][0], pts[0][1]);
  for (var k = 1; k < pts.length; k++) ctx.lineTo(pts[k][0], pts[k][1]);
  ctx.closePath();
}
function inPoly(p, x, y){
  var c = false, a, b;
  for (a = 0, b = p.length - 1; a < p.length; b = a++) {
    if (((p[a][1] > y) !== (p[b][1] > y)) &&
        (x < (p[b][0] - p[a][0]) * (y - p[a][1]) / (p[b][1] - p[a][1]) + p[a][0])) c = !c;
  }
  return c;
}

var panels = [
  {cv: $('cv0'), cat: 'all', label: '', hits: []},
  {cv: $('cv1'), cat: 'dec', label: '', hits: []}
];
function makeView(W, H){
  var cy = Math.cos(S.yaw), sy = Math.sin(S.yaw), cp = Math.cos(S.pitch), sp = Math.sin(S.pitch);
  var D = 300, F = 300, scale = Math.min(W, H) / 130 * S.zoom;
  function rot(p){
    var x1 = p[0]*cy + p[2]*sy, z1 = -p[0]*sy + p[2]*cy, y1 = p[1];
    return [x1, y1*cp - z1*sp, y1*sp + z1*cp];
  }
  function proj(p){
    var r = rot(p), k = F / (D - r[2]);
    return [W/2 + r[0]*k*scale, H/2 - r[1]*k*scale, r[2]];
  }
  return {rot: rot, proj: proj};
}
function drawPanel(P, max){
  var f = fit(P.cv, VH), ctx = f.ctx, W = f.w, H = f.h;
  ctx.globalAlpha = 1;
  ctx.fillStyle = css('--cbg'); ctx.fillRect(0, 0, W, H);
  var V = makeView(W, H), items = [], k;
  for (k = 0; k < ELS.length; k++) {
    if (!S.faceOn[ELS[k].face]) continue;
    items.push({el: ELS[k], z: V.rot(ELS[k].c)[2]});
  }
  items.sort(function(a, b){ return a.z - b.z; });
  P.hits = [];
  ctx.lineJoin = 'round';
  for (k = 0; k < items.length; k++) {
    var el = items[k].el;
    var pe = el.ce.map(V.proj), ps = el.cs.map(V.proj);
    var n = countFor(P.cat, el.id);
    ctx.globalAlpha = S.alpha;
    path(ctx, pe); ctx.fillStyle = DEAD; ctx.fill();
    path(ctx, ps); ctx.fillStyle = colorOf(n, max); ctx.fill();
    ctx.lineWidth = 0.6; ctx.strokeStyle = 'rgba(60,66,75,0.55)';
    path(ctx, pe); ctx.stroke();
    P.hits.push({el: el, pe: pe});
  }
  ctx.globalAlpha = 1;
  var fg = css('--fg');

  var half = NPIX * G.pitch / 2;
  var a = V.proj([0, 0, -half - 30]), b = V.proj([0, 0, -half - 2]);
  ctx.strokeStyle = fg; ctx.fillStyle = fg; ctx.lineWidth = 2;
  ctx.beginPath(); ctx.moveTo(a[0], a[1]); ctx.lineTo(b[0], b[1]); ctx.stroke();
  var ang = Math.atan2(b[1] - a[1], b[0] - a[0]);
  ctx.beginPath(); ctx.moveTo(b[0], b[1]);
  ctx.lineTo(b[0] - 9*Math.cos(ang - 0.4), b[1] - 9*Math.sin(ang - 0.4));
  ctx.lineTo(b[0] - 9*Math.cos(ang + 0.4), b[1] - 9*Math.sin(ang + 0.4));
  ctx.closePath(); ctx.fill();
  ctx.font = '12px system-ui, sans-serif';
  ctx.fillText('Beam', a[0] + 6, a[1] - 6);

  var ax = [['x', [1,0,0], '#d1382b'], ['y', [0,1,0], '#2c9a3f'], ['z', [0,0,1], '#2b6fd6']];
  var ox = 46, oy = H - 40, q;
  for (q = 0; q < 3; q++) {
    var r = V.rot(ax[q][1]);
    ctx.strokeStyle = ax[q][2]; ctx.fillStyle = ax[q][2]; ctx.lineWidth = 2;
    ctx.beginPath(); ctx.moveTo(ox, oy); ctx.lineTo(ox + r[0]*28, oy - r[1]*28); ctx.stroke();
    ctx.fillText(ax[q][0], ox + r[0]*34 - 3, oy - r[1]*34 + 4);
  }

  ctx.fillStyle = fg; ctx.font = '13px system-ui, sans-serif';
  ctx.fillText(P.label, 10, 20);
}
function draw3D(){
  var split = S.mode === 'split';
  $('vp1').style.display = split ? 'block' : 'none';
  
  var labEv = S.showAllEv ? '' : ' (Event ' + S.evID + ')';
  var lab = {
    imp: 'Implantation' + labEv,
    dec: 'Decay' + labEv,
    all: 'Both (Sum)' + labEv
  };
  panels[0].cat = split ? 'imp' : S.mode;
  panels[1].cat = 'dec';
  panels[0].label = lab[panels[0].cat];
  panels[1].label = lab.dec;
  var max = maxCountFor(split ? ['imp', 'dec'] : [panels[0].cat]);
  drawPanel(panels[0], max);
  if (split) drawPanel(panels[1], max);
  updateLegend(max);
}
function updateLegend(max){
  var stops = [], k;
  for (k = 0; k <= 8; k++) stops.push('hsl(' + (240 * (1 - k / 8)) + ',90%,50%)');
  $('bar').style.background = 'linear-gradient(to right,' + stops.join(',') + ')';
  $('lgmax').textContent = max > 0 ? String(max) : '(N/A)';
  $('lgnote').textContent = S.logc ? '(Log Scale)' : '(Linear Scale)';
}

var tip = $('tip');
function showTip(P, e){
  var r = P.cv.getBoundingClientRect(), x = e.clientX - r.left, y = e.clientY - r.top, h = null, k;
  for (k = P.hits.length - 1; k >= 0; k--) { if (inPoly(P.hits[k].pe, x, y)) { h = P.hits[k]; break; } }
  if (!h) { tip.style.display = 'none'; return; }
  var el = h.el;
  tip.textContent =
    'pixelID ' + el.id + '\n' + FACE_NAMES[el.face] + '  ix=' + el.ix + ', iy=' + el.iy + '\n' +
    'Center (' + el.c.map(function(v){ return v.toFixed(1); }).join(', ') + ') mm\n' +
    'Counts in current view\n  Implantation: ' + C.imp[el.id] + ' / Decay: ' + C.dec[el.id];
  tip.style.left = (e.clientX + 14) + 'px';
  tip.style.top = (e.clientY + 14) + 'px';
  tip.style.display = 'block';
}

function niceStep(range, n){
  var raw = range / n, p = Math.pow(10, Math.floor(Math.log10(raw))), m = raw / p;
  return (m < 1.5 ? 1 : m < 3 ? 2 : m < 7 ? 5 : 10) * p;
}

function drawHist(){
  var cv = $('hist'), f = fit(cv, HH), ctx = f.ctx, W = f.w, H = f.h;
  var pw = W - HL - HR, ph = H - HT - HB;
  ctx.globalAlpha = 1;
  ctx.fillStyle = css('--cbg'); ctx.fillRect(0, 0, W, H);
  var range = S.vmax - S.vmin;
  var k = 1; 
  var gw = k * BW;
  var g0 = Math.floor(S.vmin / gw), g1 = Math.ceil(S.vmax / gw);
  var bars = [], ymax = 1, g, j;
  for (g = g0; g < g1; g++) {
    var a = 0, b = 0, i0 = g * k;
    for (j = i0; j < i0 + k; j++) if (j >= 0 && j < NB) { a += HIST_IMP[j]; b += HIST_DEC[j]; }
    if (a > 0 || b > 0) { bars.push([g, a, b]); ymax = Math.max(ymax, a, b); }
  }
  var top = S.logy ? ymax * 1.5 : ymax * 1.08;
  var lo = Math.log10(0.5), hi = Math.log10(top);
  function yOf(c){
    if (c <= 0) return 0;
    var t = S.logy ? (Math.log10(c) - lo) / (hi - lo) : c / top;
    return Math.max(1, t * ph);
  }
  function xOf(e){ return HL + (e - S.vmin) / range * pw; }
  var fg = css('--fg'), muted = css('--muted'), cImp = css('--imp'), cDec = css('--dec');

  ctx.font = '11px system-ui, sans-serif';
  ctx.lineWidth = 1;
  
  var yt = [], v;
  if (S.logy) { for (v = 1; v <= top; v *= 10) yt.push(v); }
  else { var ys = niceStep(top, 4); for (v = 0; v <= top; v += ys) yt.push(v); }
  ctx.textAlign = 'right'; ctx.textBaseline = 'middle';
  for (j = 0; j < yt.length; j++) {
    var yy = HT + ph - (yt[j] === 0 ? 0 : yOf(yt[j]));
    ctx.strokeStyle = 'rgba(128,136,148,0.28)';
    ctx.beginPath(); ctx.moveTo(HL, yy); ctx.lineTo(HL + pw, yy); ctx.stroke();
    ctx.fillStyle = muted; ctx.fillText(fmt(yt[j]), HL - 6, yy);
  }
  
  var xs = niceStep(range, 7), x0 = Math.ceil(S.vmin / xs) * xs;
  ctx.textAlign = 'center'; ctx.textBaseline = 'top';
  for (v = x0; v <= S.vmax + 1e-9; v += xs) {
    var xx = xOf(v);
    ctx.strokeStyle = 'rgba(128,136,148,0.28)';
    ctx.beginPath(); ctx.moveTo(xx, HT); ctx.lineTo(xx, HT + ph); ctx.stroke();
    ctx.fillStyle = muted; ctx.fillText(fmt(v), xx, HT + ph + 5);
  }

  ctx.save();
  ctx.beginPath(); ctx.rect(HL, HT, pw, ph); ctx.clip();
  var sx0 = xOf(S.emin), sx1 = xOf(S.emax);
  var partial = (S.emin > 0 || S.emax < EMAX);
  if (partial) { ctx.fillStyle = css('--sel'); ctx.fillRect(sx0, HT, Math.max(1, sx1 - sx0), ph); }
  for (j = 0; j < bars.length; j++) {
    var bx0 = xOf(bars[j][0] * gw), bx1 = xOf((bars[j][0] + 1) * gw), bw = Math.max(1, bx1 - bx0 - 0.5);
    if (bars[j][2] > 0) { ctx.fillStyle = cDec; ctx.fillRect(bx0, HT + ph - yOf(bars[j][2]), bw, yOf(bars[j][2])); }
    if (bars[j][1] > 0) { ctx.fillStyle = cImp; ctx.fillRect(bx0, HT + ph - yOf(bars[j][1]), bw, yOf(bars[j][1])); }
  }
  if (partial) {
    ctx.strokeStyle = 'rgba(70,110,255,0.9)'; ctx.lineWidth = 1.2;
    ctx.beginPath(); ctx.moveTo(sx0, HT); ctx.lineTo(sx0, HT + ph); ctx.moveTo(sx1, HT); ctx.lineTo(sx1, HT + ph); ctx.stroke();
  }
  var tx = xOf(S.thr);
  ctx.setLineDash([5, 4]); ctx.strokeStyle = fg; ctx.lineWidth = 1;
  ctx.beginPath(); ctx.moveTo(tx, HT); ctx.lineTo(tx, HT + ph); ctx.stroke(); ctx.setLineDash([]);
  ctx.restore();

  ctx.strokeStyle = muted; ctx.lineWidth = 1; ctx.strokeRect(HL, HT, pw, ph);
  ctx.fillStyle = fg; ctx.textAlign = 'center'; ctx.textBaseline = 'alphabetic';
  ctx.font = '12px system-ui, sans-serif';
  ctx.fillText('Energy [MeV]', HL + pw / 2, H - 6);
  
  ctx.save();
  ctx.translate(16, HT + ph / 2);
  ctx.rotate(-Math.PI / 2);
  ctx.textAlign = 'center';
  ctx.fillText('Counts' + (S.logy ? ' (Log)' : ''), 0, 0);
  ctx.restore();

  ctx.textAlign = 'right';
  ctx.fillStyle = cImp; ctx.fillText('■ Implantation', HL + pw - 78, HT + 14);
  ctx.fillStyle = cDec; ctx.fillText('■ Decay', HL + pw - 8, HT + 14);
}

function drawTimeHist() {
  var cv = $('histTime'), f = fit(cv, HH), ctx = f.ctx, W = f.w, H = f.h;
  var pw = W - HL - HR, ph = H - HT - HB;
  ctx.globalAlpha = 1;
  ctx.fillStyle = css('--cbg'); ctx.fillRect(0, 0, W, H);
  
  var tmin_view = S.vtmin;
  var tmax_view = Math.max(S.vtmax, S.vtmin + 1e-9);
  
  var logX = S.logxTime;
  var LMIN = Math.log10(tmin_view + T_EPS);
  var LMAX = Math.log10(tmax_view + T_EPS);
  
  function xOf(t) {
    if (logX) {
      return HL + (Math.log10(t + T_EPS) - LMIN) / (LMAX - LMIN) * pw;
    } else {
      return HL + (t - tmin_view) / (tmax_view - tmin_view) * pw;
    }
  }
  
  var binsI = new Float64Array(Math.ceil(pw));
  var binsD = new Float64Array(Math.ceil(pw));
  var ymax = 1;
  
  for (var k = 0; k < N; k++) {
    var pdgCode = PDG ? PDG[k] : 0;
    if (!matchParticleFilter(pdgCode)) continue;

    if (!S.showAllEv && EV[k] !== S.evID) continue;
    if (!S.showAllPix && PIX[k] !== S.pixID) continue;
    
    var e = EN[k];
    if (e < S.emin || e > S.emax) continue;
    
    var t = T[k];
    var xpx = Math.floor(xOf(t) - HL);
    if (xpx >= 0 && xpx < pw) {
      if (e >= S.thr) {
        binsI[xpx]++;
        if (binsI[xpx] > ymax) ymax = binsI[xpx];
      } else {
        binsD[xpx]++;
        if (binsD[xpx] > ymax) ymax = binsD[xpx];
      }
    }
  }
  
  var top = S.logyTime ? ymax * 1.5 : ymax * 1.08;
  var lo = Math.log10(0.5), hi = Math.log10(top);
  function yOf(c){
    if (c <= 0) return 0;
    var t = S.logyTime ? (Math.log10(c) - lo) / (hi - lo) : c / top;
    return Math.max(1, t * ph);
  }
  
  var fg = css('--fg'), muted = css('--muted'), cImp = css('--imp'), cDec = css('--dec');
  ctx.font = '11px system-ui, sans-serif';
  ctx.lineWidth = 1;
  
  var yt = [], v, j;
  if (S.logyTime) { for (v = 1; v <= top; v *= 10) yt.push(v); }
  else { var ys = niceStep(top, 4); for (v = 0; v <= top; v += ys) yt.push(v); }
  ctx.textAlign = 'right'; ctx.textBaseline = 'middle';
  for (j = 0; j < yt.length; j++) {
    var yy = HT + ph - (yt[j] === 0 ? 0 : yOf(yt[j]));
    ctx.strokeStyle = 'rgba(128,136,148,0.28)';
    ctx.beginPath(); ctx.moveTo(HL, yy); ctx.lineTo(HL + pw, yy); ctx.stroke();
    ctx.fillStyle = muted; ctx.fillText(fmt(yt[j]), HL - 6, yy);
  }
  
  ctx.textAlign = 'center'; ctx.textBaseline = 'top';
  if (logX) {
    for (var p = Math.floor(LMIN); p <= Math.ceil(LMAX); p++) {
      var val = Math.pow(10, p) - T_EPS;
      if (val < 0) val = 0;
      var xx = xOf(val);
      if (xx >= HL && xx <= HL + pw) {
        ctx.strokeStyle = 'rgba(128,136,148,0.28)';
        ctx.beginPath(); ctx.moveTo(xx, HT); ctx.lineTo(xx, HT + ph); ctx.stroke();
        ctx.fillStyle = muted; ctx.fillText(fmt(val), xx, HT + ph + 5);
      }
    }
  } else {
    var range = tmax_view - tmin_view;
    var xs = niceStep(range, 7), x0 = Math.ceil(tmin_view / xs) * xs;
    for (v = x0; v <= tmax_view + 1e-9; v += xs) {
      var xx = xOf(v);
      ctx.strokeStyle = 'rgba(128,136,148,0.28)';
      ctx.beginPath(); ctx.moveTo(xx, HT); ctx.lineTo(xx, HT + ph); ctx.stroke();
      ctx.fillStyle = muted; ctx.fillText(fmt(v), xx, HT + ph + 5);
    }
  }

  ctx.save();
  ctx.beginPath(); ctx.rect(HL, HT, pw, ph); ctx.clip();
  
  var sx0 = xOf(S.tmin), sx1 = xOf(S.tcurrent);
  if (sx0 > sx1) { var tmp = sx0; sx0 = sx1; sx1 = tmp; }
  ctx.fillStyle = css('--sel'); 
  ctx.fillRect(sx0, HT, Math.max(1, sx1 - sx0), ph);
  
  for (var px = 0; px < pw; px++) {
    var bx0 = HL + px, bw = 1.5;
    if (binsD[px] > 0) { ctx.fillStyle = cDec; ctx.fillRect(bx0, HT + ph - yOf(binsD[px]), bw, yOf(binsD[px])); }
    if (binsI[px] > 0) { ctx.fillStyle = cImp; ctx.fillRect(bx0, HT + ph - yOf(binsI[px]), bw, yOf(binsI[px])); }
  }
  
  ctx.strokeStyle = 'rgba(70,110,255,0.9)'; ctx.lineWidth = 1.2;
  ctx.beginPath(); ctx.moveTo(sx0, HT); ctx.lineTo(sx0, HT + ph); ctx.moveTo(sx1, HT); ctx.lineTo(sx1, HT + ph); ctx.stroke();
  ctx.restore();

  ctx.strokeStyle = muted; ctx.lineWidth = 1; ctx.strokeRect(HL, HT, pw, ph);
  ctx.fillStyle = fg; ctx.textAlign = 'center'; ctx.textBaseline = 'alphabetic';
  ctx.font = '12px system-ui, sans-serif';
  ctx.fillText('Time [µs]' + (S.logxTime ? ' (Log)' : ''), HL + pw / 2, H - 6);
  
  ctx.save();
  ctx.translate(16, HT + ph / 2);
  ctx.rotate(-Math.PI / 2);
  ctx.textAlign = 'center';
  ctx.fillText('Counts' + (S.logyTime ? ' (Log)' : ''), 0, 0);
  ctx.restore();

  ctx.textAlign = 'right';
  ctx.fillStyle = cImp; ctx.fillText('■ Implantation', HL + pw - 78, HT + 14);
  ctx.fillStyle = cDec; ctx.fillText('■ Decay', HL + pw - 8, HT + 14);
}

function updateStats(){
  var nImp = 0, nDec = 0, det = 0, id, mx = 0, evI = 0, evD = 0, evA = 0, k;
  for (id = 0; id < NTOT; id++) {
    nImp += C.imp[id]; nDec += C.dec[id];
    var t = C.imp[id] + C.dec[id];
    if (t > 0) det++;
    if (t > mx) mx = t;
  }
  for (k = 0; k <= MAXEV; k++) {
    if (EVSEEN.imp[k]) evI++;
    if (EVSEEN.dec[k]) evD++;
    if (EVSEEN.imp[k] || EVSEEN.dec[k]) evA++;
  }
  
  var evText = S.showAllEv 
    ? 'Hit Events: <b>' + evA + '</b> / ' + DATA.nEvents + ' (Implantation ' + evI + ', Decay ' + evD + ')'
    : 'Showing Event: <b>Event ' + S.evID + '</b>';

  var pixText = S.showAllPix 
    ? 'Hit Pixels: <b>' + det + '</b> / ' + NTOT + '<br>'
    : 'Showing Pixel: <b>pixelID ' + S.pixID + '</b> only<br>';

  $('stats').innerHTML =
    'Selected Energy: <b>' + fmt(S.emin) + ' - ' + fmt(S.emax) + ' MeV</b> / ' +
    'Time Window: <b>' + fmt(S.tmin) + ' - ' + fmt(S.tcurrent) + ' µs</b> <span style="color:#888;">(Max ' + fmt(S.tmax) + ' µs)</span><br>' +
    'Hits: Implantation <b>' + nImp + '</b> / Decay <b>' + nDec + '</b><br>' +
    pixText +
    evText;

  var ptype = S.ptype;
  var genTotal = 0;
  var pLabel = "";
  
  if (ptype === 'alpha') { 
    genTotal = DATA.nGenAlphas; 
    pLabel = "Alpha"; 
  }
  else if (ptype === 'gamma') { 
    genTotal = DATA.nGenGammas; 
    pLabel = "Gamma"; 
  }
  else if (ptype === 'electron') { 
    genTotal = DATA.nGenElectrons; 
    pLabel = "Beta (e±)"; 
  }
  else if (ptype === 'heavy') { 
    genTotal = DATA.nEvents; 
    pLabel = "Heavy Ion (Beam)"; 
  }

  if (ptype !== 'all') {
    var eff = genTotal > 0 ? (C.detCounts / genTotal * 100).toFixed(2) : "0.00";
    $('effDisplay').innerHTML = pLabel + ' Efficiency: <b>' + eff + '%</b><br><span style="font-size:11px;color:#888;">(Detected: ' + C.detCounts + ' / Generated: ' + genTotal + ')</span>';
  } else {
    $('effDisplay').innerHTML = 'Efficiency: Please select a particle';
  }
}

var pendRefresh = false, pend3D = false;
function refresh(){ computeCounts(); draw3D(); buildHist(); drawHist(); drawTimeHist(); updateStats(); }
function scheduleRefresh(){
  if (pendRefresh) return; pendRefresh = true;
  requestAnimationFrame(function(){ pendRefresh = false; refresh(); });
}
function scheduleDraw3D(){
  if (pend3D) return; pend3D = true;
  requestAnimationFrame(function(){ pend3D = false; draw3D(); });
}
function syncInputs(){
  $('emin').value = Math.round(S.emin * 1000) / 1000;
  $('emax').value = Math.round(S.emax * 1000) / 1000;
  $('tmin').value = Math.round(S.tmin * 10000) / 10000;
  $('tmax').value = Math.round(S.tmax * 10000) / 10000;
  $('timeSlider').value = timeToSlider(S.tcurrent);
  $('timeCurrentDisplay').textContent = fmt(S.tcurrent);
}

var playTimer = null;
var playStartMs = 0;
var playStartVal = 0;
var DURATION = 15000; 

function stopTimePlay() {
  if (playTimer) cancelAnimationFrame(playTimer);
  playTimer = null;
  $('btnPlayTime').textContent = 'Play';
}

function tickTimePlay(timestamp) {
  if (!playStartMs) playStartMs = timestamp;
  var elapsed = timestamp - playStartMs;
  
  var nextVal = playStartVal + (elapsed / DURATION) * 1000;
  if (nextVal >= 1000) {
    nextVal = 1000;
    $('timeSlider').value = nextVal;
    S.tcurrent = S.tmax;
    syncInputs();
    refresh();
    stopTimePlay();
    return;
  }
  
  $('timeSlider').value = nextVal;
  S.tcurrent = sliderToTime(nextVal);
  $('timeCurrentDisplay').textContent = fmt(S.tcurrent);
  refresh();
  playTimer = requestAnimationFrame(tickTimePlay);
}

$('btnPlayTime').addEventListener('click', function(){
  if (playTimer) {
    stopTimePlay(); 
  } else {
    var curVal = parseFloat($('timeSlider').value);
    if (curVal >= 1000) { curVal = 0; } 
    playStartVal = curVal;
    playStartMs = 0;
    this.textContent = 'Stop';
    playTimer = requestAnimationFrame(tickTimePlay);
  }
});

$('timeSlider').addEventListener('input', function(){
  stopTimePlay(); 
  S.tcurrent = sliderToTime(parseFloat(this.value));
  $('timeCurrentDisplay').textContent = fmt(S.tcurrent);
  scheduleRefresh();
});

Array.prototype.forEach.call(document.querySelectorAll('input[name=ptype]'), function(el){
  el.addEventListener('change', function(){ if (el.checked) { S.ptype = el.value; refresh(); } });
});

$('chkCluster').addEventListener('change', function(){
  S.clustering = this.checked;
  refresh();
});

$('chkAllEv').addEventListener('change', function(){
  S.showAllEv = this.checked;
  var ctl = $('evControls');
  ctl.style.opacity = S.showAllEv ? '0.4' : '1';
  ctl.style.pointerEvents = S.showAllEv ? 'none' : 'auto';
  refresh();
});
$('evSlider').addEventListener('input', function(){
  var v = parseInt(this.value, 10);
  $('evNum').value = v; S.evID = v; scheduleRefresh();
});
$('evNum').addEventListener('change', function(){
  var v = parseInt(this.value, 10);
  if (isNaN(v)) v = 0;
  v = clamp(v, 0, MAXEV);
  this.value = v; $('evSlider').value = v; S.evID = v; refresh();
});

$('chkAllPix').addEventListener('change', function(){
  S.showAllPix = this.checked;
  var ctl = $('pixControls');
  ctl.style.opacity = S.showAllPix ? '0.4' : '1';
  ctl.style.pointerEvents = S.showAllPix ? 'none' : 'auto';
  refresh();
});
$('pixSlider').addEventListener('input', function(){
  var v = parseInt(this.value, 10);
  $('pixNum').value = v; S.pixID = v; scheduleRefresh();
});
$('pixNum').addEventListener('change', function(){
  var v = parseInt(this.value, 10);
  if (isNaN(v)) v = 0;
  v = clamp(v, 0, NTOT - 1);
  this.value = v; $('pixSlider').value = v; S.pixID = v; refresh();
});

Array.prototype.forEach.call(document.querySelectorAll('input[name=mode]'), function(el){
  el.addEventListener('change', function(){ if (el.checked) { S.mode = el.value; draw3D(); } });
});
$('thr').addEventListener('change', function(){
  S.thr = Math.max(0, parseFloat($('thr').value) || 0); buildHist(); refresh();
});
$('binw').addEventListener('change', function(){
  var oldEmax = EMAX;
  buildHist();
  if (S.emax >= oldEmax) S.emax = EMAX;
  if (S.vmax >= oldEmax) S.vmax = EMAX;
  syncInputs();
  refresh();
});
$('btnLogY').addEventListener('click', function(){
  S.logy = !S.logy;
  this.textContent = 'Y-axis: ' + (S.logy ? 'Log' : 'Linear');
  drawHist();
});
$('btnLogXTime').addEventListener('click', function(){
  S.logxTime = !S.logxTime;
  this.textContent = 'X-axis: ' + (S.logxTime ? 'Log' : 'Linear');
  drawTimeHist();
});
$('btnLogYTime').addEventListener('click', function(){
  S.logyTime = !S.logyTime;
  this.textContent = 'Y-axis: ' + (S.logyTime ? 'Log' : 'Linear');
  drawTimeHist();
});

function onRange(){
  var a = parseFloat($('emin').value), b = parseFloat($('emax').value);
  if (isNaN(a)) a = 0; if (isNaN(b)) b = EMAX;
  if (a > b) { var t = a; a = b; b = t; }
  S.emin = clamp(a, 0, EMAX); S.emax = clamp(b, 0, EMAX); syncInputs(); refresh();
}
function onTimeRange(){
  stopTimePlay();
  var a = parseFloat($('tmin').value), b = parseFloat($('tmax').value);
  if (isNaN(a)) a = rawTmin; if (isNaN(b)) b = rawTmax;
  if (a > b) { var t = a; a = b; b = t; }
  S.tmin = a; S.tmax = b;
  S.tcurrent = S.tmax; 
  syncInputs(); refresh();
}
$('emin').addEventListener('change', onRange);
$('emax').addEventListener('change', onRange);
$('tmin').addEventListener('change', onTimeRange);
$('tmax').addEventListener('change', onTimeRange);

$('btnAll').addEventListener('click', function(){
  S.emin = 0; S.emax = EMAX; S.vmin = 0; S.vmax = EMAX; syncInputs(); refresh();
});
$('btnDec').addEventListener('click', function(){ S.emin = 0; S.emax = Math.min(S.thr, EMAX); syncInputs(); refresh(); });
$('btnImp').addEventListener('click', function(){ S.emin = Math.min(S.thr, EMAX); S.emax = EMAX; syncInputs(); refresh(); });
$('btnZoom').addEventListener('click', function(){
  S.vmin = S.emin; S.vmax = Math.min(EMAX, Math.max(S.emax, S.emin + BW * 20)); refresh();
});

$('btnZoomTime').addEventListener('click', function(){
  S.vtmin = S.tmin; 
  S.vtmax = Math.max(S.tmax, S.tmin + 0.05); 
  refresh();
});

$('btnAllTime').addEventListener('click', function(){
  stopTimePlay();
  S.tmin = rawTmin; S.tmax = rawTmax; S.tcurrent = rawTmax; 
  S.vtmin = rawTmin; S.vtmax = rawTmax; 
  syncInputs(); refresh();
});

panels.forEach(function(P){
  var cv = P.cv, drag = null;
  cv.addEventListener('pointerdown', function(e){
    drag = {x: e.clientX, y: e.clientY}; cv.setPointerCapture(e.pointerId); tip.style.display = 'none';
  });
  cv.addEventListener('pointermove', function(e){
    if (drag) {
      S.yaw += (e.clientX - drag.x) * 0.008;
      S.pitch = clamp(S.pitch + (e.clientY - drag.y) * 0.008, -1.55, 1.55);
      drag = {x: e.clientX, y: e.clientY};
      scheduleDraw3D();
    } else showTip(P, e);
  });
  cv.addEventListener('pointerup', function(){ drag = null; });
  cv.addEventListener('pointerleave', function(){ tip.style.display = 'none'; });
  cv.addEventListener('wheel', function(e){
    e.preventDefault();
    S.zoom = clamp(S.zoom * Math.exp(-e.deltaY * 0.0012), 0.4, 8);
    scheduleDraw3D();
  }, {passive: false});
  cv.addEventListener('dblclick', function(){ S.yaw = YAW0; S.pitch = PITCH0; S.zoom = 1; scheduleDraw3D(); });
});

(function(){
  var cv = $('hist'), drag = null;
  function energyAt(e){
    var r = cv.getBoundingClientRect();
    return S.vmin + (e.clientX - r.left - HL) / (r.width - HL - HR) * (S.vmax - S.vmin);
  }
  cv.addEventListener('pointerdown', function(e){
    drag = energyAt(e); cv.setPointerCapture(e.pointerId);
  });
  cv.addEventListener('pointermove', function(e){
    if (drag === null) return;
    var a = clamp(Math.min(drag, energyAt(e)), 0, EMAX), b = clamp(Math.max(drag, energyAt(e)), 0, EMAX);
    S.emin = a; S.emax = b; syncInputs(); scheduleRefresh();
  });
  cv.addEventListener('pointerup', function(){ drag = null; });
  cv.addEventListener('wheel', function(e){
    e.preventDefault();
    var c = energyAt(e), f = e.deltaY > 0 ? 1.25 : 0.8;
    var nr = clamp((S.vmax - S.vmin) * f, BW * 20, EMAX);
    var rel = (c - S.vmin) / (S.vmax - S.vmin);
    var nmin = clamp(c - rel * nr, 0, EMAX - nr);
    S.vmin = nmin; S.vmax = nmin + nr; scheduleRefresh();
  }, {passive: false});
})();

(function(){
  var cv = $('histTime'), drag = null;
  function timeAt(e){
    var r = cv.getBoundingClientRect();
    var px = e.clientX - r.left - HL;
    var pw = r.width - HL - HR;
    var tmin_view = S.vtmin, tmax_view = S.vtmax; 
    if (S.logxTime) {
      var LMIN = Math.log10(tmin_view + T_EPS);
      var LMAX = Math.log10(tmax_view + T_EPS);
      var lt = LMIN + (px / pw) * (LMAX - LMIN);
      return Math.exp(lt * Math.LN10) - T_EPS;
    } else {
      return tmin_view + (px / pw) * (tmax_view - tmin_view);
    }
  }
  cv.addEventListener('pointerdown', function(e){
    stopTimePlay();
    drag = timeAt(e); cv.setPointerCapture(e.pointerId);
  });
  cv.addEventListener('pointermove', function(e){
    if (drag === null) return;
    var cur = timeAt(e);
    var a = clamp(Math.min(drag, cur), 0, rawTmax);
    var b = clamp(Math.max(drag, cur), 0, rawTmax);
    S.tmin = a; S.tmax = b; S.tcurrent = b; 
    syncInputs(); scheduleRefresh();
  });
  cv.addEventListener('pointerup', function(){ drag = null; });
  
  cv.addEventListener('wheel', function(e){
    e.preventDefault();
    var c = timeAt(e), f = e.deltaY > 0 ? 1.25 : 0.8;
    var nr = clamp((S.vtmax - S.vtmin) * f, 0.01, (rawTmax - rawTmin + 10) * 5);
    var rel = (c - S.vtmin) / (S.vtmax - S.vtmin);
    var nmin = c - rel * nr;
    S.vtmin = nmin; S.vtmax = nmin + nr; scheduleRefresh();
  }, {passive: false});
})();

Array.prototype.forEach.call(document.querySelectorAll('button[data-view]'), function(b){
  b.addEventListener('click', function(){
    var v = VIEWS[b.getAttribute('data-view')];
    S.yaw = v[0] * Math.PI / 180; S.pitch = v[1] * Math.PI / 180; S.zoom = 1; draw3D();
  });
});
Array.prototype.forEach.call(document.querySelectorAll('input.face'), function(el){
  el.addEventListener('change', function(){ S.faceOn[parseInt(el.value, 10)] = el.checked; draw3D(); });
});
$('alpha').addEventListener('input', function(){ S.alpha = parseInt($('alpha').value, 10) / 100; draw3D(); });
$('logc').addEventListener('change', function(){ S.logc = $('logc').checked; draw3D(); });
window.addEventListener('resize', function(){ draw3D(); drawHist(); drawTimeHist(); });
if (window.matchMedia) {
  var mq = window.matchMedia('(prefers-color-scheme: dark)');
  if (mq.addEventListener) mq.addEventListener('change', function(){ draw3D(); drawHist(); drawTimeHist(); });
}

$('info').textContent = 'File: ' + DATA.file + ' / Events: ' + DATA.nEvents +
  ' / Hit rows: ' + N + ' / Pixels: ' + NPIX + 'x' + NPIX + ' x 5 faces';

$('timeInfo').textContent = 'Recorded Time: Min ' + fmt(rawTmin) + ' µs to Max ' + fmt(rawTmax) + ' µs';

buildHist();
S.emax = EMAX;
S.vmax = EMAX;
$('btnLogY').textContent = 'Y-axis: ' + (S.logy ? 'Log' : 'Linear');
$('btnLogXTime').textContent = 'X-axis: ' + (S.logxTime ? 'Log' : 'Linear');
$('btnLogYTime').textContent = 'Y-axis: ' + (S.logyTime ? 'Log' : 'Linear');
syncInputs();
refresh();
})();
</script>
</body>
</html>
)HTML";

static std::string jsonEscape(const std::string& s)
{
  std::string out;
  for (char c : s) {
    if (c == '\\' || c == '"') {
      out += '\\';
      out += c;
    }
    else if (c == '\n') {
      out += "\\n";
    }
    else {
      out += c;
    }
  }
  return out;
}

static TTree* getTree(TFile* f, const char* name)
{
  TTree* t = dynamic_cast<TTree*>(f->Get(name));
  if (!t) t = dynamic_cast<TTree*>(f->Get((std::string("ntuples/") + name).c_str()));
  return t;
}

int makeSiViewer(const char* inFile = "SiDetector.root", const char* outFile = "SiViewer.html")
{
  std::unique_ptr<TFile> f(TFile::Open(inFile, "READ"));
  if (!f || f->IsZombie()) {
    std::cerr << "[makeSiViewer] Cannot open file: " << inFile << std::endl;
    return 1;
  }

  TTree* hits = getTree(f.get(), "hits");
  if (!hits) {
    std::cerr << "[makeSiViewer] Ntuple \"hits\" not found." << std::endl;
    return 1;
  }

  std::vector<int> evID, pixID, hitPdg;
  std::vector<double> edep, hitTime;
  {
    TTreeReader reader(hits);
    TTreeReaderValue<Int_t> rEvent(reader, "eventID");
    TTreeReaderValue<Int_t> rPixel(reader, "pixelID");
    TTreeReaderValue<Double_t> rEdep(reader, "edep");
    
    TTreeReaderValue<Double_t>* rTime = nullptr;
    if (hits->GetBranch("time")) {
      rTime = new TTreeReaderValue<Double_t>(reader, "time");
    }

    TTreeReaderValue<Int_t>* rPdg = nullptr;
    if (hits->GetBranch("pdg")) {
      rPdg = new TTreeReaderValue<Int_t>(reader, "pdg");
    }

    while (reader.Next()) {
      evID.push_back(*rEvent);
      pixID.push_back(*rPixel);
      edep.push_back(*rEdep);
      hitTime.push_back(rTime ? *(*rTime) : 0.0);
      hitPdg.push_back(rPdg ? *(*rPdg) : 0);
    }
    delete rTime;
    delete rPdg;
  }

  long long nEvents = 0;
  long long totalGenAlphas = 0;
  long long totalGenGammas = 0;
  long long totalGenElectrons = 0;
  long long totalGenHeavyIons = 0;

  if (TTree* ev = getTree(f.get(), "events")) {
    nEvents = ev->GetEntries();
    
    TTreeReader evReader(ev);
    TTreeReaderValue<Int_t>* rAlphas = ev->GetBranch("nGenAlphas") ? new TTreeReaderValue<Int_t>(evReader, "nGenAlphas") : nullptr;
    TTreeReaderValue<Int_t>* rGammas = ev->GetBranch("nGenGammas") ? new TTreeReaderValue<Int_t>(evReader, "nGenGammas") : nullptr;
    TTreeReaderValue<Int_t>* rElectrons = ev->GetBranch("nGenElectrons") ? new TTreeReaderValue<Int_t>(evReader, "nGenElectrons") : nullptr;
    TTreeReaderValue<Int_t>* rHeavyIons = ev->GetBranch("nGenHeavyIons") ? new TTreeReaderValue<Int_t>(evReader, "nGenHeavyIons") : nullptr;

    while (evReader.Next()) {
      if (rAlphas) totalGenAlphas += **rAlphas;
      if (rGammas) totalGenGammas += **rGammas;
      if (rElectrons) totalGenElectrons += **rElectrons;
      if (rHeavyIons) totalGenHeavyIons += **rHeavyIons;
    }
    delete rAlphas;
    delete rGammas;
    delete rElectrons;
    delete rHeavyIons;
  }
  else if (!evID.empty()) {
    nEvents = *std::max_element(evID.begin(), evID.end()) + 1;
  }

  std::ostringstream js;
  js << std::setprecision(7);
  js << "{\"file\":\"" << jsonEscape(inFile) << "\"";
  js << ",\"nEvents\":" << nEvents;
  js << ",\"nGenAlphas\":" << totalGenAlphas;
  js << ",\"nGenGammas\":" << totalGenGammas;
  js << ",\"nGenElectrons\":" << totalGenElectrons;
  js << ",\"nGenHeavyIons\":" << totalGenHeavyIons;
  js << ",\"binWidth\":" << kBinWidth;
  js << ",\"geom\":{\"nPix\":" << kNPix << ",\"pitch\":" << kPitch << ",\"size\":" << kPixSize
     << ",\"sens\":" << kSensSize << ",\"thick\":" << kPixThick << "}";
  js << ",\"ev\":[";
  for (size_t i = 0; i < evID.size(); ++i) js << (i ? "," : "") << evID[i];
  js << "],\"pix\":[";
  for (size_t i = 0; i < pixID.size(); ++i) js << (i ? "," : "") << pixID[i];
  js << "],\"e\":[";
  for (size_t i = 0; i < edep.size(); ++i) js << (i ? "," : "") << edep[i];
  js << "],\"t\":[";
  for (size_t i = 0; i < hitTime.size(); ++i) js << (i ? "," : "") << hitTime[i];
  js << "],\"pdg\":[";
  for (size_t i = 0; i < hitPdg.size(); ++i) js << (i ? "," : "") << hitPdg[i];
  js << "]}";

  std::string html(kHtml);
  const std::string key = "/*DATA_JSON*/null";
  const size_t pos = html.find(key);
  if (pos == std::string::npos) {
    std::cerr << "[makeSiViewer] Data insertion point not found in template." << std::endl;
    return 1;
  }
  html.replace(pos, key.size(), js.str());

  std::ofstream out(outFile, std::ios::binary);
  if (!out) {
    std::cerr << "[makeSiViewer] Cannot create output file: " << outFile << std::endl;
    return 1;
  }
  out << html;
  out.close();

  std::cout << "[makeSiViewer] Created " << outFile << " (Events: " << nEvents << ")" << std::endl;
  return 0;
}

#if !defined(__CLING__)
int main(int argc, char** argv)
{
  const char* in = (argc > 1) ? argv[1] : "SiDetector.root";
  const char* out = (argc > 2) ? argv[2] : "SiViewer.html";
  return makeSiViewer(in, out);
}
#endif