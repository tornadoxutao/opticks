/*
 * The information in this file is
 * Copyright(c) 2007 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include <QtGui/QFileDialog>
#include <QtGui/QMessageBox>

#include "AppAssert.h"
#include "AppConfig.h"
#include "AppVerify.h"
#include "AppVersion.h"
#include "AoiElement.h"
#include "ApplicationServices.h"
#include "ConfigurationSettings.h"
#include "DataAccessorImpl.h"
#include "DimensionDescriptor.h"
#include "EigenPlotDlg.h"
#include "Filename.h"
#include "FileResource.h"
#include "MatrixFunctions.h"
#include "MessageLogResource.h"
#include "ObjectResource.h"
#include "PCA.h"
#include "PcaDlg.h"
#include "PlugInArg.h"
#include "PlugInArgList.h"
#include "PlugInResource.h"
#include "RasterDataDescriptor.h"
#include "RasterElement.h"
#include "RasterFileDescriptor.h"
#include "RasterUtilities.h"
#include "SpatialDataView.h"
#include "SpatialDataWindow.h"
#include "StatisticsDlg.h"
#include "switchOnEncoding.h"
#include "Undo.h"

#include <fstream>
#include <limits>
#include <math.h>
#include <typeinfo>
using namespace std;

template<class T>
T *GetPixelPtr(T *raw, int numCols, int numBands, int row, int col)
{
   return raw + numBands * (row *numCols + col);
}

template<class T>
void ComputeFactoredCov(T *pData, RasterElement* pRaster, double **pMatrix,
                        Progress *pProgress, const bool *pAbortFlag,
                        int rowFactor, int columnFactor)
{
   const RasterDataDescriptor* pDescriptor = dynamic_cast<const RasterDataDescriptor*>(pRaster->getDataDescriptor());
   if(pDescriptor == NULL)
   {
      return;
   }

   unsigned int NumRows = pDescriptor->getRowCount();
   unsigned int NumCols = pDescriptor->getColumnCount();
   unsigned int NumBands = pDescriptor->getBandCount();
   unsigned int row, col;
   unsigned int band1, band2;
   unsigned int lCount = 0;
   T *pPixel = NULL;
   vector<double> averages(NumBands);
   double* pAverage = &averages.front();
   fill(averages.begin(), averages.end(), 0);

   if(rowFactor < 1)
   {
      rowFactor = 1;
   }
   if(columnFactor < 1)
   {
      columnFactor = 1;
   }

   // calculate average spectrum
   float progScale = 100.0f * static_cast<float>(rowFactor)/static_cast<float>(NumRows);
   FactoryResource<DataRequest> pRequest;
   pRequest->setInterleaveFormat(BIP);
   DataAccessor accessor = pRaster->getDataAccessor(pRequest.release());
   for(row = 0; row < NumRows; row += rowFactor)
   {
      VERIFYNRV(accessor.isValid());
      if(pProgress != NULL)
      {
         if((pAbortFlag == NULL) || !(*pAbortFlag))
         {
            pProgress->updateProgress ("Computing Average Signature...", int(progScale*row), NORMAL);
         }
         else
         {
            break;
         }
      }
      for(col = 0; col < NumCols; col += columnFactor)
      {
         lCount++;
         pPixel = reinterpret_cast<T*>(accessor->getColumn());

         for(band1 = 0; band1 < NumBands; band1++)
         {
            pAverage[band1] += *pPixel;
            pPixel++;
         }
         accessor->nextColumn();
      }
      accessor->nextRow();
   }

   // check if aborted
   if((pAbortFlag == NULL) || !(*pAbortFlag))
   {
      for(band1 = 0; band1 < NumBands; band1++)
      {
         pAverage[band1] /= lCount;
      }

      // compute the covariance
      FactoryResource<DataRequest> pRequest;
      pRequest->setInterleaveFormat(BIP);
      accessor = pRaster->getDataAccessor(pRequest.release());

      for(row = 0; row < NumRows; row += rowFactor)
      {
         VERIFYNRV(accessor.isValid());
         if(pProgress != NULL)
         {
            if((pAbortFlag == NULL) || !(*pAbortFlag))
            {
               pProgress->updateProgress("Computing Covariance Matrix...", int(progScale * row), NORMAL);
            }
            else
            {
               break;
            }
         }
         for(col = 0; col < NumCols; col += columnFactor)
         {
            pPixel = reinterpret_cast<T*>(accessor->getColumn());

            for(band2 = 0; band2 < NumBands; band2++)
            {
               pData = pPixel;
               for(band1 = band2; band1 < NumBands; band1++)
               {
                  pMatrix[band2][band1] += (*pData - pAverage[band1]) * (*pPixel - pAverage[band2]);
                  pData++;
               }
               pPixel++;
            }
            accessor->nextColumn();
         }
         accessor->nextRow();
      }
   }

   // check if aborted
   if((pAbortFlag == NULL) || !(*pAbortFlag))
   {
      for(band2 = 0; band2 < NumBands; band2++)
      {
        for(band1 = band2; band1 < NumBands; band1++)
        {
          pMatrix[band2][band1] /= lCount;
        }
      }

      for(band2 = 0; band2 < NumBands; band2++)
      {
        for(band1 = band2 + 1; band1 < NumBands; band1++)
        {
          pMatrix[band1][band2] = pMatrix[band2][band1];
        }
      }
   }

   if(pProgress != NULL)
   {
      if((pAbortFlag == NULL) || !(*pAbortFlag))
      {
         pProgress->updateProgress("Covariance Matrix Complete", 100, NORMAL);
      }
      else
      {
         pProgress->updateProgress("Aborted computing Covariance Matrix", 0, ABORT);
      }
   }
}

struct MaskInput
{
   RasterElement* pRaster;
   double **pMatrix;
   Progress *pProgress;
   const bool *pAbortFlag;
   const BitMask* pMask;
};

template<class T>
void ComputeMaskedCov(T *pData, MaskInput *pInput)
{
   const RasterDataDescriptor* pDescriptor = dynamic_cast<const RasterDataDescriptor*>
      (pInput->pRaster->getDataDescriptor());
   if(pDescriptor == NULL)
   {
      return;
   }

   unsigned int NumRows = pDescriptor->getRowCount();
   unsigned int NumCols = pDescriptor->getColumnCount();
   unsigned int NumBands = pDescriptor->getBandCount();

   unsigned int col, row;
   unsigned int band1, band2;
   unsigned int lCount = 0;
   vector<double> averages(NumBands);
   double* pAverage = &averages.front();
   fill(averages.begin(), averages.end(), 0);
   T *pPixel = NULL;
   int x1 = 0,x2 = 0,y1 = 0,y2 = 0;
   pInput->pMask->getBoundingBox(x1, y1, x2, y2);
   x1 = x1 < 0 ? 0 : x1;
   y1 = y1 < 0 ? 0 : y1;
   x1 = x1 > static_cast<int>(NumCols) ? static_cast<int>(NumCols) : x1;
   y1 = y1 > static_cast<int>(NumRows) ? static_cast<int>(NumRows) : y1;
   const bool** pSelectedPixels = const_cast<BitMask*>(pInput->pMask)->getRegion(x1, y1, x2, y2);

   unsigned int boundingBoxXSize = x2 - x1 + 1;
   boundingBoxXSize = boundingBoxXSize > NumCols ? NumCols : boundingBoxXSize;
   unsigned int boundingBoxYSize = y2 - y1 + 1;
   boundingBoxYSize = boundingBoxYSize > NumRows ? NumRows : boundingBoxYSize;

   // calculate average spectrum
   float progScale = 100.0f / boundingBoxYSize;
   FactoryResource<DataRequest> pRequest;
   pRequest->setInterleaveFormat(BIP);
   pRequest->setRows(pDescriptor->getActiveRow(y1), pDescriptor->getActiveRow(y2));
   pRequest->setColumns(pDescriptor->getActiveColumn(x1), pDescriptor->getActiveColumn(x2));
   DataAccessor accessor = pInput->pRaster->getDataAccessor(pRequest->copy());
   for(row = 0; row < boundingBoxYSize; row++)
   {
      VERIFYNRV(accessor.isValid());
      if(pInput->pProgress != NULL)
      {
         if((pInput->pAbortFlag == NULL) || !(*pInput->pAbortFlag))
         {
            pInput->pProgress->updateProgress ("Computing Average Signature...", int(progScale*row), NORMAL);
         }
         else
         {
            break;
         }
      }
      for(col = 0; col < boundingBoxXSize; col++)
      {
         if(pSelectedPixels[row][col])
         {
            lCount++;
            pPixel = reinterpret_cast<T*>(accessor->getColumn());

            for(band1 = 0; band1 < NumBands; band1++)
            {
               pAverage[band1] += *pPixel;
               pPixel++;
            }
         }
         accessor->nextColumn();
      }
      accessor->nextRow();
   }

   // check if aborted
   if((pInput->pAbortFlag == NULL) || !(*pInput->pAbortFlag))
   {
      for(band1 = 0; band1 < NumBands; band1++)
      {
         pAverage[band1] /= lCount;
      }

      // calculate covariance matrix
      accessor = pInput->pRaster->getDataAccessor(pRequest->copy());
      for(row = 0; row < boundingBoxYSize; row++)
      {
         if(pInput->pProgress != NULL)
         {
            if((pInput->pAbortFlag == NULL) || !(*pInput->pAbortFlag))
            {
               pInput->pProgress->updateProgress("Computing Covariance Matrix...", int(progScale * row), NORMAL);
            }
            else
            {
               break;
            }
         }

         VERIFYNRV(accessor.isValid());
         for(col = 0; col < boundingBoxXSize; col++)
         {
            if(pSelectedPixels[row][col])
            {
               pPixel = reinterpret_cast<T*>(accessor->getColumn());
               for(band2 = 0; band2 < NumBands; band2++)
               {
                  pData = pPixel;
                  for(band1 = band2; band1 < NumBands; band1++)
                  {
                     pInput->pMatrix[band2][band1] += (*pPixel-pAverage[band2]) * (*pData-pAverage[band1]);
                     pData++;
                  }
                  pPixel++;
               }
            }
            accessor->nextColumn();
         }
         accessor->nextRow();
      }
   }

   // check if aborted
   if((pInput->pAbortFlag == NULL) || !(*pInput->pAbortFlag))
   {
      for(band2 = 0; band2 < NumBands; band2++)
      {
         for(band1 = band2; band1 < NumBands; band1++)
         {
            pInput->pMatrix[band2][band1] /= lCount;
         }
      }

      for(band2 = 0; band2 < NumBands; band2++)
      {
         for(band1 = band2; band1 < NumBands; band1++)
         {
            pInput->pMatrix[band1][band2] = pInput->pMatrix[band2][band1];
         }
      }
   }


   if(pInput->pProgress != NULL)
   {
      if((pInput->pAbortFlag == NULL) || !(*pInput->pAbortFlag))
      {
         pInput->pProgress->updateProgress("Covariance Matrix Complete", 100, NORMAL);
      }
      else
      {
         pInput->pProgress->updateProgress("Aborted computing Covariance Matrix", 0, ABORT);
      }
   }
}

template<class T>
void ComputePcaValue(T *pData, double* pPcaValue, double *pCoefficients, unsigned int numBands)
{
   T* pInput = pData;
   double* pCoef = pCoefficients;
   *pPcaValue = 0.0;

   for(unsigned int i = 0; i < numBands; i++)
   {
      *pPcaValue += static_cast<double>(*pInput) * (*pCoef);
      pInput++;
      pCoef++;
   }
}

template <class T>
void ComputePcaRow(T* pData, double* pPcaData,  double* pCoefficients,
                   unsigned int numCols, unsigned int numBands,
                   double* minValue, double* maxValue)
{
   T* pColumn = pData;
   T* pInput = NULL;
   double* pValue = pPcaData;
   double* pCoef = NULL;

   for(unsigned int col = 0; col < numCols; col++)
   {
      *pValue = 0.0;
      pInput = pColumn;
      pCoef = pCoefficients;
      for(unsigned int band = 0; band < numBands; band++)
      {
         *pValue += (*pCoef * static_cast<double>(*pInput));
         pCoef++;
         pInput++;
      }
      if (*pValue > *maxValue) *maxValue = *pValue;
      if (*pValue < *minValue) *minValue = *pValue;
      pValue++;
      pColumn += numBands;
   }
}

template <class T>
void StorePcaRow(T* pPcaData, double* pCompValues, unsigned int numCols,
                 unsigned int numComponents, double minVal, double scaleFactor)
{
   T* pOutput = pPcaData;
   double* pInput = pCompValues;
   for(unsigned int col = 0; col < numCols; col++)
   {
      *pOutput = static_cast<T>((*pInput - minVal) * scaleFactor + 0.5);
      pInput++;
      pOutput += numComponents;
   }
}

template <class T>
void StorePcaValue(T* pPcaData, double* pValue, double* pMinVal, double* pScaleFactor)
{
   *pPcaData = static_cast<T>((*pValue - *pMinVal) * (*pScaleFactor) + 0.5);
}

using namespace std;

PCA::PCA() :
   mpProgress(NULL),
   mpRaster(NULL),
   mpSecondMomentMatrix(NULL),
   mp_MatrixValues(NULL),
   mp_AOIbitmask(NULL),
   m_NumRows(0),
   m_NumColumns(0),
   m_NumBands(0),
   mbInteractive(false),
   mbAbort(false),
   mb_UseAoi(false),
   m_CalcMethod(SECONDMOMENT),
   mbUseEigenValPlot(false),
   mpStep(NULL)
{
   setName("Principal Component Analysis");
   setVersion(APP_VERSION_NUMBER);
   setCreator("Ball Aerospace & Technologies Corp.");
   setCopyright(APP_COPYRIGHT);
   setShortDescription("PCA");
   setDescription("Run Principal Component Analysis on data cube.");
   setMenuLocation("[General Algorithms]\\Principal Component Analysis");
   setDescriptorId("{7D2F39B3-31BA-4ef1-B326-7ADCD7F92186}");
   allowMultipleInstances(true);
   setProductionStatus(APP_IS_PRODUCTION_RELEASE);
}

PCA::~PCA()
{
   if(mp_AOIbitmask != NULL)
   {
      mpObjFact->destroyObject(mp_AOIbitmask, "BitMask");
   }
}

bool PCA::setBatch()
{
   mbInteractive = false;
   return true;
}

bool PCA::setInteractive()
{
   mbInteractive = true;
   return true;
}

bool PCA::hasAbort()
{
   return true;
}

bool PCA::getInputSpecification(PlugInArgList*& pArgList)
{
   // Set up list
   pArgList = mpPlugInMgr->getPlugInArgList();
   VERIFY(pArgList != NULL);
   VERIFY(pArgList->addArg<Progress>(ProgressArg(), NULL));
   VERIFY(pArgList->addArg<RasterElement>(DataElementArg(), NULL));

   if(!mbInteractive) // need additional info in batch mode
   {
      /*
         Cases for batch mode:
         1) Use PCA File
            a) SMM
            b) Covariance
            c) Correlation Coefficients
         2) Don't Use PCA File
            a) AOI
               i) SMM
                  A) Use SMM File / Don't use
            b) No AOI
               i) SMM
                  A) Use SMM File / Don't use
      */

      VERIFY(pArgList->addArg<bool>("Use Transform File", NULL));
      VERIFY(pArgList->addArg<Filename>("Transform Filename", NULL));
      VERIFY(pArgList->addArg<string>("Transform Type", NULL));
      VERIFY(pArgList->addArg<bool>("Use AOI", false));
      VERIFY(pArgList->addArg<string>("AOI Name", false));
      VERIFY(pArgList->addArg<int>("Components", NULL));
      VERIFY(pArgList->addArg<EncodingType>("Output Encoding Type", NULL));
      VERIFY(pArgList->addArg<int>("Max Scale Value", NULL));
      VERIFY(pArgList->addArg<RasterElement>("Second Moment Matrix", NULL));
   }

   return true;
}

