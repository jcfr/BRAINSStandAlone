/*=========================================================================

  Program:   GTRACT (Guided Tensor Restore Anatomical Connectivity Tractography)
  Module:    $RCSfile: $
  Language:  C++
  Date:      $Date: 2006/03/29 14:53:40 $
  Version:   $Revision: 1.9 $

    Copyright (c) University of Iowa Department of Radiology. All rights reserved.
    See GTRACT-Copyright.txt or http://mri.radiology.uiowa.edu/copyright/GTRACT-Copyright.txt
    for details.

      This software is distributed WITHOUT ANY WARRANTY; without even
      the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
      PURPOSE.  See the above copyright notices for more information.

=========================================================================*/

#include <vtkPoints.h>
#include <vtkFloatArray.h>
#include <vtkPointData.h>
#include <vtkLookupTable.h>
#include <vtkCellArray.h>
#include <vtkPolyData.h>
#include <vtkPolyDataWriter.h>
#include <vtkPolyDataReader.h>
#include <vtkXMLPolyDataWriter.h>
#include <vtkXMLPolyDataReader.h>
#include <vtkAppendPolyData.h>

#include <itkOrientImageFilter.h>
#include "itkDiffusionTensor3DReconstructionImageFilter.h"
#include "itkVectorImage.h"
#include "itkNrrdImageIO.h"
#include "itkImageFileReader.h"
#include "itkImageFileWriter.h"
#include "itkMetaDataDictionary.h"
#include "itkImageSeriesReader.h"
#include "itkImageRegionConstIterator.h"
#include "itkImageRegionIterator.h"
#include "itkImageRegionIteratorWithIndex.h"
#include "itkVectorIndexSelectionCastImageFilter.h"
#include <itkSpatialOrientationAdapter.h>
#include "vnl/vnl_math.h"
#include "vnl/vnl_matrix.h"
#include "itkNeighborhoodAlgorithm.h"
#include "itkConstantBoundaryCondition.h"
#include "itkVectorLinearInterpolateImageFunction.h"
#include "itkVariableLengthVector.h"
#include "itkImageFileWriter.h"
#include <vnl/algo/vnl_svd.h>
#include <iostream>

#include "algo.h"
#include "GtractTypes.h"

// ///////////// VTK Version Compatibility   //////////////////////////////
#ifndef vtkFloatingPointType
#define vtkFloatingPointType vtkFloatingPointType
typedef float vtkFloatingPointType;
#endif
// ////////////////////////////////////////////////////////////////////////

#include "gtractResampleFibersCLP.h"
#include "BRAINSThreadControl.h"
template <class TImageType>
void AdaptOriginAndDirection( typename TImageType::Pointer image )
{
  typename TImageType::DirectionType imageDir = image->GetDirection();
  typename TImageType::PointType origin = image->GetOrigin();

  int dominantAxisRL = itk::Function::Max3(imageDir[0][0], imageDir[1][0], imageDir[2][0]);
  int signRL = itk::Function::Sign(imageDir[dominantAxisRL][0]);
  int dominantAxisAP = itk::Function::Max3(imageDir[0][1], imageDir[1][1], imageDir[2][1]);
  int signAP = itk::Function::Sign(imageDir[dominantAxisAP][1]);
  int dominantAxisSI = itk::Function::Max3(imageDir[0][2], imageDir[1][2], imageDir[2][2]);
  int signSI = itk::Function::Sign(imageDir[dominantAxisSI][2]);

  /* This current  algorithm needs to be verified.
     I had previously though that it should be
     signRL == 1
     signAP == -1
     signSI == 1
     This appears to be incorrect with the NRRD file format
     at least. Visually this appears to work
     signRL == 1
     signAP == 1
     signSI == -1
  */
  typename TImageType::DirectionType DirectionToRAS;
  DirectionToRAS.SetIdentity();
  if( signRL == 1 )
    {
    DirectionToRAS[dominantAxisRL][dominantAxisRL] = -1.0;
    origin[dominantAxisRL] *= -1.0;
    }
  if( signAP == 1 )
    {
    DirectionToRAS[dominantAxisAP][dominantAxisAP] = -1.0;
    origin[dominantAxisAP] *= -1.0;
    }
  if( signSI == -1 )
    {
    DirectionToRAS[dominantAxisSI][dominantAxisSI] = -1.0;
    origin[dominantAxisSI] *= -1.0;
    }
  imageDir *= DirectionToRAS;
  image->SetDirection( imageDir  );
  image->SetOrigin( origin );
}

