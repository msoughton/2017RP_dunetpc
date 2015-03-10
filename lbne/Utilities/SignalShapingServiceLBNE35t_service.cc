////////////////////////////////////////////////////////////////////////
/// \file   SignalShapingServiceLBNE35t_service.cc
/// \author H. Greenlee 
////////////////////////////////////////////////////////////////////////

#include "lbne/Utilities/SignalShapingServiceLBNE35t.h"
#include "art/Framework/Services/Registry/ServiceHandle.h"
#include "messagefacility/MessageLogger/MessageLogger.h"
#include "cetlib/exception.h"
#include "Geometry/Geometry.h"
#include "Geometry/TPCGeo.h"
#include "Geometry/PlaneGeo.h"
#include "Utilities/DetectorProperties.h"
#include "Utilities/LArProperties.h"
#include "Utilities/LArFFT.h"
#include "TFile.h"

//----------------------------------------------------------------------
// Constructor.
util::SignalShapingServiceLBNE35t::SignalShapingServiceLBNE35t(const fhicl::ParameterSet& pset,
								    art::ActivityRegistry& /* reg */) 
  : fInit(false)
{
  reconfigure(pset);
}


//----------------------------------------------------------------------
// Destructor.
util::SignalShapingServiceLBNE35t::~SignalShapingServiceLBNE35t()
{}