bool PCA::getOutputSpecification(PlugInArgList*& pArgList)
{
   // Set up list
   pArgList = mpPlugInMgr->getPlugInArgList();
   VERIFY(pArgList != NULL);
   VERIFY(pArgList->addArg<RasterElement>("Corrected Data Cube", NULL));
   return true;
}

bool PCA::execute(PlugInArgList* pInArgList, PlugInArgList* pOutArgList)
{
   StepResource pStep("Perform PCA", "app", "74394A49-702A-4609-83B9-69363E28488D");
   mpStep = pStep.get();

   try
   {
      bool bLoadFromFile = false;
      QString message;
      QString transformFilename;
      if(!extractInputArgs(pInArgList))
      {
         pStep->finalize(Message::Failure, "Unable to extract arguments.");
         return false;
      }

      if(mpRaster== NULL)
      {
         pStep->finalize(Message::Failure, "No raster element available.");
         return false;
      }

      const RasterDataDescriptor* pDescriptor =
         dynamic_cast<const RasterDataDescriptor*>(mpRaster->getDataDescriptor());
      if(pDescriptor == NULL)
      {
         pStep->finalize(Message::Failure, "PCA received null pointer to the data descriptor from the raster element");
         return false;
      }

      m_NumRows = pDescriptor->getRowCount();
      m_NumColumns = pDescriptor->getColumnCount();
      m_NumBands = pDescriptor->getBandCount();

      vector<string> aoiNames = mpModel->getElementNames(mpRaster, TypeConverter::toString<AoiElement>());
      int iResult = 0;

      if(mbInteractive)
      {
         PcaDlg dlg(aoiNames, m_NumBands, mpDesktop->getMainWidget());

         bool inputValid = false;
         while(!inputValid)
         {
            if(dlg.exec() == QDialog::Rejected)
            {
               pStep->finalize(Message::Abort);
               return false;
            }

            mbUseEigenValPlot = dlg.selectNumComponentsFromPlot();
            m_NumComponentsToUse = dlg.getNumComponents();
            m_OutputDataType = dlg.getOutputDataType();
            m_MaxScaleValue = dlg.getMaxScaleValue();

            transformFilename = dlg.getTransformFilename();
            if(!transformFilename.isEmpty())
            {
               bLoadFromFile = true;
            }
            else
            {
               QString strMethod = dlg.getCalcMethod();
               if(strMethod.contains("second", Qt::CaseInsensitive) > 0)
               {
                  m_CalcMethod = SECONDMOMENT;
               }
               else if(strMethod.contains("covariance", Qt::CaseInsensitive) > 0)
               {
                  m_CalcMethod = COVARIANCE;
               }
               else
               {
                  m_CalcMethod = CORRCOEF;
               }
            }

            m_ROIname = dlg.getRoiName();
            if(!m_ROIname.isEmpty())
            {
               pStep->addProperty("AOI", m_ROIname.toStdString());
               // check if any pixels are selected in the AOI
               AoiElement *pAoi = getAoiElement(m_ROIname.toStdString());
               VERIFY(pAoi != NULL);
               const BitMask *pMask = pAoi->getSelectedPoints();
               int numPoints = pMask->getCount();
               if(numPoints > 0)
               {
                  mb_UseAoi = true;
                  inputValid = true;
                  mp_AOIbitmask = reinterpret_cast<BitMask*>(mpObjFact->createObject("BitMask"));
                  mp_AOIbitmask->merge(*pMask);
                  int x1 = 0, y1 = 0, x2 = 0, y2 = 0;
                  mp_AOIbitmask->getBoundingBox(x1, y1, x2, y2);
                  const bool **pSelectedPixels = const_cast<BitMask*>(mp_AOIbitmask)->getRegion(x1, y1, x2, y2);
               }
               else
               {
                  message = "No pixels are currently selected in AOI";
                  QMessageBox::critical( NULL, "PCA", message );
                  pStep->finalize(Message::Failure, message.toStdString());
                  inputValid = false;
               }
            }
            else  // use whole image
            {
               m_ROIname = "Whole Image";
               mb_UseAoi = false;
               inputValid = true;
            }
         }
      }
      else // batch mode
      {
         // extract batch-mode only args
         string xformType;
         pInArgList->getPlugInArgValue("Transform Type", xformType);
         if(xformType == "Second Moment")
         {
            m_CalcMethod = SECONDMOMENT;
         }
         else if(xformType == "Covariance")
         {
            m_CalcMethod = COVARIANCE;
         }
         else if(xformType == "Correlation Coefficient")
         {
            m_CalcMethod = CORRCOEF;
         }
         else
         {
            pStep->finalize(Message::Failure, "Bad Transform Type!");
            return false;
         }

         pInArgList->getPlugInArgValue("Use Transform File", bLoadFromFile);
         if(bLoadFromFile)
         {
            Filename *pFn = pInArgList->getPlugInArgValue<Filename>("Transform Filename");
            if(pFn == NULL)
            {
               pStep->finalize(Message::Failure, "No Transform Filename specified!");
               return false;
            }
            transformFilename = QString::fromStdString(pFn->getFullPathAndName());
         }

         // arg extraction so we can continue and do the actual calculations
         int numComponents;
         if (pInArgList->getPlugInArgValue("Components", numComponents) == false)
         {
            pStep->finalize(Message::Failure, "Number of components not specified!");
            return false;
         }

#pragma message(__FILE__ "(" STRING(__LINE__) ") : warning : Number of Components input argument should be an unsigned int (dadkins)")
         m_NumComponentsToUse = static_cast<unsigned int>(numComponents);
         if (numComponents <= 0 || m_NumBands < m_NumComponentsToUse)
         {
            pStep->finalize(Message::Failure, "Invalid number of components specified!");
            return false;
         }

         //set default condition
         pInArgList->getPlugInArgValue<bool>("Use AOI", mb_UseAoi);
         if (mb_UseAoi == true)
         {
            bool aoiFailure = false;
            string roiName;
#pragma message(__FILE__ "(" STRING(__LINE__) ") : warning : AOI Name should be an AoiElement (dadkins)")
            if (pInArgList->getPlugInArgValue("AOI Name", roiName) == false)
            {
               pStep->finalize(Message::Failure, "Must specify the AOI name when choosing to process over an AOI!");
               return false;
            }

            // process the AOI
            m_ROIname = QString::fromStdString(roiName);
            if (m_ROIname.isEmpty())
            {
               aoiFailure = true;
            }
            else  // use the AOI
            {
               // check if any pixels are selected in the AOI
               AoiElement *pAoi = getAoiElement(m_ROIname.toStdString());
               if(pAoi == NULL)
               {
                  pStep->finalize(Message::Failure, "Specified AOI does not exist.");
                  return false;
               }
               BitMask* pMask = const_cast<BitMask*>(pAoi->getSelectedPoints());

               if((pMask != NULL) && (pMask->getCount() > 0))
               {
                  mp_AOIbitmask = reinterpret_cast<BitMask*>(mpObjFact->createObject("BitMask"));
                  mp_AOIbitmask->merge(*pMask);
               }
               else
               {
                  aoiFailure = true;
               }
            }

            if(aoiFailure)
            {
               // if any AOI loading failure, use the whole image!
               m_ROIname = "Whole Image";
               mb_UseAoi = false;
            }

         }

         // output data type argument
         if (pInArgList->getPlugInArgValue<EncodingType>("Output Encoding Type",
            m_OutputDataType) == false || m_OutputDataType.isValid() == false)
         {
            pStep->finalize(Message::Failure, "Invalid Output Encoding Type!");
            return false;
         }

         if (pInArgList->getPlugInArgValue("Max Scale Value", m_MaxScaleValue) == false)
         {
            pStep->finalize(Message::Failure, "Invalid Maximum Scale Value!");
            return false;
         }

         int maxThreshold = 0;
         switch(m_OutputDataType)
         {
         case INT1SBYTE:
            maxThreshold = numeric_limits<char>::max();
            break;
         case INT1UBYTE:
            maxThreshold = numeric_limits<unsigned char>::max();
            break;
         case INT2SBYTES:
            maxThreshold = numeric_limits<short>::max();
            break;
         case INT2UBYTES:
            maxThreshold = numeric_limits<unsigned short>::max();
            break;
         default:
            pStep->finalize(Message::Failure, "Invalid Output Encoding Type!");
            return false;
         }

         if (m_MaxScaleValue > maxThreshold)
         {
            pStep->finalize(Message::Failure, "Bad Maximum Scale Value!");
            return false;
         }
      }

      // Second Moment RasterElement
      mpSecondMomentMatrix = pInArgList->getPlugInArgValue<RasterElement>("Second Moment Matrix");

      // log PCA options
      if(bLoadFromFile)
      {
         pStep->addProperty("PCA File", transformFilename.toStdString());
      }
      else
      {
         pStep->addProperty("Number of Components", m_NumComponentsToUse);
         pStep->addProperty("Calculation Method", 
            (m_CalcMethod == SECONDMOMENT ? "Second Moment" :
                        (m_CalcMethod == COVARIANCE ? "Covariance" : "Correlation Coefficient")));
      }

      // create array for component coefficients
      MatrixFunctions::MatrixResource<double> pMatrixValues(m_NumBands, m_NumBands);
      mp_MatrixValues = pMatrixValues;
      if(!mp_MatrixValues)
      {
         pStep->finalize(Message::Failure, "Unable to obtain memory needed to calculate PCA coefficients");
         return false;
      }
      if(bLoadFromFile)
      {
         if(!m_ReadInPCAtransform(transformFilename))
         {
            if(mbAbort)
            {
               pStep->finalize(Message::Abort);
            }
            else
            {
               pStep->finalize(Message::Failure, "Error loading transform file");
            }

            return false;
         }
      }
      else
      {
         // get statistics to use for PCA
         if(!m_GetStatistics(aoiNames))
         {
            if(mbAbort)
            {
               pStep->finalize(Message::Abort);
            }
            else
            {
               pStep->finalize(Message::Failure, "Error determining statistics");
            }

            return false;
         }

         // Calculate PCA coefficients
         m_CalculateEigenValues();
         if(mbAbort)
         {
            if(mpProgress != NULL)
            {
               mpProgress->updateProgress("PCA Aborted", 0, ABORT);
            }

            pStep->finalize(Message::Abort);
            return false;
         }

         // Save PCA transform
         QString filename = QString::fromStdString(mpRaster->getFilename());

         switch(m_CalcMethod)
         {
         case SECONDMOMENT:
            filename += ".pcasmm";
            break;
         case COVARIANCE:
            filename += ".pcacvm";
            break;
         case CORRCOEF:
            filename += ".pcaccm";
            break;
         }
         if(mbInteractive)
         {
            filename = QFileDialog::getSaveFileName(NULL, "Choose filename to save PCA Transform",
               filename, "PCA files (*.pca*);;All Files (*.*)");
         }
         if(!filename.isEmpty())
         {
            m_WriteOutPCAtransform(filename);
         }
      }

      // Create the PCA sensor data
      if(!m_CreatePCACube())
      {
         if(mbAbort)
         {
            pStep->finalize(Message::Abort);
         }
         else
         {
            pStep->finalize(Message::Failure, "Error allocating result cube");
         }

         return false;
      }

      // compute PCAcomponents
      bool bSuccess;
      if (mb_UseAoi)
      {
         bSuccess = m_ComputePCAaoi();
      }
      else
      {
         bSuccess = m_ComputePCAwhole();
      }

      if(!bSuccess)
      {
         mpModel->destroyElement(mpPCARaster);
         if(mbAbort)
         {
            pStep->finalize(Message::Abort);
         }
         else
         {
            pStep->finalize(Message::Failure, "Error computing PCA components");
         }

         return false;
      }

      // Create the spectral data window
      if(!m_CreatePCAView())
      {
         if(mpPCARaster!= NULL)
         {
            mpModel->destroyElement(mpPCARaster);
            if(mbAbort)
            {
               pStep->finalize(Message::Abort);
            }
            else
            {
               pStep->finalize(Message::Failure, "Error creating view");
            }

            return false;
         }
      }

      // Set the corrected cube value in the output arg list
      PlugInArg* pArg = NULL;
      if(!pOutArgList->getArg("Corrected Data Cube", pArg) || (pArg == NULL))
      {
         if (mpPCARaster!= NULL)
         {
            mpModel->destroyElement(mpPCARaster);
            if(mbAbort)
            {
               pStep->finalize(Message::Abort);
            }
            else
            {
               pStep->finalize(Message::Failure, "Error setting output argument");
            }

            return false;
         }
      }

      pArg->setActualValue(static_cast<void*>(mpPCARaster));
      if(mpProgress != NULL)
      {
         mpProgress->updateProgress("Principle Component Analysis completed",100,NORMAL);
      }
   }
   catch(const bad_alloc&)
   {
      if(mpProgress != NULL)
      {
         mpProgress->updateProgress("Out of memory",0,ERRORS);
      }
      pStep->finalize(Message::Failure, "Out of memory");

      return false;
   }
   pStep->finalize(Message::Success);

   return true;
}