int main( int argc, char *argv[] )
{
  PARSE_ARGS;
  BRAINSUtils::SetThreadCount(numberOfThreads);
// itk::AddExtraTransformRegister();

  const unsigned int Dimension = 3;
  typedef unsigned short                      PixelType;
  typedef itk::VectorImage<unsigned short, 3> ImageType;
  typedef itk::VariableLengthVector<double>   realPixelType;

  // Some required typedef's
  typedef itk::Vector<double, Dimension>            VectorType;
  typedef itk::Matrix<double, Dimension, Dimension> MatrixType;
  typedef MatrixType::InternalMatrixType            vnlMatrixType;

  typedef   float                                       VectorComponentType;
  typedef   itk::Vector<VectorComponentType, Dimension> VectorPixelType;
  typedef   itk::Image<VectorPixelType,  Dimension>     DeformationFieldType;
  typedef   itk::ImageFileReader<DeformationFieldType>  FieldReaderType;

  FieldReaderType::Pointer forwardFieldReader = FieldReaderType::New();
  forwardFieldReader->SetFileName( inputForwardDeformationFieldVolume );

  try
    {
    std::cout << "Reading Forward Deformation Field........." << std::endl;
    forwardFieldReader->Update();
    }
  catch( itk::ExceptionObject & fe )
    {
    std::cout << "Field Exception caught ! " << fe << std::endl;
    }

  DeformationFieldType::Pointer forwardDeformationField = forwardFieldReader->GetOutput();
  // AdaptOriginAndDirection<DeformationFieldType>( forwardDeformationField );

  FieldReaderType::Pointer reverseFieldReader = FieldReaderType::New();
  reverseFieldReader->SetFileName( inputReverseDeformationFieldVolume );

  try
    {
    std::cout << "Reading Reverse Deformation Field........." << std::endl;
    reverseFieldReader->Update();
    }
  catch( itk::ExceptionObject & fe )
    {
    std::cout << "Field Exception caught ! " << fe << std::endl;
    }

  DeformationFieldType::Pointer reverseDeformationField = reverseFieldReader->GetOutput();
  // AdaptOriginAndDirection<DeformationFieldType>( reverseDeformationField );

  typedef itk::OrientImageFilter<DeformationFieldType, DeformationFieldType> OrientFilterType;
  OrientFilterType::Pointer orientImageFilter = OrientFilterType::New();
  orientImageFilter->SetInput( reverseDeformationField );
  orientImageFilter->SetDesiredCoordinateDirection( forwardDeformationField->GetDirection() );
  orientImageFilter->UseImageDirectionOn();
  try
    {
    orientImageFilter->Update();
    }
  catch( itk::ExceptionObject e )
    {
    std::cout << e << std::endl;
    throw;
    }

  vtkPolyData *inputFiber;
  if( writeXMLPolyDataFile )
    {
    vtkXMLPolyDataReader *inputFiberReader = vtkXMLPolyDataReader::New();
    inputFiberReader->SetFileName( inputTract.c_str() );
    inputFiberReader->Update();
    inputFiber = inputFiberReader->GetOutput();
    }
  else
    {
    vtkPolyDataReader *inputFiberReader = vtkPolyDataReader::New();
    inputFiberReader->SetFileName( inputTract.c_str() );
    inputFiberReader->Update();
    inputFiber = inputFiberReader->GetOutput();
    }

  //
  // Define Neighbourhood Iterator for computing the Jacobian on the fly

  typedef itk::ConstantBoundaryCondition<DeformationFieldType>                        boundaryConditionType;
  typedef itk::ConstNeighborhoodIterator<DeformationFieldType, boundaryConditionType> ConstNeighborhoodIteratorType;
  ConstNeighborhoodIteratorType::RadiusType radius;
  radius[0] = 1; radius[1] = 1; radius[2] = 1;

  DeformationFieldType::RegionType region;
  region.SetSize( forwardDeformationField->GetRequestedRegion().GetSize() );
  ConstNeighborhoodIteratorType bit( radius, forwardDeformationField, region);

  DeformationFieldType::PointType physicalPoint;
  DeformationFieldType::IndexType indexPoint;

  typedef double                                                                        CoordRepType;
  typedef itk::VectorLinearInterpolateImageFunction<DeformationFieldType, CoordRepType> InterpolatorType;
  InterpolatorType::Pointer interpolator = InterpolatorType::New();
  interpolator->SetInputImage( orientImageFilter->GetOutput() );

  vtkPoints *fiberPoints = inputFiber->GetPoints();
  // /vtkDataArray *fiberTensors = inputFiber->GetPointData()->GetTensors( );

  vnlMatrixType J;
  for( int i = 0; i < inputFiber->GetNumberOfPoints(); i++ )
    {
    vtkFloatingPointType fiberPoint[3];
    fiberPoints->GetPoint(i, fiberPoint);

    /* Map Point */
    typedef itk::Point<double, 3> PointType;
    PointType currentLocation;
    currentLocation[0] = fiberPoint[0];
    currentLocation[1] = fiberPoint[1];
    currentLocation[2] = fiberPoint[2];

    const itk::FixedArray<double, 3> deformationVector = interpolator->Evaluate( currentLocation );
    fiberPoint[0] += deformationVector[0];
    fiberPoint[1] += deformationVector[1];
    fiberPoint[2] += deformationVector[2];
    fiberPoints->SetPoint(i, fiberPoint);

    vnlMatrixType fullTensorPixel;
    inputFiber->GetPointData()->GetTensors()->GetTuple( i, fullTensorPixel.data_block() );

    /* Rotate Tensor */
    physicalPoint[0] = fiberPoint[0];
    physicalPoint[1] = fiberPoint[1];
    physicalPoint[2] = fiberPoint[2];
    forwardDeformationField->TransformPhysicalPointToIndex(physicalPoint, indexPoint);
    bit.SetLocation(indexPoint);
    bit.GoToBegin();

    // compute Jacobian
    for( int k = 0; k < 3; ++k )
      {
      for( int j = 0; j < 3; ++j )
        {
        J(j, k) =  0.5 * ( bit.GetPrevious(k)[j] - bit.GetNext(k)[j] );
        }
      }

    // Use SVD for computing rotation matrix

    vnlMatrixType iden;
    iden.set_identity();

    // Compute Singluar value Decompostion of the Jacobian to find the Rotation
    // Matrix.

    vnl_svd<double> svd(J + iden);
    vnlMatrixType   rotationMatrix( svd.U() * svd.V().transpose() );

    vnlMatrixType rotatedTensorPixel = rotationMatrix * fullTensorPixel * rotationMatrix.transpose();

    inputFiber->GetPointData()->GetTensors()->SetTuple( i, rotatedTensorPixel.data_block() );
    }

  inputFiber->SetPoints( fiberPoints );

  if( writeXMLPolyDataFile )
    {
    vtkXMLPolyDataWriter *fiberWriter = vtkXMLPolyDataWriter::New();
    fiberWriter->SetFileName( outputTract.c_str() );
    fiberWriter->SetInput( inputFiber );
    fiberWriter->Update();
    }
  else
    {
    vtkPolyDataWriter *fiberWriter = vtkPolyDataWriter::New();
    fiberWriter->SetFileName( outputTract.c_str() );
    fiberWriter->SetInput( inputFiber );
    fiberWriter->Update();
    }
  return EXIT_SUCCESS;
}

