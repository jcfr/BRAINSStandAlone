#include "LLSModel.h"
#include <iostream>
LLSModel
::LLSModel() : m_H5File(0)
{
}

LLSModel
::~LLSModel()
{
  if( this->m_H5File != 0 )
    {
    this->m_H5File->close();
    delete this->m_H5File;
    }
}

void
LLSModel
::SetFileName(const std::string & fileName)
{
  m_FileName = fileName;
}

const char * const LLSModel::m_LLSMeansGroupName = "LLSMeans";
const char * const LLSModel::m_LLSMatricesGroupName = "LLSMatrices";
const char * const LLSModel::m_LLSSearchRadiiGroupName = "LLSSearchRadii";

void
LLSModel
::WriteVector(const std::string & path,
              const std::vector<double> & vec)
{
  hsize_t dim(vec.size() );
  double *buf(new double[dim]);
  for( unsigned i(0); i < dim; i++ )
    {
    buf[i] = vec[i];
    }

  H5::DataSpace vecSpace(1, &dim);
  H5::PredType  vecType = H5::PredType::NATIVE_DOUBLE;
  H5::DataSet   vecSet =
    this->m_H5File->createDataSet(path,
                                  vecType,
                                  vecSpace);
  vecSet.write(buf, vecType);
  vecSet.close();
  delete [] buf;
}

void
LLSModel
::WriteMatrix(const std::string & path,
              const MatrixType & matrix)
{
  hsize_t dims[2];

  dims[0] = matrix.rows();
  dims[1] = matrix.cols();

  double *buffer = new double[dims[0] * dims[1]];
  matrix.copy_out(buffer);

  H5::DataSpace matrixSpace(2, dims);
  H5::DataSet   matrixSet =
    this->m_H5File->createDataSet(path,
                                  H5::PredType::NATIVE_DOUBLE,
                                  matrixSpace);
  matrixSet.write(buffer, H5::PredType::NATIVE_DOUBLE,
                  matrixSpace);
  matrixSet.close();
  delete [] buffer;
}

void
LLSModel
::WriteScalar(const std::string & path,
              const double & value)
{
  hsize_t       numScalars(1);
  H5::DataSpace scalarSpace(1, &numScalars);
  H5::PredType  scalarType = H5::PredType::NATIVE_DOUBLE;
  H5::DataSet   scalarSet =
    this->m_H5File->createDataSet(path,
                                  scalarType,
                                  scalarSpace);

  scalarSet.write(&value, scalarType);
  scalarSet.close();
}

int
LLSModel
::Write()
{
  if( this->m_FileName == "" )
    {
    return -1;
    }
  if( this->m_LLSMeans.size() == 0 ||
      this->m_LLSMatrices.size() == 0 ||
      this->m_LLSSearchRadii.size() == 0 )
    {
    return -1;
    }
  try
    {

    this->m_H5File = new H5::H5File(this->m_FileName.c_str(), H5F_ACC_TRUNC);

    H5::Group MeansGroup(this->m_H5File->createGroup(LLSModel::m_LLSMeansGroupName) );
    for( LLSMeansType::iterator it = this->m_LLSMeans.begin();
         it != this->m_LLSMeans.end();
         it++ )
      {
      std::string curVecName(LLSModel::m_LLSMeansGroupName);
      curVecName += "/";
      curVecName += it->first;
      this->WriteVector(curVecName, it->second);
      }

    H5::Group MatricesGroup(this->m_H5File->createGroup(LLSModel::m_LLSMatricesGroupName) );
    for( LLSMatricesType::iterator it = this->m_LLSMatrices.begin();
         it != this->m_LLSMatrices.end();
         it++ )
      {
      std::string curMatName(LLSModel::m_LLSMatricesGroupName);
      curMatName += "/";
      curMatName += it->first;
      this->WriteMatrix(curMatName, it->second);
      }

    H5::Group SearchRadiiGroup(this->m_H5File->createGroup(LLSModel::m_LLSSearchRadiiGroupName) );
    for( LLSSearchRadiiType::iterator it = this->m_LLSSearchRadii.begin();
         it != this->m_LLSSearchRadii.end();
         it++ )
      {
      std::string curRadiusName(LLSModel::m_LLSSearchRadiiGroupName);
      curRadiusName += "/";
      curRadiusName += it->first;
      this->WriteScalar(curRadiusName, it->second);
      }

    this->m_H5File->close();
    delete this->m_H5File;
    this->m_H5File = 0;
    }
  // catch failure caused by the H5File operations
  catch( H5::FileIException error )
    {
    error.printError();
    return -1;
    }
  // catch failure caused by the DataSet operations
  catch( H5::DataSetIException error )
    {
    error.printError();
    return -1;
    }
  // catch failure caused by the DataSpace operations
  catch( H5::DataSpaceIException error )
    {
    error.printError();
    return -1;
    }
  // catch failure caused by the DataType operations
  catch( H5::DataTypeIException error )
    {
    error.printError();
    return -1;
    }
  return 0;
}

std::vector<double>
LLSModel
::ReadVector(const std::string & DataSetName)
{
  std::vector<double> vec;
  hsize_t             dim;
  H5::DataSet         vecSet = this->m_H5File->openDataSet(DataSetName);
  H5::DataSpace       Space = vecSet.getSpace();

  if( Space.getSimpleExtentNdims() != 1 )
    {
    std::cerr << "Wrong #of dims for vector "
              << "in HDF5 File" << std::endl;
    throw;
    }
  Space.getSimpleExtentDims(&dim, 0);
  vec.resize(dim);

  double *buf = new double[dim];
  vecSet.read(buf, H5::PredType::NATIVE_DOUBLE);
  for( unsigned i(0); i < dim; i++ )
    {
    vec[i] = buf[i];
    }
  delete [] buf;
  vecSet.close();
  return vec;
}