bool PCA::abort()
{
   Executable* pSecondMoment = dynamic_cast<Executable*>(mpSecondMoment->getPlugIn());
   if(pSecondMoment != NULL)
   {
      pSecondMoment->abort();
   }

   mbAbort = true;
   return true;
}

bool PCA::extractInputArgs(PlugInArgList* pArgList)
{
   if(pArgList == NULL)
   {
      mpStep->finalize(Message::Failure, "PCA received null input argument list");
      return false;
   }

   mpProgress = pArgList->getPlugInArgValue<Progress>(ProgressArg());
   mpRaster = pArgList->getPlugInArgValue<RasterElement>(DataElementArg());
   if (mpRaster == NULL)
   {
      mMessage = "The input raster element was null";
      if (mpProgress != NULL)
      {
         mpProgress->updateProgress(mMessage, 0, ERRORS);
      }
      mpStep->finalize(Message::Failure, mMessage);
      return false;
   }

   const RasterDataDescriptor* pDescriptor = dynamic_cast<const RasterDataDescriptor*>(mpRaster->getDataDescriptor());
   if (pDescriptor == NULL)
   {
      mMessage = "Unable to access data descriptor for original data set!";
      if (mpProgress != NULL)
      {
         mpProgress->updateProgress(mMessage, 0, ERRORS);
      }
      mpStep->finalize(Message::Failure, mMessage);
      return false;
   }

   EncodingType dataType = pDescriptor->getDataType();
   if((dataType == INT4SCOMPLEX) || (dataType == FLT8COMPLEX))
   {
      mMessage = "Complex data is not supported!";
      if (mpProgress != NULL)
      {
         mpProgress->updateProgress(mMessage, 0, ERRORS);
      }
      mpStep->finalize(Message::Failure, mMessage);
      return false;
   }

   if (pDescriptor->getBandCount() == 1)
   {
      mMessage = "Cannot perform PCA on 1 band data!";
      if (mpProgress != NULL)
      {
         mpProgress->updateProgress(mMessage, 0, ERRORS);
      }
      mpStep->finalize(Message::Failure, mMessage);
      return false;
   }

   return true;
}