//----------------------------------------------------------------------
// Reconfigure method.
void util::SignalShapingServiceLBNE35t::reconfigure(const fhicl::ParameterSet& pset)
{
  // Reset initialization flag.

  fInit = false;

  // Reset kernels.

  fColSignalShaping.Reset();
  fIndSignalShaping.Reset();

  // Fetch fcl parameters.

  fNFieldBins = pset.get<int>("FieldBins");
  fCol3DCorrection = pset.get<double>("Col3DCorrection");
  fInd3DCorrection = pset.get<double>("Ind3DCorrection");
  fColFieldRespAmp = pset.get<double>("ColFieldRespAmp");
  fIndFieldRespAmp = pset.get<double>("IndFieldRespAmp");
  
  fDeconNorm = pset.get<double>("DeconNorm");
  fADCPerPCAtLowestASICGain = pset.get<double>("ADCPerPCAtLowestASICGain");
  fASICGainInMVPerFC = pset.get<std::vector<double> >("ASICGainInMVPerFC");
  fShapeTimeConst = pset.get<std::vector<double> >("ShapeTimeConst");
  fNoiseFactVec =  pset.get<std::vector<DoubleVec> >("NoiseFactVec");

  fInputFieldRespSamplingPeriod = pset.get<double>("InputFieldRespSamplingPeriod");
  
  fFieldResponseTOffset = pset.get<std::vector<double> >("FieldResponseTOffset");

  fUseFunctionFieldShape= pset.get<bool>("UseFunctionFieldShape");
  fUseHistogramFieldShape = pset.get<bool>("UseHistogramFieldShape");

  fGetFilterFromHisto= pset.get<bool>("GetFilterFromHisto");
  
  // Construct parameterized collection filter function.
 if(!fGetFilterFromHisto)
 {
  mf::LogInfo("SignalShapingServiceLBNE35t") << "Getting Filter from .fcl file" ;
  std::string colFilt = pset.get<std::string>("ColFilter");
  std::vector<double> colFiltParams =
  pset.get<std::vector<double> >("ColFilterParams");
  fColFilterFunc = new TF1("colFilter", colFilt.c_str());
  for(unsigned int i=0; i<colFiltParams.size(); ++i)
    fColFilterFunc->SetParameter(i, colFiltParams[i]);

  // Construct parameterized induction filter function.

  std::string indFilt = pset.get<std::string>("IndFilter");
  std::vector<double> indFiltParams =
  pset.get<std::vector<double> >("IndFilterParams");
  fIndFilterFunc = new TF1("indFilter", indFilt.c_str());
  for(unsigned int i=0; i<indFiltParams.size(); ++i)
    fIndFilterFunc->SetParameter(i, indFiltParams[i]);
 }
 else
 {  
   std::string histoname = pset.get<std::string>("FilterHistoName");
   mf::LogInfo("SignalShapingServiceLBNE35t") << " using filter from .root file " ;
   int fNPlanes=3;
   
  // constructor decides if initialized value is a path or an environment variable
  std::string fname;
  cet::search_path sp("FW_SEARCH_PATH");
  sp.find_file(pset.get<std::string>("FilterFunctionFname"), fname);
    
  TFile * in=new TFile(fname.c_str(),"READ");
   for(int i=0;i<fNPlanes;i++){
     TH1D * temp=(TH1D *)in->Get(Form(histoname.c_str(),i));
     fFilterHist[i]=new TH1D(Form(histoname.c_str(),i),Form(histoname.c_str(),i),temp->GetNbinsX(),0,temp->GetNbinsX());
     temp->Copy(*fFilterHist[i]); 
    }
   
   in->Close();
   
 }

 /////////////////////////////////////
 if(fUseFunctionFieldShape)
 {
  std::string colField = pset.get<std::string>("ColFieldShape");
  std::vector<double> colFieldParams =
    pset.get<std::vector<double> >("ColFieldParams");
  fColFieldFunc = new TF1("colField", colField.c_str());
  for(unsigned int i=0; i<colFieldParams.size(); ++i)
    fColFieldFunc->SetParameter(i, colFieldParams[i]);

  // Construct parameterized induction filter function.

  std::string indField = pset.get<std::string>("IndFieldShape");
  std::vector<double> indFieldParams =
    pset.get<std::vector<double> >("IndFieldParams");
  fIndFieldFunc = new TF1("indField", indField.c_str());
  for(unsigned int i=0; i<indFieldParams.size(); ++i)
    fIndFieldFunc->SetParameter(i, indFieldParams[i]);
   // Warning, last parameter needs to be multiplied by the FFTSize, in current version of the code,
  } else if ( fUseHistogramFieldShape ) {
    mf::LogInfo("SignalShapingServiceLBNE35t") << " using the field response provided from a .root file " ;
    int fNPlanes = 3;
    
    // constructor decides if initialized value is a path or an environment variable
    std::string fname;   
    cet::search_path sp("FW_SEARCH_PATH");
    sp.find_file( pset.get<std::string>("FieldResponseFname"), fname );
    std::string histoname = pset.get<std::string>("FieldResponseHistoName");

    std::unique_ptr<TFile> fin(new TFile(fname.c_str(), "READ"));
    if ( !fin->IsOpen() ) throw art::Exception( art::errors::NotFound ) << "Could not find the field response file " << fname << "!" << std::endl;

    std::string iPlane[3] = { "U", "V", "Y" };

    for ( int i = 0; i < fNPlanes; i++ ) {
      TString iHistoName = Form( "%s_%s", histoname.c_str(), iPlane[i].c_str());
      TH1F *temp = (TH1F*) fin->Get( iHistoName );  
      if ( !temp ) throw art::Exception( art::errors::NotFound ) << "Could not find the field response histogram " << iHistoName << std::endl;
      if ( temp->GetNbinsX() > fNFieldBins ) throw art::Exception( art::errors::InvalidNumber ) << "FieldBins should always be larger than or equal to the number of the bins in the input histogram!" << std::endl;
      
      fFieldResponseHist[i] = new TH1F( iHistoName, iHistoName, temp->GetNbinsX(), temp->GetBinLowEdge(1), temp->GetBinLowEdge( temp->GetNbinsX() + 1) );
      temp->Copy(*fFieldResponseHist[i]);
    }
    
    fin->Close();
  }
}


//----------------------------------------------------------------------
// Accessor for single-plane signal shaper.
const util::SignalShaping&
util::SignalShapingServiceLBNE35t::SignalShaping(unsigned int channel) const
{
  if(!fInit)
    init();

  // Figure out plane type.

  art::ServiceHandle<geo::Geometry> geom;
  geo::SigType_t sigtype = geom->SignalType(channel);

  // Return appropriate shaper.

  if(sigtype == geo::kInduction)
    return fIndSignalShaping;
  else if(sigtype == geo::kCollection)
    return fColSignalShaping;
  else
    throw cet::exception("SignalShapingServiceLBNE35t")<< "can't determine"
                                                          << " SignalType\n";
							  
return fColSignalShaping;
}

