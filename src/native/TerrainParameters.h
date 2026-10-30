#pragma once
#include "Noise.h"
namespace nativegen {
// A Field owns one immutable recipe. Decode its hot scalar parameters once;
// sampling retains the same doubles and operation order as the JSON formulas.
struct ReliefLevelParameters {
 double cap,edgeBand,gain,reliefCap,reliefHi,reliefLo,pitch;
 explicit ReliefLevelParameters(J const&j):
     cap(j.at("cap").get<double>()),
     edgeBand(j.at("edgeBand").get<double>()),
     gain(j.at("gain").get<double>()),
     reliefCap(j.at("reliefCap").get<double>()),
     reliefHi(j.at("reliefHi").get<double>()),
     reliefLo(j.at("reliefLo").get<double>()),
     pitch(j.at("pitch").get<double>()){}
};
struct SecondaryReliefParameters {
 double amplitude,mountainHigh,mountainLow,octaves,offsetX,offsetZ,ridgeMean,rotationCos,rotationSin,warpBlocks,warpWavelength,wavelength;
 explicit SecondaryReliefParameters(J const&j):
     amplitude(j.at("amplitude").get<double>()),
     mountainHigh(j.at("mountainHigh").get<double>()),
     mountainLow(j.at("mountainLow").get<double>()),
     octaves(j.at("octaves").get<double>()),
     offsetX(j.at("offsetX").get<double>()),
     offsetZ(j.at("offsetZ").get<double>()),
     ridgeMean(j.at("ridgeMean").get<double>()),
     rotationCos(j.at("rotationCos").get<double>()),
     rotationSin(j.at("rotationSin").get<double>()),
     warpBlocks(j.at("warpBlocks").get<double>()),
     warpWavelength(j.at("warpWavelength").get<double>()),
     wavelength(j.at("wavelength").get<double>()){}
};
struct SpikeReliefParameters {
 double maxCut,prominenceHi,prominenceLo;
 explicit SpikeReliefParameters(J const&j):
     maxCut(j.at("maxCut").get<double>()),
     prominenceHi(j.at("prominenceHi").get<double>()),
     prominenceLo(j.at("prominenceLo").get<double>()){}
};
struct GiantReliefParameters {
 double centerX,centerZ,outerRadius;
 explicit GiantReliefParameters(J const&j):
     centerX(j.at("centerX").get<double>()),
     centerZ(j.at("centerZ").get<double>()),
     outerRadius(j.at("outerRadius").get<double>()){}
};
struct CliffParameters {
 double grid,seed,chance,minHeight,maxHeight,minSlope,maxSlope,maxShift,front,halfLength;
 explicit CliffParameters(J const&j):
     grid(j.at("grid").get<double>()),
     seed(j.at("seed").get<double>()),
     chance(j.at("chance").get<double>()),
     minHeight(j.at("minHeight").get<double>()),
     maxHeight(j.at("maxHeight").get<double>()),
     minSlope(j.at("minSlope").get<double>()),
     maxSlope(j.at("maxSlope").get<double>()),
     maxShift(j.at("maxShift").get<double>()),
     front(j.at("front").get<double>()),
     halfLength(j.at("halfLength").get<double>()){}
};
struct ReliefParameters {
 double flatHi,flatLo,highAmplitude,highOctaves,highPosHi,highPosLo,highReliefHi,highReliefLo,highRidgeMean,highWavelength,landFadeHi,landFadeLo,lowAmplitude,lowOctaves,lowWavelength,totalCap,waterFadeHi,waterFadeLo;
 std::array<ReliefLevelParameters,2> levels;
 SecondaryReliefParameters secondary;
 SpikeReliefParameters spike;
 explicit ReliefParameters(J const&j):
     flatHi(j.at("flatHi").get<double>()),
     flatLo(j.at("flatLo").get<double>()),
     highAmplitude(j.at("highAmplitude").get<double>()),
     highOctaves(j.at("highOctaves").get<double>()),
     highPosHi(j.at("highPosHi").get<double>()),
     highPosLo(j.at("highPosLo").get<double>()),
     highReliefHi(j.at("highReliefHi").get<double>()),
     highReliefLo(j.at("highReliefLo").get<double>()),
     highRidgeMean(j.at("highRidgeMean").get<double>()),
     highWavelength(j.at("highWavelength").get<double>()),
     landFadeHi(j.at("landFadeHi").get<double>()),
     landFadeLo(j.at("landFadeLo").get<double>()),
     lowAmplitude(j.at("lowAmplitude").get<double>()),
     lowOctaves(j.at("lowOctaves").get<double>()),
     lowWavelength(j.at("lowWavelength").get<double>()),
     totalCap(j.at("totalCap").get<double>()),
     waterFadeHi(j.at("waterFadeHi").get<double>()),
     waterFadeLo(j.at("waterFadeLo").get<double>()),
     levels{ReliefLevelParameters(j.at("levels").at(0)),
     ReliefLevelParameters(j.at("levels").at(1))},secondary(j.at("secondary")),
     spike(j.at("spike")){}
};
struct TerrainParameters {
 ReliefParameters relief;
 GiantReliefParameters giant;
 CliffParameters cliffs;
 double horizontalScale;
 int cliffRevision;
 explicit TerrainParameters(J const&c):
     relief(c.at("relief")),
     giant(c.at("giantForest")),
     cliffs(c.at("cliffs")),
     horizontalScale(c.at("epic").at("horizontalScale").get<double>()),
     cliffRevision(c.value("cliffRevision",0)){}
};
}