bool PCA::m_CreatePCACube()
{
   string filename = mpRaster->getFilename();
   if(filename.empty())
   {
      mMessage = "Could not access the cube's filename!";
      if(mpProgress != NULL) mpProgress->updateProgress(mMessage, 0, ERRORS);
      mpStep->finalize(Message::Failure, mMessage);
      return false;
   }

   StepResource pStep(mMessage, "app", "2EA8BA45-E826-4bb1-AB94-968D29184F80", "Can't create spectral cube");

   int loc = filename.rfind('.');
   if (loc == string::npos)
   {
      loc = filename.length();
   }
   string addOn = "";
   switch(m_CalcMethod)
   {
   case SECONDMOMENT:
      addOn = "_pca_smm";
      break;
   case COVARIANCE:
      addOn = "_pca_cvm";
      break;
   case CORRCOEF:
      addOn = "_pca_ccm";
      break;
   }
   filename = filename.insert(loc, addOn);

   const RasterDataDescriptor* pDescriptor = dynamic_cast<const RasterDataDescriptor*>(mpRaster->getDataDescriptor());
   if (pDescriptor == NULL)
   {
      mMessage = "Could not access the cube's data descriptor!";
      if(mpProgress != NULL) mpProgress->updateProgress(mMessage, 0, ERRORS);
      pStep->finalize(Message::Failure, mMessage);
      return false;
   }

   ProcessingLocation outLocation = pDescriptor->getProcessingLocation();

   mpPCARaster = RasterUtilities::createRasterElement(filename, m_NumRows, m_NumColumns, m_NumComponentsToUse,
      m_OutputDataType, BIP,
      (outLocation == IN_MEMORY ? true : false), NULL);

   if (mpPCARaster == NULL)
   {
      mMessage = "Unable to create a new raster element!";
      if(mpProgress != NULL) mpProgress->updateProgress(mMessage, 0, ERRORS);
      pStep->finalize(Message::Failure, mMessage);
      return false;
   }

   RasterDataDescriptor* pRdd = dynamic_cast<RasterDataDescriptor*>(mpPCARaster->getDataDescriptor());
   if (pRdd == NULL)
   {
      mMessage = "Unable to create a new raster element!";
      if(mpProgress != NULL) mpProgress->updateProgress(mMessage, 0, ERRORS);
      pStep->finalize(Message::Failure, mMessage);
      return false;
   }
   const Classification* pClass = mpRaster->getClassification();
   if(pClass != NULL)
   {
      pRdd->setClassification(pClass);
   }

   // Bad values
   vector<int>badValue(1);
   badValue[0] = 0;
   pRdd->setBadValues(badValue);

   // TODO: Units

   pStep->finalize(Message::Success);
   return true;
}

bool PCA::m_ComputePCAwhole()
{
   QString message;

   const RasterDataDescriptor* pPcaDesc = dynamic_cast<RasterDataDescriptor*>(mpPCARaster->getDataDescriptor());
   EncodingType PcaDataType = pPcaDesc->getDataType();
   unsigned int PcaNumRows = pPcaDesc->getRowCount();
   unsigned int PcaNumCols = pPcaDesc->getColumnCount();
   unsigned int PcaNumBands = pPcaDesc->getBandCount();
   if((PcaNumRows != m_NumRows) || (PcaNumCols != m_NumColumns) || (PcaNumBands != m_NumComponentsToUse))
   {
      mMessage = "The dimensions of the PCA RasterElement are not correct.";
      if(mpProgress != NULL) mpProgress->updateProgress(mMessage, 0, ERRORS);
      mpStep->finalize(Message::Failure, mMessage);
      return false;
   }

   FactoryResource<DataRequest> pBipRequest;
   pBipRequest->setInterleaveFormat(BIP);
   DataAccessor PcaAccessor = mpPCARaster->getDataAccessor(pBipRequest->copy());
   if(!PcaAccessor.isValid())
   {
      mMessage = "PCA could not obtain an accessor the PCA RasterElement";
      if(mpProgress != NULL) mpProgress->updateProgress(mMessage, 0, ERRORS);
      mpStep->finalize(Message::Failure, mMessage);
      return false;
   }

   const RasterDataDescriptor* pOrigDescriptor = dynamic_cast<const RasterDataDescriptor*>
      (mpRaster->getDataDescriptor());
   if (pOrigDescriptor == NULL)
   {
      mMessage = "PCA received null pointer to the source data descriptor";
      if(mpProgress != NULL) mpProgress->updateProgress(mMessage, 0, ERRORS);
      mpStep->finalize(Message::Failure, mMessage);
      return false;
   }

   EncodingType eDataType = pOrigDescriptor->getDataType();
   if(eDataType == UNKNOWN)
   {
      mMessage = "PCA received invalid value for source data encoding type";
      if(mpProgress != NULL) mpProgress->updateProgress(mMessage, 0, ERRORS);
      mpStep->finalize(Message::Failure, mMessage);
      return false;
   }

   // Initialize progress bar variables
   int FractionalProgress = (m_NumRows * m_NumColumns) / 100;
   int CurrentProgress = 0;
   int count = 0;

   void* pPCAData = NULL;
   void* pOrigData = NULL;

   DataAccessor OrigAccessor = mpRaster->getDataAccessor(pBipRequest->copy());
   if(!OrigAccessor.isValid())
   {
      mMessage = "Could not get the pixels in the original cube!";
      if(mpProgress != NULL) mpProgress->updateProgress(mMessage, CurrentProgress, ERRORS);
      mpStep->finalize(Message::Failure, mMessage);
      return false;
   }

   // Set the PCA cube data values
   double min = 0.0, max = 0.0, scalefactor = 0.0;
   int memsize = m_NumRows * m_NumColumns * sizeof(float);
   string CompValuesName = "PcaComponentValues";
   RasterElement *pOldComponentValues = dynamic_cast<RasterElement*>(
      Service<ModelServices>()->getElement(CompValuesName, TypeConverter::toString<RasterElement>(), mpRaster));
   if (pOldComponentValues != NULL)
   {
      VERIFY(Service<ModelServices>()->destroyElement(pOldComponentValues));
   }

   ModelResource<RasterElement> pComponentValues(RasterUtilities::createRasterElement(CompValuesName,
      m_NumRows, m_NumColumns, FLT8BYTES, true, mpRaster));
   if(pComponentValues.get() == NULL)
   {
      mMessage = "Out of memory";
      if(mpProgress != NULL) mpProgress->updateProgress(mMessage, 0, ERRORS);
      mpStep->finalize(Message::Failure, mMessage);
      return false;
   }

   FactoryResource<DataRequest> pBipWritableRequest;
   pBipWritableRequest->setWritable(true);
   pBipWritableRequest->setInterleaveFormat(BIP);

   unsigned int comp = 0, row = 0, band= 0;
   double* pValues = NULL;
   vector<double> coefficients(m_NumBands, 0);
   double* pCoefficients = &coefficients.front();
   for(comp = 0; comp < m_NumComponentsToUse; comp++)
   {
      OrigAccessor = mpRaster->getDataAccessor(pBipRequest->copy());

      DataAccessor CompValAccessor = pComponentValues->getDataAccessor(pBipWritableRequest->copy());
      for(band = 0; band < m_NumBands; band++)
      {
         pCoefficients[band] = mp_MatrixValues[band][comp];
      }
      min = numeric_limits<double>::max();
      max = numeric_limits<double>::min();
      for(row = 0; row < m_NumRows; row++)
      {
         VERIFY(OrigAccessor.isValid());
         pOrigData = OrigAccessor->getRow();
         VERIFY(CompValAccessor.isValid());
         pValues = reinterpret_cast<double*>(CompValAccessor->getRow());
         switchOnEncoding(eDataType,ComputePcaRow,pOrigData,pValues,
                           pCoefficients,m_NumColumns,m_NumBands,&min,&max);
         OrigAccessor->nextRow();
         CompValAccessor->nextRow();
      }

      // check if aborted
      if(!mbAbort)
      {
         // scale component values and save in pPCACube
         scalefactor = double(m_MaxScaleValue) / (max - min);

         FactoryResource<DataRequest> pPcaRequest;
         pPcaRequest->setBands(pPcaDesc->getActiveBand(comp), pPcaDesc->getActiveBand(comp));
         pPcaRequest->setWritable(true);
         DataAccessor PcaAccessor = mpPCARaster->getDataAccessor(pPcaRequest.release());
         if(!PcaAccessor.isValid())
         {
            mMessage = "Could not get the pixels in the PCA cube!";
            if(mpProgress != NULL) mpProgress->updateProgress(mMessage, CurrentProgress, ERRORS);
            mpStep->finalize(Message::Failure, mMessage);
            return false;
         }

         CompValAccessor = pComponentValues->getDataAccessor(pBipRequest->copy());
         for(row = 0; row < m_NumRows; row++)
         {
            VERIFY(PcaAccessor.isValid());
            pPCAData = PcaAccessor->getRow();
            VERIFY(CompValAccessor.isValid());
            pValues  = reinterpret_cast<double*>(CompValAccessor->getRow());
            switchOnEncoding(m_OutputDataType,StorePcaRow,pPCAData,pValues,PcaNumCols,PcaNumBands,min,scalefactor);
            PcaAccessor->nextRow();
            CompValAccessor->nextRow();

            if(mbAbort)
            {
               break;
            }
         }

         CurrentProgress = 100 * (comp + 1) / m_NumComponentsToUse;
         if(mpProgress != NULL) mpProgress->updateProgress("Generating scaled PCA data cube...",
                                             CurrentProgress, NORMAL);
      }
   }

   if(mbAbort)
   {
      mpProgress->updateProgress("PCA aborted!", CurrentProgress, ABORT);
      mpStep->finalize(Message::Abort);
      return false;
   }
   if(mpProgress != NULL) mpProgress->updateProgress("PCA computations complete!", 100, NORMAL);

   return true;
}