//-----Give Gain Settings to SimWire-----//jyoti
double util::SignalShapingServiceLBNE35t::GetASICGain(unsigned int const channel) const
{
  art::ServiceHandle<geo::Geometry> geom;
    
  geo::SigType_t sigtype = geom->SignalType(channel);

  
  double gain = 0;
  if(sigtype == geo::kInduction)
    gain = fASICGainInMVPerFC.at(0);
  else if(sigtype == geo::kCollection)
    gain = fASICGainInMVPerFC.at(1);
  else
    throw cet::exception("SignalShapingServiceLBNE35t")<< "can't determine"
						       << " SignalType\n";
  return gain;
}


//-----Give Shaping time to SimWire-----//jyoti
double util::SignalShapingServiceLBNE35t::GetShapingTime(unsigned int const channel) const
{
  art::ServiceHandle<geo::Geometry> geom;
  geo::SigType_t sigtype = geom->SignalType(channel);

  double shaping_time = 0;

  if(sigtype == geo::kInduction)
    shaping_time = fShapeTimeConst.at(0);
  else if(sigtype == geo::kCollection)
    shaping_time = fShapeTimeConst.at(1);
  else
    throw cet::exception("SignalShapingServiceLBNE35t")<< "can't determine"
						       << " SignalType\n";
  return shaping_time;
}

double util::SignalShapingServiceLBNE35t::GetRawNoise(unsigned int const channel) const
{
  unsigned int plane;
  art::ServiceHandle<geo::Geometry> geom;
  geo::SigType_t sigtype = geom->SignalType(channel);
  if(sigtype == geo::kInduction)
    plane = 0;
  else if(sigtype == geo::kCollection)
    plane = 1;
  else
    throw cet::exception("SignalShapingServiceLBNE35t")<< "can't determine"
                                                          << " SignalType\n";

  double shapingtime = fShapeTimeConst.at(plane);
  double gain = fASICGainInMVPerFC.at(plane);
  int temp;
  if (shapingtime == 0.5){
    temp = 0;
  }else if (shapingtime == 1.0){
    temp = 1;
  }else if (shapingtime == 2.0){
    temp = 2;
  }else{
    temp = 3;
  }
  double rawNoise;

  auto tempNoise = fNoiseFactVec.at(plane);
  rawNoise = tempNoise.at(temp);

  rawNoise *= gain/4.7;
  return rawNoise;
}

double util::SignalShapingServiceLBNE35t::GetDeconNoise(unsigned int const channel) const
{
  unsigned int plane;
  art::ServiceHandle<geo::Geometry> geom;
  
  geo::SigType_t sigtype = geom->SignalType(channel);
  if(sigtype == geo::kInduction)
    plane = 0;
  else if(sigtype == geo::kCollection)
    plane = 1;
  else
    throw cet::exception("SignalShapingServiceLBNE35t")<< "can't determine"
                                                          << " SignalType\n";

  double shapingtime = fShapeTimeConst.at(plane);
  int temp;
  if (shapingtime == 0.5){
    temp = 0;
  }else if (shapingtime == 1.0){
    temp = 1;
  }else if (shapingtime == 2.0){
    temp = 2;
  }else{
    temp = 3;
  }
  auto tempNoise = fNoiseFactVec.at(plane);
  double deconNoise = tempNoise.at(temp);

  deconNoise = deconNoise /4096.*2000./4.7 *6.241*1000/fDeconNorm;
  return deconNoise;
}