LLSModel::MatrixType
LLSModel
::ReadMatrix(const std::string & DataSetName)
{
  hsize_t       dims[2];
  H5::DataSet   matrixSet = this->m_H5File->openDataSet(DataSetName);
  H5::DataSpace matrixSpace = matrixSet.getSpace();

  if( matrixSpace.getSimpleExtentNdims() != 2 )
    {
    std::cerr << "Wrong #of dims for matrix "
              << "in HDF5 File" << std::endl;
    throw;
    }
  matrixSpace.getSimpleExtentDims(dims, 0);
  MatrixType mat(dims[0], dims[1]);
  matrixSet.read(mat.data_block(), H5::PredType::NATIVE_DOUBLE);
  matrixSet.close();
  return mat;
}

double
LLSModel
::ReadScalar(const std::string & DataSetName)
{
  hsize_t       dim;
  H5::DataSet   scalarSet = this->m_H5File->openDataSet(DataSetName);
  H5::DataSpace Space = scalarSet.getSpace();

  if( Space.getSimpleExtentNdims() != 1 )
    {
    std::cerr << "Wrong #of dims for TransformType "
              << "in HDF5 File" << std::endl;
    }
  Space.getSimpleExtentDims(&dim, 0);
  if( dim != 1 )
    {
    std::cerr << "Elements > 1 for scalar type "
              << "in HDF5 File" << std::endl;;
    }
  double scalar;
  scalarSet.read(&scalar, H5::PredType::NATIVE_DOUBLE);
  scalarSet.close();
  return scalar;
}

int
LLSModel
::Read()
{

  if( this->m_FileName == "" )
    {
    return -1;
    }
  try
    {

    this->m_H5File = new H5::H5File(this->m_FileName.c_str(), H5F_ACC_RDONLY);

    H5::Group MeansGroup(this->m_H5File->openGroup(LLSModel::m_LLSMeansGroupName) );

    hsize_t numLLSMeans = MeansGroup.getNumObjs();
    for( hsize_t i = 0; i < numLLSMeans; i++ )
      {
      std::string LLSMeanName = MeansGroup.getObjnameByIdx(i);
      std::string curVecName(LLSModel::m_LLSMeansGroupName);
      curVecName += "/";
      curVecName += LLSMeanName;
      this->m_LLSMeans[LLSMeanName] = this->ReadVector(curVecName);
      }
    MeansGroup.close();

    H5::Group MatricesGroup(this->m_H5File->openGroup(LLSModel::m_LLSMatricesGroupName) );
    hsize_t   numLLSMatrices = MatricesGroup.getNumObjs();
    for( hsize_t i = 0; i < numLLSMatrices; i++ )
      {
      std::string LLSMatrixName = MatricesGroup.getObjnameByIdx(i);
      std::string curMatName(LLSModel::m_LLSMatricesGroupName);
      curMatName += "/";
      curMatName += LLSMatrixName;
      this->m_LLSMatrices[LLSMatrixName] = this->ReadMatrix(curMatName);
      }
    MatricesGroup.close();

    H5::Group SearchRadiiGroup(this->m_H5File->openGroup(LLSModel::m_LLSSearchRadiiGroupName) );
    hsize_t   numSearchRadii = SearchRadiiGroup.getNumObjs();
    for( hsize_t i = 0; i < numSearchRadii; i++ )
      {
      std::string SearchRadiusName = SearchRadiiGroup.getObjnameByIdx(i);
      std::string curRadiusName(LLSModel::m_LLSSearchRadiiGroupName);
      curRadiusName += "/";
      curRadiusName += SearchRadiusName;
      this->m_LLSSearchRadii[SearchRadiusName] = this->ReadScalar(curRadiusName);
      }
    SearchRadiiGroup.close();

    this->m_H5File->close();
    delete this->m_H5File;
    this->m_H5File = 0;
    }
  // catch failure caused by the H5File operations
  catch( H5::FileIException error )
    {
    error.printError();
    return -1;
    }
  // catch failure caused by the DataSet operations
  catch( H5::DataSetIException error )
    {
    error.printError();
    return -1;
    }
  // catch failure caused by the DataSpace operations
  catch( H5::DataSpaceIException error )
    {
    error.printError();
    return -1;
    }
  // catch failure caused by the DataType operations
  catch( H5::DataTypeIException error )
    {
    error.printError();
    return -1;
    }
  return 0;
}

void
LLSModel
::SetLLSMeans(const LLSMeansType & llsMeans)
{
  this->m_LLSMeans = llsMeans;
}

const LLSModel::LLSMeansType &
LLSModel
::GetLLSMeans()
{
  return this->m_LLSMeans;
}

void
LLSModel
::SetLLSMatrices(const LLSMatricesType & llsMatrices)
{
  this->m_LLSMatrices = llsMatrices;
}

const LLSModel::LLSMatricesType &
LLSModel
::GetLLSMatrices()
{
  return this->m_LLSMatrices;
}

void
LLSModel
::SetSearchRadii(const LLSSearchRadiiType & llsSearchRadii)
{
  this->m_LLSSearchRadii = llsSearchRadii;
}

const LLSModel::LLSSearchRadiiType &
LLSModel
::GetSearchRadii()
{
  return m_LLSSearchRadii;
}