bool PCA::m_ComputePCAaoi()
{
   const RasterDataDescriptor *pPcaDescriptor = dynamic_cast<const RasterDataDescriptor*>(
      mpPCARaster->getDataDescriptor());
   if (pPcaDescriptor == NULL)
   {
      mMessage = "PCA received null pointer to the PCA data RaterElement";
      if(mpProgress != NULL) mpProgress->updateProgress(mMessage, 0, ERRORS);
      mpStep->finalize(Message::Failure, mMessage);
      return false;
   }

   const RasterDataDescriptor* pOrigDescriptor = dynamic_cast<const RasterDataDescriptor*>
      (mpRaster->getDataDescriptor());
   if (pOrigDescriptor == NULL)
   {
      mMessage = "PCA received null pointer to the source data RasterElement";
      if(mpProgress != NULL) mpProgress->updateProgress(mMessage, 0, ERRORS);
      mpStep->finalize(Message::Failure, mMessage);
      return false;
   }
   int NumRows = pOrigDescriptor->getRowCount();
   int NumCols = pOrigDescriptor->getColumnCount();

   EncodingType eDataType = pOrigDescriptor->getDataType();
   if(eDataType == UNKNOWN)
   {
      mMessage = "PCA received invalid value for source data encoding type";
      if(mpProgress != NULL) mpProgress->updateProgress(mMessage, 0, ERRORS);
      mpStep->finalize(Message::Failure, mMessage);
      return false;
   }

   // Initialize progress bar variables
   int CurrentProgress = 0;
   int count = 0;

   void* pPCAData = NULL;
   void* pOrigData = NULL;

   // Set the PCA cube data values
   double min = 0.0, max = 0.0, scalefactor = 0.0;
   int x1 = 0, y1 = 0, x2 = 0, y2 = 0;
   mp_AOIbitmask->getBoundingBox(x1, y1, x2, y2);
   x1 = std::max(x1, 0);
   x1 = std::min(x1, NumCols - 1);
   y1 = std::max(y1, 0);
   y1 = std::min(y1, NumRows - 1);

   const bool **pSelectedPixels = const_cast<BitMask*>(mp_AOIbitmask)->getRegion(x1, y1, x2, y2);
   int numRows = y2 - y1 + 1;
   int numCols = x2 - x1 + 1;
   int numPixels = mp_AOIbitmask->getCount();

   string CompValuesName = "PcaComponentValues";
   RasterElement *pOldComponentValues = dynamic_cast<RasterElement*>(
      Service<ModelServices>()->getElement(CompValuesName, TypeConverter::toString<RasterElement>(), mpRaster));
   if (pOldComponentValues != NULL)
   {
      VERIFY(Service<ModelServices>()->destroyElement(pOldComponentValues));
   }

   ModelResource<RasterElement> pComponentValues(RasterUtilities::createRasterElement(CompValuesName,
      numRows, numCols, FLT8BYTES, true, mpRaster));
   if(pComponentValues.get() == NULL)
   {
      mMessage = "Out of memory";
      if(mpProgress != NULL) mpProgress->updateProgress(mMessage, CurrentProgress, ERRORS);
      mpStep->finalize(Message::Failure, mMessage);
      return false;
   }

   int comp = 0, row = 0, col = 0, band = 0, i = 0, j = 0;
   vector<double> coefficients(m_NumBands, 0);
   double* pCoefficients = &coefficients.front();
   double* pTempVal;
   for(comp = 0; comp < static_cast<int>(m_NumComponentsToUse); comp++)
   {
      FactoryResource<DataRequest> pRequest;
      pRequest->setInterleaveFormat(BIP);
      pRequest->setRows(pOrigDescriptor->getActiveRow(y1), pOrigDescriptor->getActiveRow(y2));
      pRequest->setColumns(pOrigDescriptor->getActiveColumn(x1), pOrigDescriptor->getActiveColumn(x2));
      pRequest->setBands(pOrigDescriptor->getActiveBand(comp), pOrigDescriptor->getActiveBand(comp));
      DataAccessor accessor = mpRaster->getDataAccessor(pRequest.release());
      if(!accessor.isValid())
      {
         mMessage = "Could not get the pixels in the original cube!";
         if(mpProgress != NULL) mpProgress->updateProgress(mMessage, CurrentProgress, ERRORS);
         mpStep->finalize(Message::Failure, mMessage);
         return false;
      }

      FactoryResource<DataRequest> pWritableRequest;
      pWritableRequest->setWritable(true);
      DataAccessor CompValAccessor = pComponentValues->getDataAccessor(pWritableRequest.release());

      for(band = 0; band < static_cast<int>(m_NumBands); band++)
      {
         pCoefficients[band] = mp_MatrixValues[band][comp];
      }
      min = numeric_limits<double>::max();
      max = numeric_limits<double>::min();
      for(row = y1, i = 0; row <= y2; row++, i++)
      {
         VERIFY(accessor.isValid());
         VERIFY(CompValAccessor.isValid());
         for(col = x1, j = 0; col <= x2; col++, j++)
         {
            if(pSelectedPixels[i][j])
            {
               pTempVal = reinterpret_cast<double*>(CompValAccessor->getColumn());
               pOrigData = accessor->getColumn();
               if(pOrigData == NULL)
               {
                  mMessage = "Could not get the pixels in the Original cube!";
                  if(mpProgress != NULL) mpProgress->updateProgress(mMessage,
                                                CurrentProgress, ERRORS);
                  mpStep->finalize(Message::Failure, mMessage);
                  return false;
               }

               switchOnEncoding(eDataType,ComputePcaValue,pOrigData,pTempVal,pCoefficients,m_NumBands);

               if(*pTempVal > max)
               {
                  max = *pTempVal;
               }
               if(*pTempVal < min)
               {
                  min = *pTempVal;
               }

               if(mbAbort)
               {
                  break;
               }
            }
            accessor->nextColumn();
            CompValAccessor->nextColumn();
         }
         accessor->nextRow();
         CompValAccessor->nextRow();
      }

      if(!mbAbort)
      {
         scalefactor = double(m_MaxScaleValue) / (max - min);

         FactoryResource<DataRequest> pPcaRequest;
         pPcaRequest->setRows(pPcaDescriptor->getActiveRow(y1), pPcaDescriptor->getActiveRow(y2));
         pPcaRequest->setColumns(pPcaDescriptor->getActiveColumn(x1), pPcaDescriptor->getActiveColumn(x2));
         pPcaRequest->setBands(pPcaDescriptor->getActiveBand(comp), pPcaDescriptor->getActiveBand(comp));
         pPcaRequest->setWritable(true);
         DataAccessor PcaAccessor = mpPCARaster->getDataAccessor(pPcaRequest.release());

         if(!PcaAccessor.isValid())
         {
            mMessage = "Could not get the pixels in the PCA cube!";
            if(mpProgress != NULL) mpProgress->updateProgress(mMessage, CurrentProgress, ERRORS);
            mpStep->finalize(Message::Failure, mMessage);
            return false;
         }

         CompValAccessor = pComponentValues->getDataAccessor();
         for(row = y1, i = 0; row <= y2; row++, i++)
         {
            for(col = x1, j = 0; col <= x2; col++, j++)
            {
               if(pSelectedPixels[i][j])
               {
                  pPCAData = PcaAccessor->getColumn();
                  pTempVal = reinterpret_cast<double*>(CompValAccessor->getColumn());
                  if(pPCAData == NULL)
                  {
                     mMessage = "Could not get the pixels in the PCA cube!";
                     if(mpProgress != NULL) mpProgress->updateProgress(mMessage, CurrentProgress, ERRORS);
                     mpStep->finalize(Message::Failure, mMessage);
                     return false;
                  }

                  switchOnEncoding(m_OutputDataType,StorePcaValue,pPCAData,pTempVal,&min,&scalefactor);
               }
               PcaAccessor->nextColumn();
               CompValAccessor->nextColumn();
            }
            PcaAccessor->nextRow();
            CompValAccessor->nextRow();
            if(mbAbort)
            {
               break;
            }
         }

         CurrentProgress = 100 * (comp + 1) / m_NumComponentsToUse;
         if(mpProgress != NULL)
         {
            mpProgress->updateProgress("Generating scaled PCA data cube...", CurrentProgress, NORMAL);
         }
      }
   }

   if(mbAbort)
   {
      mpProgress->updateProgress("PCA aborted!", CurrentProgress, ABORT);
      mpStep->finalize(Message::Abort);
      return false;
   }

   if(mpProgress != NULL)
   {
      mpProgress->updateProgress("PCA computations complete!", 99, NORMAL);
   }

   return true;
}