//----------------------------------------------------------------------
// Initialization method.
// Here we do initialization that can't be done in the constructor.
// All public methods should ensure that this method is called as necessary.
void util::SignalShapingServiceLBNE35t::init()
{
  if(!fInit) {
    fInit = true;

    // Do microboone-specific configuration of SignalShaping by providing
    // microboone response and filter functions.

    // Calculate field and electronics response functions.

    SetFieldResponse();
    SetElectResponse(fShapeTimeConst.at(1),fASICGainInMVPerFC.at(1));

    // Configure convolution kernels.

    fColSignalShaping.AddResponseFunction(fColFieldResponse);
    fColSignalShaping.AddResponseFunction(fElectResponse);
    fColSignalShaping.save_response();
    fColSignalShaping.set_normflag(false);
    //fColSignalShaping.SetPeakResponseTime(0.);

    SetElectResponse(fShapeTimeConst.at(0),fASICGainInMVPerFC.at(0));

    fIndSignalShaping.AddResponseFunction(fIndFieldResponse);
    fIndSignalShaping.AddResponseFunction(fElectResponse);
    fIndSignalShaping.save_response();
    fIndSignalShaping.set_normflag(false);
    //fIndSignalShaping.SetPeakResponseTime(0.);

    SetResponseSampling();

    // Calculate filter functions.

    SetFilters();

    // Configure deconvolution kernels.

    fColSignalShaping.AddFilterFunction(fColFilter);
    fColSignalShaping.CalculateDeconvKernel();

    fIndSignalShaping.AddFilterFunction(fIndFilter);
    fIndSignalShaping.CalculateDeconvKernel();
  }
}


//----------------------------------------------------------------------
// Calculate microboone field response.
void util::SignalShapingServiceLBNE35t::SetFieldResponse()
{
  // Get services.

  art::ServiceHandle<geo::Geometry> geo;
  art::ServiceHandle<util::DetectorProperties> detprop;
  art::ServiceHandle<util::LArProperties> larp;

  // Get plane pitch.
 
  double xyz1[3] = {0.};
  double xyz2[3] = {0.};
  double xyzl[3] = {0.};
  // should always have at least 2 planes
  geo->Plane(0).LocalToWorld(xyzl, xyz1);
  geo->Plane(1).LocalToWorld(xyzl, xyz2);

  // this assumes all planes are equidistant from each other,
  // probably not a bad assumption
  double pitch = xyz2[0] - xyz1[0]; ///in cm

  fColFieldResponse.resize(fNFieldBins, 0.);
  fIndFieldResponse.resize(fNFieldBins, 0.);

  // set the response for the collection plane first
  // the first entry is 0

  double driftvelocity=larp->DriftVelocity()/1000.;  
  int nbinc = TMath::Nint(fCol3DCorrection*(fabs(pitch))/(driftvelocity*detprop->SamplingRate())); ///number of bins //KP
  double integral = 0.;  
  ////////////////////////////////////////////////////
   if(fUseFunctionFieldShape)
  {
  art::ServiceHandle<util::LArFFT> fft;
  int signalSize = fft->FFTSize();
  std::vector<double> ramp(signalSize);
   // TComplex kernBin;
   // int size = signalSize/2;
   // int bin=0;
    //std::vector<TComplex> freqSig(size+1);
  std::vector<double> bipolar(signalSize);
    
    
  fColFieldResponse.resize(signalSize, 0.);
  fIndFieldResponse.resize(signalSize, 0.);
   
  // Hardcoding. Bad. Temporary hopefully.
  fIndFieldFunc->SetParameter(4,fIndFieldFunc->GetParameter(4)*signalSize);
  
  
  //double integral = 0.;
    for(int i = 0; i < signalSize; i++) {
          ramp[i]=fColFieldFunc->Eval(i);
          fColFieldResponse[i]=ramp[i];
          integral += fColFieldResponse[i];
     // rampc->Fill(i,ramp[i]);
      bipolar[i]=fIndFieldFunc->Eval(i);
      fIndFieldResponse[i]=bipolar[i];
     // bipol->Fill(i,bipolar[i]);
    }
     
   for(int i = 0; i < signalSize; ++i){
      fColFieldResponse[i] *= fColFieldRespAmp/integral;
     }
      
    //this might be not necessary if the function definition is not defined in the middle of the signal range  
    fft->ShiftData(fIndFieldResponse,signalSize/2.0);
  } else if ( fUseHistogramFieldShape ) {
    
    // Ticks in nanosecond
    // Calculate the normalization of the collection plane
    for ( int ibin = 1; ibin <= fFieldResponseHist[2]->GetNbinsX(); ibin++ )
      integral += fFieldResponseHist[2]->GetBinContent( ibin );   

    // Induction plane
    for ( int ibin = 1; ibin <= fFieldResponseHist[1]->GetNbinsX(); ibin++ )
      fIndFieldResponse[ibin-1] = fIndFieldRespAmp*fFieldResponseHist[1]->GetBinContent( ibin )/integral;

    for ( int ibin = 1; ibin <= fFieldResponseHist[2]->GetNbinsX(); ibin++ )
      fColFieldResponse[ibin-1] = fColFieldRespAmp*fFieldResponseHist[2]->GetBinContent( ibin )/integral;
  } else
  {
    //////////////////////////////////////////////////
    mf::LogInfo("SignalShapingServiceLBNE35t") << " using the old field shape " ;
    double integral = 0.;
    for(int i = 1; i < nbinc; ++i){
      fColFieldResponse[i] = fColFieldResponse[i-1] + 1.0;
      integral += fColFieldResponse[i];
    }
    
    for(int i = 0; i < nbinc; ++i){
      fColFieldResponse[i] *= fColFieldRespAmp/integral;
    }
    
    // now the induction plane
    
    int nbini = TMath::Nint(fInd3DCorrection*(fabs(pitch))/(driftvelocity*detprop->SamplingRate()));//KP
    for(int i = 0; i < nbini; ++i){
      fIndFieldResponse[i] = fIndFieldRespAmp/(1.*nbini);
      fIndFieldResponse[nbini+i] = -fIndFieldRespAmp/(1.*nbini);
    }

  }
  
  return;
}

