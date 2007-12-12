/*
 * The information in this file is
 * Copyright(c) 2007 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "TypeConverter.h"

#include "AppConfig.h"
#include "ComplexData.h"
#include "Location.h"
#include "LocationType.h"
#include "SpatialDataView.h"
#include "TypesFile.h"
#include "Point.h"

#include <string>
#include <vector>

#define TYPECONVERTERTOSTRING_IMP(type__) \
   template<> \
   const char* TypeConverter::toString< type__ >() \
   { \
      return #type__; \
   } \
   template<> \
   const char* TypeConverter::toString<std::vector< type__ *> >() \
   { \
      return "vector<"#type__">"; \
   } \
   template<> \
   const char* TypeConverter::toString<std::vector< type__ > >() \
   { \
      return toString<std::vector<type__*> >(); \
   }

#define TYPECONVERTERTOSTRING_IMP_FORWARD(type__) \
   class type__; \
   TYPECONVERTERTOSTRING_IMP(type__) \
   template<> \
   const char* TypeConverter::toString< type__ * >() \
   { \
      return #type__; \
   }

using std::string;

TYPECONVERTERTOSTRING_IMP(bool)
TYPECONVERTERTOSTRING_IMP(char)
TYPECONVERTERTOSTRING_IMP(unsigned char)
TYPECONVERTERTOSTRING_IMP(short)
TYPECONVERTERTOSTRING_IMP(unsigned short)
TYPECONVERTERTOSTRING_IMP(int)
TYPECONVERTERTOSTRING_IMP(unsigned int)
TYPECONVERTERTOSTRING_IMP(long)
TYPECONVERTERTOSTRING_IMP(unsigned long)
#ifdef WIN_API
TYPECONVERTERTOSTRING_IMP(int64_t)
TYPECONVERTERTOSTRING_IMP(uint64_t)
#endif
TYPECONVERTERTOSTRING_IMP(float)
TYPECONVERTERTOSTRING_IMP(double)
TYPECONVERTERTOSTRING_IMP(void)
TYPECONVERTERTOSTRING_IMP(string)

TYPECONVERTERTOSTRING_IMP(AnimationCycle)
TYPECONVERTERTOSTRING_IMP(AnimationState)
TYPECONVERTERTOSTRING_IMP(ArcRegion)
TYPECONVERTERTOSTRING_IMP(ComplexComponent)
TYPECONVERTERTOSTRING_IMP(DataOrigin)
TYPECONVERTERTOSTRING_IMP(DisplayMode)
TYPECONVERTERTOSTRING_IMP(DmsFormatType)
TYPECONVERTERTOSTRING_IMP(EncodingType)
TYPECONVERTERTOSTRING_IMP(EndianType)
TYPECONVERTERTOSTRING_IMP(FillStyle)
TYPECONVERTERTOSTRING_IMP(InsetZoomMode)
TYPECONVERTERTOSTRING_IMP(InterleaveFormatType)
TYPECONVERTERTOSTRING_IMP(GcpSymbol)
TYPECONVERTERTOSTRING_IMP(GeocoordType)
TYPECONVERTERTOSTRING_IMP(GraphicObjectType)
TYPECONVERTERTOSTRING_IMP(LatLonStyle)
TYPECONVERTERTOSTRING_IMP(LayerType)
TYPECONVERTERTOSTRING_IMP(LineStyle)
TYPECONVERTERTOSTRING_IMP(PanLimitType)
TYPECONVERTERTOSTRING_IMP(PassArea)
TYPECONVERTERTOSTRING_IMP(PlotObjectType)
TYPECONVERTERTOSTRING_IMP(Point::PointSymbolType)
TYPECONVERTERTOSTRING_IMP(PositionType)
TYPECONVERTERTOSTRING_IMP(ProcessingLocation)
TYPECONVERTERTOSTRING_IMP(SessionSaveType)
TYPECONVERTERTOSTRING_IMP(RasterChannelType)
TYPECONVERTERTOSTRING_IMP(ReleaseType)
TYPECONVERTERTOSTRING_IMP(StretchType)
TYPECONVERTERTOSTRING_IMP(SymbolType)
TYPECONVERTERTOSTRING_IMP(RegionUnits)
TYPECONVERTERTOSTRING_IMP(UnitType)
TYPECONVERTERTOSTRING_IMP(WindowSizeType)
TYPECONVERTERTOSTRING_IMP(WindowType)

TYPECONVERTERTOSTRING_IMP_FORWARD(ApplicationWindow)
TYPECONVERTERTOSTRING_IMP_FORWARD(Animation)
TYPECONVERTERTOSTRING_IMP_FORWARD(AnimationController)
TYPECONVERTERTOSTRING_IMP_FORWARD(AnnotationElement)
TYPECONVERTERTOSTRING_IMP_FORWARD(AnnotationLayer)
TYPECONVERTERTOSTRING_IMP_FORWARD(Any)
TYPECONVERTERTOSTRING_IMP_FORWARD(AoiElement)
TYPECONVERTERTOSTRING_IMP_FORWARD(AoiLayer)
TYPECONVERTERTOSTRING_IMP_FORWARD(BitMask)
TYPECONVERTERTOSTRING_IMP_FORWARD(Classification)
TYPECONVERTERTOSTRING_IMP_FORWARD(ClassificationLayer)
TYPECONVERTERTOSTRING_IMP_FORWARD(ColorType)
TYPECONVERTERTOSTRING_IMP_FORWARD(DataDescriptor)
TYPECONVERTERTOSTRING_IMP_FORWARD(DataElement)
TYPECONVERTERTOSTRING_IMP_FORWARD(DataElementGroup)
TYPECONVERTERTOSTRING_IMP_FORWARD(DataRequest)
TYPECONVERTERTOSTRING_IMP_FORWARD(DataVariantAnyData)
TYPECONVERTERTOSTRING_IMP_FORWARD(DateTime)
TYPECONVERTERTOSTRING_IMP_FORWARD(DimensionDescriptor)
TYPECONVERTERTOSTRING_IMP_FORWARD(DynamicObject)
TYPECONVERTERTOSTRING_IMP_FORWARD(ExecutableAgent)
TYPECONVERTERTOSTRING_IMP_FORWARD(ExecutableAgentCommon)
TYPECONVERTERTOSTRING_IMP_FORWARD(ExportAgent)
TYPECONVERTERTOSTRING_IMP_FORWARD(ExportAgentCommon)
TYPECONVERTERTOSTRING_IMP_FORWARD(FileDescriptor)
TYPECONVERTERTOSTRING_IMP_FORWARD(FileFinder)
TYPECONVERTERTOSTRING_IMP_FORWARD(Filename)
TYPECONVERTERTOSTRING_IMP_FORWARD(FloatComplex)
TYPECONVERTERTOSTRING_IMP_FORWARD(Font)
TYPECONVERTERTOSTRING_IMP_FORWARD(GcpLayer)
TYPECONVERTERTOSTRING_IMP_FORWARD(GcpList)
TYPECONVERTERTOSTRING_IMP_FORWARD(GraphicElement)
TYPECONVERTERTOSTRING_IMP_FORWARD(GraphicLayer)
TYPECONVERTERTOSTRING_IMP_FORWARD(ImportAgent)
TYPECONVERTERTOSTRING_IMP_FORWARD(ImportAgentCommon)
TYPECONVERTERTOSTRING_IMP_FORWARD(IntegerComplex)
TYPECONVERTERTOSTRING_IMP_FORWARD(LatLonLayer)
TYPECONVERTERTOSTRING_IMP_FORWARD(Layer)
TYPECONVERTERTOSTRING_IMP(LocationType)
TYPECONVERTERTOSTRING_IMP(Opticks::PixelLocation)
TYPECONVERTERTOSTRING_IMP_FORWARD(PlotWidget)
TYPECONVERTERTOSTRING_IMP_FORWARD(PlotWindow)
TYPECONVERTERTOSTRING_IMP_FORWARD(ProductView)
TYPECONVERTERTOSTRING_IMP_FORWARD(ProductWindow)
TYPECONVERTERTOSTRING_IMP_FORWARD(Progress)
TYPECONVERTERTOSTRING_IMP_FORWARD(PseudocolorLayer)
TYPECONVERTERTOSTRING_IMP_FORWARD(RasterDataDescriptor)
TYPECONVERTERTOSTRING_IMP_FORWARD(RasterElement)
TYPECONVERTERTOSTRING_IMP_FORWARD(RasterFileDescriptor)
TYPECONVERTERTOSTRING_IMP_FORWARD(RasterLayer)
TYPECONVERTERTOSTRING_IMP_FORWARD(SessionItem)
TYPECONVERTERTOSTRING_IMP_FORWARD(SettableSessionItem)
TYPECONVERTERTOSTRING_IMP_FORWARD(Signature)
TYPECONVERTERTOSTRING_IMP_FORWARD(SignatureLibrary)
TYPECONVERTERTOSTRING_IMP_FORWARD(SignatureSet)
TYPECONVERTERTOSTRING_IMP_FORWARD(SpatialDataView)
TYPECONVERTERTOSTRING_IMP_FORWARD(SpatialDataWindow)
TYPECONVERTERTOSTRING_IMP_FORWARD(ThresholdLayer)
TYPECONVERTERTOSTRING_IMP_FORWARD(TiePointLayer)
TYPECONVERTERTOSTRING_IMP_FORWARD(TiePointList)
TYPECONVERTERTOSTRING_IMP_FORWARD(Units)
TYPECONVERTERTOSTRING_IMP_FORWARD(View)
TYPECONVERTERTOSTRING_IMP_FORWARD(Window)
TYPECONVERTERTOSTRING_IMP_FORWARD(WizardObject)
TYPECONVERTERTOSTRING_IMP_FORWARD(WorkspaceWindow)