bool PCA::m_CreatePCAView()
{
   if(mbInteractive) // only create a view in interactive mode!
   {
      if(mpProgress != NULL) mpProgress->updateProgress("Creating view...", 0, NORMAL);

      string filename = mpPCARaster->getName();

      if(mpProgress != NULL) mpProgress->updateProgress("Creating view...", 25, NORMAL);

      SpatialDataWindow* pWindow = static_cast<SpatialDataWindow*>(
         mpDesktop->createWindow(filename.c_str(), SPATIAL_DATA_WINDOW));
      if (pWindow == NULL)
      {
         mMessage = "Could not create new window!";
         if(mpProgress != NULL) mpProgress->updateProgress(mMessage, 25, ERRORS);
         mpStep->finalize(Message::Failure, mMessage);
         return false;
      }

      SpatialDataView* pView = NULL;
      if(pWindow != NULL)
      {
         pView = pWindow->getSpatialDataView();
      }

      if(pView == NULL)
      {
         mMessage = "Could not obtain new view!";
         if(mpProgress != NULL) mpProgress->updateProgress(mMessage, 25, ERRORS);
         mpStep->finalize(Message::Failure, mMessage);
         return false;
      }

      pView->setPrimaryRasterElement(mpPCARaster);

      if(mpProgress != NULL) mpProgress->updateProgress("Creating view...", 50, NORMAL);

      UndoLock lock(pView);
      if (pView->createLayer(RASTER, mpPCARaster) == NULL)
      {
         mpDesktop->deleteWindow(pWindow);
         mpPCARaster = NULL;
         mMessage = "Could not access raster properties for view!";
         if(mpProgress != NULL) mpProgress->updateProgress(mMessage, 50, ERRORS);
         mpStep->finalize(Message::Failure, mMessage);
         return false;
      }

      if(mpProgress != NULL) mpProgress->updateProgress("Creating view...", 75, NORMAL);

      // Create a GCP layer if available
      if (mpRaster != NULL)
      {
         const RasterDataDescriptor* pDescriptor =
            dynamic_cast<const RasterDataDescriptor*>(mpRaster->getDataDescriptor());
         if (pDescriptor != NULL)
         {
            const RasterFileDescriptor* pFileDescriptor =
               dynamic_cast<const RasterFileDescriptor*>(pDescriptor->getFileDescriptor());
            if (pFileDescriptor != NULL)
            {
               Service<ModelServices> pModel;
               if (pModel.get() != NULL)
               {
                  list<GcpPoint> gcps;
                  if ((m_NumRows == pFileDescriptor->getRowCount()) &&
                     (m_NumColumns == pFileDescriptor->getColumnCount()))
                  {
                     gcps = pFileDescriptor->getGcps();
                  }

                  if (gcps.empty() == true)
                  {
                     if (mpRaster->isGeoreferenced())
                     {
                        GcpPoint gcp;

                        // Lower left
                        gcp.mPixel.mX = 0.0;
                        gcp.mPixel.mY = 0.0;
                        gcp.mCoordinate = mpRaster->convertPixelToGeocoord(gcp.mPixel);
                        gcps.push_back(gcp);

                        // Lower right
                        gcp.mPixel.mX = m_NumColumns - 1;
                        gcp.mPixel.mY = 0.0;
                        gcp.mCoordinate = mpRaster->convertPixelToGeocoord(gcp.mPixel);
                        gcps.push_back(gcp);

                        // Upper left
                        gcp.mPixel.mX = 0.0;
                        gcp.mPixel.mY = m_NumRows - 1;
                        gcp.mCoordinate = mpRaster->convertPixelToGeocoord(gcp.mPixel);
                        gcps.push_back(gcp);

                        // Upper right
                        gcp.mPixel.mX = m_NumColumns - 1;
                        gcp.mPixel.mY = m_NumRows - 1;
                        gcp.mCoordinate = mpRaster->convertPixelToGeocoord(gcp.mPixel);
                        gcps.push_back(gcp);

                        // Center
                        gcp.mPixel.mX = m_NumColumns / 2.0;
                        gcp.mPixel.mY = m_NumRows / 2.0;
                        gcp.mCoordinate = mpRaster->convertPixelToGeocoord(gcp.mPixel);
                        gcps.push_back(gcp);
                     }
                  }

                  if (gcps.empty() == false)
                  {
                     DataDescriptor* pGcpDescriptor = pModel->createDataDescriptor("Corner Coordinates",
                        "GcpList", mpPCARaster);
                     if (pGcpDescriptor != NULL)
                     {
                        GcpList* pGcpList = static_cast<GcpList*>(pModel->createElement(pGcpDescriptor));
                        if (pGcpList != NULL)
                        {
                           // Add the GCPs to the GCP list
                           pGcpList->addPoints(gcps);

                           // Create the GCP list layer
                           pView->createLayer(GCP_LAYER, pGcpList);
                        }
                     }
                  }
                  else
                  {
                     string message = "Geocoordinates are not available and will not be added to the new PCA cube!";
                     if (mpProgress != NULL)
                     {
                        mpProgress->updateProgress(message, 0, WARNING);
                     }

                     if (mpStep != NULL)
                     {
                        mpStep->addMessage(message, "app", "FCD1A3B0-9CA3-41D5-A93B-D9D0DBE0222C", true);
                     }
                  }
               }
            }
         }
      }

      if(!mbAbort)
      {
         if(mpProgress != NULL) mpProgress->updateProgress("Finished creating view...", 100, NORMAL);
      }
      else
      {
         mpDesktop->deleteWindow(pWindow);
         mpPCARaster = NULL;
         if(mpProgress != NULL) mpProgress->updateProgress("Create view aborted", 100, NORMAL);
         mpStep->finalize(Message::Abort);
         return false;
      }
   }

   return true;
}

void PCA::m_CalculateEigenValues()
{
   StepResource pStep("Calculate Eigen Values", "app", "640DF72A-BBFC-4f17-877A-058C6B70B701");

   unsigned int lBandIndex;
   vector<double> eigenValues(m_NumBands);
   double *pEigenValues = &eigenValues.front();

   if(mpProgress != NULL)
   {
      mpProgress->updateProgress("Calculating Eigen Values...", 0, NORMAL);
   }

   // Get the eigenvalues and eigenvectors. Store the eigenvectors in mp_MatrixValues for future use.
   if (MatrixFunctions::getEigenvalues(const_cast<const double**>(mp_MatrixValues),
      pEigenValues, mp_MatrixValues, m_NumBands) == false)
   {
      pStep->finalize(Message::Failure, "Unable to calculate eigenvalues.");
      if(mpProgress != NULL)
      {
         mpProgress->updateProgress(pStep->getFailureMessage(), 100, ERRORS);
      }
   }

   if(mpProgress != NULL) mpProgress->updateProgress("Calculating Eigen Values...", 80, NORMAL);
   double dEigen_Sum = 0.0, dEigen_Current = 0.0, dTemp = 0.0;
   int lNoise_Cutoff = 1;
   for(lBandIndex = 0; lBandIndex < m_NumBands; lBandIndex++)
   {
      dEigen_Sum += pEigenValues[lBandIndex];
   }

   if(mpProgress != NULL) mpProgress->updateProgress("Calculating Eigen Values...",90,NORMAL);
   for(lBandIndex = 0; lBandIndex < m_NumBands; lBandIndex++)
   {
      dEigen_Current += pEigenValues[lBandIndex];
      dTemp = 100.0 * dEigen_Current / dEigen_Sum;
      if(dTemp < 99.99)
      {
         lNoise_Cutoff++;
      }
   }
   pStep->addProperty("Noise cutoff", lNoise_Cutoff);

   if(mpProgress != NULL) mpProgress->updateProgress("Calculation of Eigen Values completed", 100, NORMAL);

   // check if user wanted to select num components based on Eigen value plot
   if(mbUseEigenValPlot)
   {
      EigenPlotDlg plotDlg(mpDesktop->getMainWidget());
      plotDlg.setEigenValues(pEigenValues, m_NumBands);
      if(plotDlg.exec() == QDialog::Rejected)
      {
         abort();
      }
      m_NumComponentsToUse = plotDlg.getNumComponents();
   }
   pStep->finalize(Message::Success);
}