//----------------------------------------------------------------------
// Calculate microboone field response.
void util::SignalShapingServiceLBNE35t::SetElectResponse(double shapingtime, double gain)
{
  // Get services.

  art::ServiceHandle<geo::Geometry> geo;
  art::ServiceHandle<util::DetectorProperties> detprop;
  art::ServiceHandle<util::LArFFT> fft;

  LOG_DEBUG("SignalShapingLBNE35t") << "Setting LBNE35t electronics response function...";

  int nticks = fft->FFTSize();
  fElectResponse.resize(nticks, 0.);
  std::vector<double> time(nticks,0.);

  //Gain and shaping time variables from fcl file:    
  double Ao = 1.0;
  double To = shapingtime;  //peaking time
    
  // this is actually sampling time, in ns
  // mf::LogInfo("SignalShapingLBNE35t") << "Check sampling intervals: " 
  //                                  << fSampleRate << " ns" 
  //                                  << "Check number of samples: " << fNTicks;

  // The following sets the microboone electronics response function in 
  // time-space. Function comes from BNL SPICE simulation of LBNE35t 
  // electronics. SPICE gives the electronics transfer function in 
  // frequency-space. The inverse laplace transform of that function 
  // (in time-space) was calculated in Mathematica and is what is being 
  // used below. Parameters Ao and To are cumulative gain/timing parameters 
  // from the full (ASIC->Intermediate amp->Receiver->ADC) electronics chain. 
  // They have been adjusted to make the SPICE simulation to match the 
  // actual electronics response. Default params are Ao=1.4, To=0.5us. 
  double max = 0;
  
  for(size_t i = 0; i < fElectResponse.size(); ++i){

    //convert time to microseconds, to match fElectResponse[i] definition
    time[i] = (1.*i)*fInputFieldRespSamplingPeriod *1e-3; 
    fElectResponse[i] = 
      4.31054*exp(-2.94809*time[i]/To)*Ao - 2.6202*exp(-2.82833*time[i]/To)*cos(1.19361*time[i]/To)*Ao
      -2.6202*exp(-2.82833*time[i]/To)*cos(1.19361*time[i]/To)*cos(2.38722*time[i]/To)*Ao
      +0.464924*exp(-2.40318*time[i]/To)*cos(2.5928*time[i]/To)*Ao
      +0.464924*exp(-2.40318*time[i]/To)*cos(2.5928*time[i]/To)*cos(5.18561*time[i]/To)*Ao
      +0.762456*exp(-2.82833*time[i]/To)*sin(1.19361*time[i]/To)*Ao
      -0.762456*exp(-2.82833*time[i]/To)*cos(2.38722*time[i]/To)*sin(1.19361*time[i]/To)*Ao
      +0.762456*exp(-2.82833*time[i]/To)*cos(1.19361*time[i]/To)*sin(2.38722*time[i]/To)*Ao
      -2.6202*exp(-2.82833*time[i]/To)*sin(1.19361*time[i]/To)*sin(2.38722*time[i]/To)*Ao 
      -0.327684*exp(-2.40318*time[i]/To)*sin(2.5928*time[i]/To)*Ao + 
      +0.327684*exp(-2.40318*time[i]/To)*cos(5.18561*time[i]/To)*sin(2.5928*time[i]/To)*Ao
      -0.327684*exp(-2.40318*time[i]/To)*cos(2.5928*time[i]/To)*sin(5.18561*time[i]/To)*Ao
      +0.464924*exp(-2.40318*time[i]/To)*sin(2.5928*time[i]/To)*sin(5.18561*time[i]/To)*Ao;

    if(fElectResponse[i] > max) max = fElectResponse[i];
    
  }// end loop over time buckets
    

  LOG_DEBUG("SignalShapingLBNE35t") << " Done.";

 //normalize fElectResponse[i], before the convolution   
  
   for(auto& element : fElectResponse){
    element /= max;
    element *= fADCPerPCAtLowestASICGain * 1.60217657e-7;
    element *= gain / 4.7;
   }
  
  return;

}


