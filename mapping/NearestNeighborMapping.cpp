/*
 * NearestNeighborMapping.cpp
 *
 *  Created on: Sep 28, 2015
 *      Author: scheufks
 */


#include "NearestNeighborMapping.hpp"
#include "NearestNeighborQuery.hpp"
#include <iostream>

NearestNeighborMapping::NearestNeighborMapping()
  :
_map()
{}

void NearestNeighborMapping::computeMapping
(
    int inputN,
    int outputN,
    std::vector<double>& inputCoords,
    std::vector<double>& outputCoords)
{
  _map.resize(outputN);
  for (int i=0; i < outputN; i++ ){
      double outCoord = outputCoords.at(i);
      NearestNeighborQuery query(outCoord);
      query.find(inputCoords);

      _map[i] = query.getClosestVertexID();
    }
  _isComputed = true;
}

void NearestNeighborMapping::map
(
    int inputN,
    int outputN,
    double* inputData,
    double* outputData)
{
  if(!_isComputed){
    std::vector<double> _inputCoords(inputN);
    std::vector<double> _outputCoords(outputN);
    for(int i=1; i<_inputCoords.size(); i++)
      _inputCoords.at(i) = (double)i/(double)inputN;
    for(int i=1; i<_outputCoords.size(); i++)
        _outputCoords.at(i) = (double)i/(double)outputN;

    computeMapping(inputN, outputN, _inputCoords, _outputCoords);
  }

  for(int i=0; i<_map.size(); i++){
    outputData[i] = inputData[_map.at(i)];
  }

}