bool PCA::m_GetStatistics(vector<string> aoiList)
{
   double *pdTemp = NULL;
   QString strFilename;

   string filename = mpRaster->getFilename();
   if (filename.empty() == false)
   {
      strFilename = QString::fromStdString(filename);
   }

   QString message;
   bool bLoadFromFile = false;
   bool success = false;
   switch(m_CalcMethod)
   {
   case SECONDMOMENT:
      {
         if (mpSecondMomentMatrix == NULL)
         {
            ExecutableResource secondMoment("Second Moment", "", mpProgress);
            VERIFY(secondMoment->getPlugIn() != NULL);

            bool recalculate = true;
            secondMoment->getInArgList().setPlugInArgValue(DataElementArg(), mpRaster);
            secondMoment->getInArgList().setPlugInArgValue("Recalculate", &recalculate);
            if (mb_UseAoi == true)
            {
               AoiElement *pAoi = getAoiElement(m_ROIname.toStdString());
               secondMoment->getInArgList().setPlugInArgValue("AOI", pAoi);
            }
            mpSecondMoment = secondMoment;
            secondMoment->execute();
            mpSecondMomentMatrix = secondMoment->getOutArgList()
               .getPlugInArgValue<RasterElement>("Second Moment Matrix");
            mpSecondMoment = ExecutableResource();

            if (mpSecondMomentMatrix == NULL)
            {
               message = "Could not obtain Second Moment Matrix";
               if(mpProgress != NULL)
               {
                  mpProgress->updateProgress(message.toStdString(),0,ERRORS);
               }
               mpStep->finalize(Message::Failure, mMessage);
               return false;
            }
         }

         pdTemp = reinterpret_cast<double*>(mpSecondMomentMatrix->getRawData());
         if (pdTemp == NULL)
         {
            mMessage = "Unable to access data in Second Moment Matrix";
            if (mpProgress != NULL)
            {
               mpProgress->updateProgress(mMessage,0,ERRORS);
            }
            mpStep->finalize(Message::Failure, mMessage);
            return false;
         }

         int lOffset = 0;
         for(unsigned int row = 0; row < m_NumBands; row++)
         {
            memcpy(mp_MatrixValues[row], &pdTemp[lOffset], m_NumBands * sizeof(pdTemp[lOffset]));
            lOffset += m_NumBands;

            mpProgress->updateProgress("Reordering Second Moment Matrix...", 100 * row / m_NumBands, NORMAL);
            if(mbAbort)
            {
               break;
            }
         }

         if(!mbAbort)
         {
            mpProgress->updateProgress("Matrix retrieval complete", 100, NORMAL);
            success = true;
         }
         else
         {
            mpProgress->updateProgress("Matrix retrieval aborted by user", 100, NORMAL);
            success = false;
         }

         break;
      }

   case COVARIANCE:
      {
         strFilename += ".cvm";
         if(mbInteractive)
         {
            if (QFile::exists(strFilename))
            {
               bLoadFromFile = !QMessageBox::information(NULL,
                                             "Covariance Algorithm",
                                             "A covariance matrix file has been found for this image.\n"
                                                      "Do you want to use it?",
                                             "Use",
                                             "Don't Use");
            }
            if(bLoadFromFile)
            {
               if(!m_ReadMatrixFromFile(strFilename,mp_MatrixValues, m_NumBands,"Covariance"))
               {
                  // error logged in m_ReadMatrixFromFile routine
                  return false;
               }
            }
            else
            {
               StatisticsDlg sDlg("Covariance", aoiList, mpDesktop->getMainWidget());
               if(sDlg.exec() == QDialog::Rejected)
               {
                  mpStep->finalize(Message::Abort);
                  return false;
               }

               QString strAoiName = sDlg.getAoiName();
               if(!strAoiName.isEmpty())
               {
                  success = m_ComputeCovarianceMatrix(strAoiName);
               }
               else // compute covariance over whole image using skip factors
               {
                  success = m_ComputeCovarianceMatrix(QString(), sDlg.getRowFactor(), sDlg.getColumnFactor());
               }
            }
         }
         else
         {
            // compute covariance over whole image using every pixel
            success = m_ComputeCovarianceMatrix(QString(), 1, 1);
         }

         if(success && !bLoadFromFile)
         {
            m_WriteMatrixToFile(strFilename, const_cast<const double**>(mp_MatrixValues), m_NumBands, "Covariance");
         }

         break;
      }
   case CORRCOEF:
      {
         strFilename += ".ccm";
         if(mbInteractive)
         {
            if (QFile::exists(strFilename))
            {
               bLoadFromFile = !QMessageBox::information(NULL, "Correlation Coefficient Algorithm",
                                              "A correlation coefficient matrix file has been found for this image.\n"
                                                      "Do you want to use it?",
                                              "Use",
                                              "Don't Use");
            }
         }
         else
         {
            bLoadFromFile = false;
         }

         if(bLoadFromFile)
         {
            if(!m_ReadMatrixFromFile(strFilename, mp_MatrixValues, m_NumBands, "Correlation Coefficient"))
            {
               return false; // error logged in m_ReadMatrixFromFile routine
            }
         }
         else
         {
            if(mbInteractive)
            {
               StatisticsDlg sDlg("Correlation Coefficient", aoiList, mpDesktop->getMainWidget());
               if(sDlg.exec() == QDialog::Rejected)
               {
                  mpStep->finalize(Message::Abort);
                  return false;
               }

               QString strAoiName = sDlg.getAoiName();
               if(!strAoiName.isEmpty())
               {
                  success = m_ComputeCovarianceMatrix(strAoiName);
               }
               else // compute covariance over whole image using skip factors
               {
                  success = m_ComputeCovarianceMatrix(QString(), sDlg.getRowFactor(),sDlg.getColumnFactor());
               }
            }
            else // compute covariance over whole image using every pixel
            {
               success = m_ComputeCovarianceMatrix(QString(), 1, 1);
            }

            if(success)
            {
               // check if allocated
               vector<double> stdDev(m_NumBands);
               if(mpProgress != NULL)
               {
                  mpProgress->updateProgress("Computing Correlation Coefficients from Covariances...", 0, NORMAL);
               }
               for(unsigned int band = 0; band < m_NumBands; band++)
               {
                  stdDev[band] = sqrt(mp_MatrixValues[band][band]);
               }
               for(unsigned int band2 = 0; band2 < m_NumBands; band2++)
               {
                  for(unsigned int band1 = 0; band1 < m_NumBands; band1++)
                  {
                     if(band1 == band2)
                     {
                        mp_MatrixValues[band2][band1] = 1.0;
                     }
                     else
                     {
                        mp_MatrixValues[band2][band1] = mp_MatrixValues[band2][band1] /
                                                       (stdDev[band2] * stdDev[band1]);
                     }
                  }

                  if(mpProgress != NULL)
                  {
                     mpProgress->updateProgress("Computing Correlation Coefficients from Covariances...",
                                                      100 * band2 / m_NumBands, NORMAL);
                  }
               }

               if(mpProgress != NULL)
               {
                  mpProgress->updateProgress("Finished computing Correlation Coefficients", 100, NORMAL);
               }

               if(!bLoadFromFile)
               {
                  m_WriteMatrixToFile(strFilename, const_cast<const double**>(mp_MatrixValues),
                     m_NumBands, "Correlation Coefficient");
               }
            }
         }

         break;
      }
   }

   return success;
}

bool PCA::m_WriteMatrixToFile(QString filename, const double **pData, int numBands, const string &caption)
{
   FileResource pFile(filename.toStdString().c_str(), "wt");
   if(pFile.get() == NULL)
   {
      mMessage = "Unable to save " + caption + " matrix to disk as " + filename.toStdString();
      if(mpProgress != NULL) mpProgress->updateProgress(mMessage, 100, ERRORS);
      mpStep->addMessage(mMessage, "app", "A0478959-21AF-4e64-B9DA-C17D7363F1BB", true);
   }
   else
   {
      fprintf(pFile, "%d\n", numBands);
      for(int row = 0; row < numBands; row++)
      {
         for(int col = 0; col < numBands; col++)
         {
            fprintf(pFile, "%.15e ", pData[row][col]);
         }
         fprintf(pFile, "\n");
      }

      mMessage = caption + " matrix saved to disk as " + filename.toStdString();
      if(mpProgress != NULL) mpProgress->updateProgress(mMessage, 100, NORMAL);
   }

   return true;
}