//----------------------------------------------------------------------
// Calculate microboone filter functions.
void util::SignalShapingServiceLBNE35t::SetFilters()
{
  // Get services.

  art::ServiceHandle<util::DetectorProperties> detprop;
  art::ServiceHandle<util::LArFFT> fft;

  double ts = detprop->SamplingRate();
  int n = fft->FFTSize() / 2;

  // Calculate collection filter.

  fColFilter.resize(n+1);
  fIndFilter.resize(n+1);
  
  if(!fGetFilterFromHisto)
  {
  fColFilterFunc->SetRange(0, double(n));

  for(int i=0; i<=n; ++i) {
    double freq = 500. * i / (ts * n);      // Cycles / microsecond.
    double f = fColFilterFunc->Eval(freq);
    fColFilter[i] = TComplex(f, 0.);
  }
  

  // Calculate induction filter.

 
  fIndFilterFunc->SetRange(0, double(n));

  for(int i=0; i<=n; ++i) {
    double freq = 500. * i / (ts * n);      // Cycles / microsecond.
    double f = fIndFilterFunc->Eval(freq);
    fIndFilter[i] = TComplex(f, 0.);
    }
  
  }
  else
  {
    
    for(int i=0; i<=n; ++i) {
      double f = fFilterHist[2]->GetBinContent(i);  // hardcoded plane numbers. Bad. To change later.
      fColFilter[i] = TComplex(f, 0.);
      double g = fFilterHist[0]->GetBinContent(i);
      fIndFilter[i] = TComplex(g, 0.);
      
    }
  }
  
  fIndSignalShaping.AddFilterFunction(fIndFilter);
  fColSignalShaping.AddFilterFunction(fColFilter);
  
}


