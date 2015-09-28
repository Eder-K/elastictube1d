#include <iostream>
#include <stdlib.h>
#include "precice/SolverInterface.hpp"
#include "../fluid_solver/NearestNeighborMapping.hpp"
//#include "mpi.h"

using std::cout;
using std::endl;

void printData (const std::vector<double>& data)
{
  cout << "Received data = " << data[0];
  for (size_t i=1; i < data.size(); i++){
    cout << ", " << data[i];
  }
  cout << endl;
}


int main (int argc, char **argv)
{
  cout << "Starting Structure Solver..." << endl;
  using namespace precice;
  using namespace precice::constants;

  if ( argc != 4 )
  {
    cout << endl;
    cout << "Usage: " << argv[0] << " configurationFileName N" << endl;
    cout << endl;
    cout << "N:     Number of mesh elements, needs to be equal for fluid and structure solver." << endl;
    cout << "N_SM:  Number of surrogate model mesh elements, needs to be equal for fluid and structure solver." << endl;
    return -1;
  }

  std::string configFileName(argv[1]);
  int N = atoi( argv[2] );
  int N_SM = atoi( argv[3] );

  std::cout << "N: " << N <<" N (surrogate model): "<< N_SM << std::endl;

  std::string dummyName = "STRUCTURE";

  SolverInterface interface(dummyName, 0, 1);
  interface.configure(configFileName);
  cout << "preCICE configured..." << endl;

  //init data (fine model)
  double *displ, *sigma;
  int dimensions = interface.getDimensions();
  displ     = new double[N+1];  // Second dimension (only one cell deep) stored right after the first dimension: see SolverInterfaceImpl::setMeshVertices
  sigma     = new double[N+1];
  double *grid;
  grid = new double[dimensions*(N+1)];

  //init data (fine model)
  double *displ_coarse, *sigma_coarse;
  double *displ_copy_coarse;
  double *sigma_copy_coarse;

  displ_coarse = new double[N_SM+1];  // Second dimension (only one cell deep) stored right after the first dimension: see SolverInterfaceImpl::setMeshVertices
  sigma_coarse = new double[N_SM+1];
  displ_copy_coarse = new double[N+1];
  sigma_copy_coarse = new double[N+1];

  // mappings
  NearestNeighborMapping upMapping, downMapping;

  //precice stuff
  int meshID = interface.getMeshID("Structure_Nodes");
  int displID = interface.getDataID ( "Displacements", meshID );
  int sigmaID = interface.getDataID ( "Stresses", meshID );

  int displID_coarse = interface.getDataID ( "Coarse_Displacements", meshID );
  int sigmaID_coarse = interface.getDataID ( "Coarse_Stresses", meshID );
  int *vertexIDs, *vertexIDs_coarse;

  vertexIDs = new int[N+1];
  vertexIDs_coarse = new int[N+1];

  // fine model init
  for(int i=0; i<=N; i++)
  {
    displ[i] = 1.0;
    sigma[i] = 0.0;
    displ_copy_coarse[i] = 1.0;

    for(int dim = 0; dim < dimensions; dim++)
      grid[i*dimensions + dim] = i*(1-dim);   // Define the y-component of each grid point as zero
  }

  // surrogate model init
  for(int i=0; i<=N; i++)
  {
    displ_coarse[i] = 1.0;
    sigma_coarse[i] = 0.0;
  }

  int t = 0;
  interface.setMeshVertices(meshID, N+1, grid, vertexIDs);
  interface.setMeshVertices(meshID, N+1, grid, vertexIDs_coarse); // TODO: ???

  cout << "Structure: init precice..." << endl;
  double dt = interface.initialize();

  if (interface.isActionRequired(actionWriteInitialData()))
  {
    interface.writeBlockScalarData(displID, N+1, vertexIDs, displ);
    interface.writeBlockScalarData(displID_coarse, N+1, vertexIDs_coarse, displ_copy_coarse); // TODO: ???
    //interface.initializeData();
    interface.fulfilledAction(actionWriteInitialData());
  }

  interface.initializeData();


  if (interface.isReadDataAvailable())
  {
    interface.readBlockScalarData(sigmaID , N+1, vertexIDs, sigma);
    interface.readBlockScalarData(sigmaID_coarse , N+1, vertexIDs_coarse, sigma_copy_coarse);
  }

  while (interface.isCouplingOngoing())
  {
    // When an implicit coupling scheme is used, checkpointing is required
    if (interface.isActionRequired(actionWriteIterationCheckpoint()))
    {
      interface.fulfilledAction(actionWriteIterationCheckpoint());
    }

    // only surrogate model evaluation for surrogate model optimization
    if(interface.hasToEvaluateSurrogateModel())
    {

      // map down:  fine --> coarse
      downMapping.map(N, N_SM, displ_copy_coarse, displ_coarse);
      downMapping.map(N, N_SM, sigma_copy_coarse, sigma_coarse);

      // ### surrogate model evaluation ###
      for ( int i = 0; i <= N_SM; i++ )
        displ_coarse[i]   = 4.0 / ((2.0 - sigma_coarse[i])*(2.0 - sigma_coarse[i]));

      // map up:  coarse --> fine
      upMapping.map(N_SM, N, displ_coarse, displ_copy_coarse);
      upMapping.map(N_SM, N, sigma_coarse, sigma_copy_coarse);

      // write coarse model response (on fine mesh)
      interface.writeBlockScalarData(displID_coarse, N+1, vertexIDs_coarse, displ_copy_coarse);

      // fine and coarse model evaluation (in MM iteration cycles)
    }else{

      // ### fine model evaluation ###
      for ( int i = 0; i <= N; i++ )
        displ[i]   = 4.0 / ((2.0 - sigma[i])*(2.0 - sigma[i]));

      // write fine model response
      interface.writeBlockScalarData(displID, N+1, vertexIDs, displ);

      // map down:  fine --> coarse
      downMapping.map(N, N_SM, displ_copy_coarse, displ_coarse);
      downMapping.map(N, N_SM, sigma_copy_coarse, sigma_coarse);

      // ### surrogate model evaluation ###
      for ( int i = 0; i <= N_SM; i++ )
        displ_coarse[i]   = 4.0 / ((2.0 - sigma_coarse[i])*(2.0 - sigma_coarse[i]));

      // map up:  coarse --> fine
      upMapping.map(N_SM, N, displ_coarse, displ_copy_coarse);
      upMapping.map(N_SM, N, sigma_coarse, sigma_copy_coarse);

      // write coarse model response (on fine mesh)
      interface.writeBlockScalarData(displID_coarse, N+1, vertexIDs_coarse, displ_copy_coarse);

    }

    // perform coupling using preCICE
    interface.advance(0.01); // Advance by dt = 0.01

    // read coupling data from preCICE.

    // only surrogate model evaluation for surrogate model optimization
    if(interface.hasToEvaluateSurrogateModel())
    {
      interface.readBlockScalarData(sigmaID_coarse, N+1, vertexIDs_coarse, sigma_copy_coarse);

      // fine and coarse model evaluation (in MM iteration cycles)
    }else{
      interface.readBlockScalarData(sigmaID, N+1, vertexIDs, sigma);
      interface.readBlockScalarData(sigmaID_coarse, N+1, vertexIDs_coarse, sigma_copy_coarse);
    }



    if (interface.isActionRequired(actionReadIterationCheckpoint()))
    {
      cout << "Iterate" << endl;
      interface.fulfilledAction(actionReadIterationCheckpoint());
    }
    else
    {
      cout << "Advancing in time, finished timestep: " << t << endl;
      t++;
    }
  }

  interface.finalize();
  cout << "Exiting StructureSolver" << endl;

  return 0;
}