bool PCA::m_ReadMatrixFromFile(QString filename, double **pData, int numBands, const string &caption)
{
   FileResource pFile(filename.toStdString().c_str(), "rt");
   if (pFile.get() == NULL)
   {
      mMessage = "Unable to read " + caption + " matrix from file " + filename.toStdString();
      if(mpProgress != NULL) mpProgress->updateProgress(mMessage, 0, ERRORS);
      mpStep->finalize(Message::Failure, mMessage);
      return false;
   }
   mMessage = "Reading "  + caption + " matrix from file " + filename.toStdString();
   mpProgress->updateProgress(mMessage, 0, NORMAL);

   int lnumBands = 0;
   int numFieldsRead = fscanf(pFile, "%d\n", &lnumBands);
   if(numFieldsRead != 1)
   {
      mMessage = "Unable to read matrix file\n" + filename.toStdString();
      if(mpProgress != NULL) mpProgress->updateProgress(mMessage, 0, ERRORS);
      mpStep->finalize(Message::Failure, mMessage);
      return false;
   }
   if(lnumBands != numBands)
   {
      mMessage = "Mismatch between number of bands in cube and in matrix file.";
      if(mpProgress != NULL) mpProgress->updateProgress(mMessage, 0, ERRORS);
      mpStep->finalize(Message::Failure, mMessage);
      return false;
   }
   for(int row = 0; row < numBands; row++)
   {
      for(int col = 0; col < numBands; col++)
      {
         numFieldsRead = fscanf(pFile, "%lg ", &(pData[row][col]));
         if(numFieldsRead != 1)
         {
            mMessage = "Error reading " + caption + " matrix from disk.";
            if(mpProgress != NULL) mpProgress->updateProgress(mMessage, 0, ERRORS);
            mpStep->finalize(Message::Failure, mMessage);
            return false;
         }
      }
      if(mpProgress != NULL) mpProgress->updateProgress(mMessage, 100 * row / numBands, NORMAL);
   }
   mMessage = caption + " matrix successfully read from disk";
   if(mpProgress != NULL) mpProgress->updateProgress(mMessage, 100, NORMAL);

   return true;
}

bool PCA::m_ComputeCovarianceMatrix(QString aoiName, int rowSkip, int colSkip)
{
   const RasterDataDescriptor* pDescriptor = dynamic_cast<const RasterDataDescriptor*>(mpRaster->getDataDescriptor());
   if(pDescriptor == NULL)
   {
      return false;
   }

   int NumCols = pDescriptor->getColumnCount();
   int NumRows = pDescriptor->getRowCount();

   void *pData = NULL;

   if(aoiName.isEmpty())
   {
      if((rowSkip < 1) || (colSkip < 1))
      {
         return false;
      }

      switchOnEncoding(pDescriptor->getDataType(), ComputeFactoredCov,
                        pData, mpRaster, mp_MatrixValues, mpProgress,
                        &mbAbort, rowSkip, colSkip);
   }
   else  // compute over AOI
   {
      MaskInput input;
      input.pMatrix = mp_MatrixValues;
      input.pRaster = mpRaster;
      input.pProgress = mpProgress;
      input.pAbortFlag = &mbAbort;

      AoiElement *pAoi = getAoiElement(aoiName.toStdString());
      if (pAoi == NULL)
      {
         mMessage = "Invalid AOI specified";
         if(mpProgress != NULL) mpProgress->updateProgress(mMessage, 0, ERRORS);
         mpStep->finalize(Message::Failure, mMessage);
         return false;
      }
      input.pMask = pAoi->getSelectedPoints();

      // check if AOI has any points selected
      if(input.pMask->getCount() < 1)
      {
         mMessage = "Can't compute Covariance - no pixels were selected in " + aoiName.toStdString();
         if(mpProgress != NULL) mpProgress->updateProgress(mMessage, 0, ERRORS);
         mpStep->finalize(Message::Failure, mMessage);
         if(mbInteractive)
         {
            QString message;
            message.sprintf("No pixels are currently selected in %s\nPCA is aborting.", aoiName);
            QMessageBox::critical(NULL, "PCA", message);
         }
         return false;
      }
      switchOnEncoding(pDescriptor->getDataType(), ComputeMaskedCov, pData, &input);
   }

   if(mbAbort)
   {
      if(mpProgress != NULL) mpProgress->updateProgress("Processing Covariance matrix aborted", 100, NORMAL);
      mpStep->finalize(Message::Abort);
      return false;
   }

   return true;
}

bool PCA::m_ReadInPCAtransform(QString filename)
{
   FileResource pFile(filename.toStdString().c_str(), "rt");
   if(pFile.get() == NULL)
   {
      mMessage = "Unable to read PCA transform from file " + filename.toStdString();
      if(mpProgress != NULL) mpProgress->updateProgress(mMessage, 0, ERRORS);
      mpStep->finalize(Message::Failure, mMessage);
      return false;
   }

   mMessage = "Reading PCA transform from file " + filename.toStdString();
   if(mpProgress != NULL) mpProgress->updateProgress(mMessage, 0, NORMAL);

   unsigned int lnumBands = 0, lnumComponents = 0;
   int numFieldsRead = fscanf(pFile, "%d\n", &lnumBands);
   if(numFieldsRead != 1)
   {
      mMessage = "Error reading number of bands from PCA transform file:\n" + filename.toStdString();
      if(mpProgress != NULL) mpProgress->updateProgress(mMessage, 0, ERRORS);
      mpStep->finalize(Message::Failure, mMessage);
      return false;
   }

   numFieldsRead = fscanf(pFile, "%d\n", &lnumComponents);
   if(numFieldsRead != 1)
   {
      mMessage = "Error reading number of components from PCA transform file:\n" + filename.toStdString();
      if(mpProgress != NULL) mpProgress->updateProgress(mMessage, 0, ERRORS);
      mpStep->finalize(Message::Failure, mMessage);
      return false;
   }

   if(lnumBands != m_NumBands)
   {
      mMessage = "Mismatch between number of bands in cube and in PCA transform file.";
      if(mpProgress != NULL) mpProgress->updateProgress(mMessage, 0, ERRORS);
      mpStep->finalize(Message::Failure, mMessage);
      return false;
   }
   bool success = true;
   if(lnumComponents < m_NumComponentsToUse)
   {
      if(mbInteractive)
      {
         QString message;
         message.sprintf("This file only contains definitions for %d components, not %d",
                                    lnumComponents, m_NumComponentsToUse);
         success = !QMessageBox::warning(NULL, "PCA", message, "Continue", "Cancel");
      }
   }

   if(success)
   {
      double junk = 0.0;
      for(unsigned int row = 0; row < m_NumBands; row++)
      {
         for(unsigned int col = 0; col < lnumComponents; col++)
         {
            if(col < m_NumComponentsToUse)
            {
               numFieldsRead = fscanf(pFile, "%lg ", &(mp_MatrixValues[row][col]));
            }
            else
            {
               numFieldsRead = fscanf(pFile, "%lg ", &(junk));
            }

            if(numFieldsRead != 1)
            {
               success = false;
               break;
            }
         }
         if(!success)
         {
            break;
         }
         mpProgress->updateProgress(mMessage, 100 * row / m_NumBands, NORMAL);
      }
      if(success)
      {
         mMessage = "PCA transform successfully read from disk";
         if(mpProgress != NULL) mpProgress->updateProgress(mMessage, 100, NORMAL);
      }
      else
      {
         mMessage = "Error reading PCA transform from disk.";
         if(mpProgress != NULL) mpProgress->updateProgress(mMessage, 0, ERRORS);
         mpStep->finalize(Message::Failure, mMessage);
      }
   }

   if(filename.contains("pcasmm"))
   {
      m_CalcMethod = SECONDMOMENT;
   }
   else if(filename.contains("pcacvm"))
   {
      m_CalcMethod = COVARIANCE;
   }
   else if(filename.contains("pcaccm"))
   {
      m_CalcMethod = CORRCOEF;
   }

   return success;
}

bool PCA::m_WriteOutPCAtransform(QString filename)
{
   FileResource pFile(filename.toStdString().c_str(), "wt");
   if(pFile.get() == NULL)
   {
      mMessage = "Unable to save PCA transform to disk as " + filename.toStdString();
      if(mpProgress != NULL) mpProgress->updateProgress(mMessage, 100, ERRORS);
      mpStep->finalize(Message::Failure, mMessage);
      return false;
   }

   fprintf(pFile, "%d\n", m_NumBands);
   fprintf(pFile, "%d\n", m_NumComponentsToUse);
   for(unsigned int row = 0; row < m_NumBands; row++)
   {
      for(unsigned int col = 0; col < m_NumComponentsToUse; col++)
      {
         fprintf(pFile, "%.15e ", mp_MatrixValues[row][col]);
      }
      fprintf(pFile, "\n");
   }

   mMessage = "PCA transform saved to disk as " + filename.toStdString();
   if(mpProgress != NULL) mpProgress->updateProgress(mMessage, 100, NORMAL);

   return true;
}

AoiElement* PCA::getAoiElement(const std::string& aoiName)
{
   AoiElement *pAoi = dynamic_cast<AoiElement*>(mpModel->getElement(aoiName,
      TypeConverter::toString<AoiElement>(), mpRaster));
   if(pAoi == NULL)
   {
      pAoi = dynamic_cast<AoiElement*>(mpModel->getElement(aoiName,
         TypeConverter::toString<AoiElement>(), NULL));
   }

   return pAoi;
}