//----------------------------------------------------------------------
// Sample microboone response (the convoluted field and electronic
// response), will probably add the filter later
void util::SignalShapingServiceLBNE35t::SetResponseSampling()
{
  // Get services
  art::ServiceHandle<geo::Geometry> geo;
  art::ServiceHandle<util::LArProperties> larp;
  art::ServiceHandle<util::DetectorProperties> detprop;
  art::ServiceHandle<util::LArFFT> fft;

  // Operation permitted only if output of rebinning has a larger bin size
  if( fInputFieldRespSamplingPeriod > detprop->SamplingRate() )
    throw cet::exception(__FUNCTION__) << "\033[93m"
				       << "Invalid operation: cannot rebin to a more finely binned vector!"
				       << "\033[00m" << std::endl;

  int nticks = fft->FFTSize();
  std::vector<double> SamplingTime( nticks, 0. );
  for ( int itime = 0; itime < nticks; itime++ ) {
    SamplingTime[itime] = (1.*itime) * detprop->SamplingRate();
    /// VELOCITY-OUT ... comment out kDVel usage here
    //SamplingTime[itime] = (1.*itime) * detprop->SamplingRate() / kDVel;
  }

  // Sampling
  for ( int iplane = 0; iplane < 2; iplane++ ) {
    const std::vector<double>* pResp;
    switch ( iplane ) {
    case 0: pResp = &(fIndSignalShaping.Response_save()); break;
    default: pResp = &(fColSignalShaping.Response_save()); break;
    }

    std::vector<double> SamplingResp(nticks , 0. );
    
    
    int nticks_input = pResp->size();
    std::vector<double> InputTime(nticks_input, 0. );
    for ( int itime = 0; itime < nticks_input; itime++ ) {
      InputTime[itime] = (1.*itime) * fInputFieldRespSamplingPeriod;
    }
    
   
    /*
      Much more sophisticated approach using a linear (trapezoidal) interpolation 
      Current default!
    */
    int SamplingCount = 0;    
    for ( int itime = 0; itime < nticks; itime++ ) {
      int low = -1, up = -1;
      for ( int jtime = 0; jtime < nticks_input; jtime++ ) {
        if ( InputTime[jtime] == SamplingTime[itime] ) {
          SamplingResp[itime] = (*pResp)[jtime];
	  /// VELOCITY-OUT ... comment out kDVel usage here
          //SamplingResp[itime] = kDVel * (*pResp)[jtime];
          SamplingCount++;
          break;
        } else if ( InputTime[jtime] > SamplingTime[itime] ) {
          low = jtime - 1;
          up = jtime;
          SamplingResp[itime] = (*pResp)[low] + ( SamplingTime[itime] - InputTime[low] ) * ( (*pResp)[up] - (*pResp)[low] ) / ( InputTime[up] - InputTime[low] );
	  /// VELOCITY-OUT ... comment out kDVel usage here
          //SamplingResp[itime] *= kDVel;
          SamplingCount++;
          break;
        } else {
          SamplingResp[itime] = 0.;
        }
      } // for ( int jtime = 0; jtime < nticks; jtime++ )
    } // for ( int itime = 0; itime < nticks; itime++ )
    SamplingResp.resize( SamplingCount, 0.);    

  

  
    switch ( iplane ) {
      case 0: fIndSignalShaping.AddResponseFunction( SamplingResp, true ); break;
    default: fColSignalShaping.AddResponseFunction( SamplingResp, true ); break;
    }

   

  } // for ( int iplane = 0; iplane < fNPlanes; iplane++ )

  return;
}



int util::SignalShapingServiceLBNE35t::FieldResponseTOffset(unsigned int const channel) const
{
  art::ServiceHandle<geo::Geometry> geom;
  geo::SigType_t sigtype = geom->SignalType(channel);
  double time_offset = 0;
  if(sigtype == geo::kInduction)
    time_offset = fFieldResponseTOffset.at(0); 
  else if(sigtype == geo::kCollection)
    time_offset = fFieldResponseTOffset.at(1); 
  else
    throw cet::exception("SignalShapingServiceLBNE35t")<< "can't determine"
						       << " SignalType\n";

 
  auto tpc_clock = art::ServiceHandle<util::TimeService>()->TPCClock();
  return tpc_clock.Ticks(time_offset/1.e3);
  
}


namespace util {

  DEFINE_ART_SERVICE(SignalShapingServiceLBNE35t)

}
