////////////////////////////////////////////////////////////////////////
// \file    CVNEventDump_module.cc
// \brief   Analyzer module for creating CVN PixelMap objects
// \author  Alexander Radovic - a.radovic@gmail.com
////////////////////////////////////////////////////////////////////////

// C/C++ includes
#include <iostream>
#include <sstream>

// ROOT includes
#include "TTree.h"
#include "TH2F.h"

// Framework includes
#include "art/Framework/Core/EDAnalyzer.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Handle.h"
#include "art/Framework/Services/Optional/TFileDirectory.h"
#include "art/Framework/Services/Optional/TFileService.h"
#include "art/Framework/Services/Registry/ServiceHandle.h"
#include "fhiclcpp/ParameterSet.h"
#include "messagefacility/MessageLogger/MessageLogger.h"
#include "art/Framework/Core/ModuleMacros.h"
//#include "art/Framework/Core/FindManyP.h"


// NOvASoft includes
#include "lardataobj/RecoBase/Hit.h"
#include "lardata/DetectorInfoServices/DetectorPropertiesService.h"
#include "lardataobj/RawData/ExternalTrigger.h"


#include "lardata/Utilities/AssociationUtil.h"
#include "nusimdata/SimulationBase/MCNeutrino.h"
#include "nusimdata/SimulationBase/MCParticle.h"
#include "nusimdata/SimulationBase/MCTruth.h"

#include "larsim/MCCheater/BackTracker.h"

#include "dune/CVN/func/AssignLabels.h"
#include "dune/CVN/func/TrainingData.h"
#include "dune/CVN/func/InteractionType.h"
#include "dune/CVN/func/PixelMap.h"




namespace cvn {
  class CVNEventDump : public art::EDAnalyzer {
  public:
    explicit CVNEventDump(fhicl::ParameterSet const& pset);
    ~CVNEventDump();

    void analyze(const art::Event& evt);
    void reconfigure(const fhicl::ParameterSet& pset);
    void beginJob();
    void endJob();



  private:

    std::string fPixelMapInput;
    std::string fGenieGenModuleLabel;
    bool        fWriteMapTH2;

    TrainingData* fTrain;
    TTree*        fTrainTree;

    //art::ServiceHandle<cheat::BackTracker> fBT;

    /// Function to extract TH2 from PixelMap and write to TFile
    void WriteMapTH2(const art::Event& evt, int slice, const PixelMap& pm);

  };



  //.......................................................................
  CVNEventDump::CVNEventDump(fhicl::ParameterSet const& pset)
    : EDAnalyzer(pset)
  {
    this->reconfigure(pset);
  }

  //......................................................................
  CVNEventDump::~CVNEventDump()
  {  }

  //......................................................................
  void CVNEventDump::reconfigure(const fhicl::ParameterSet& pset)
  {
    fPixelMapInput  = pset.get<std::string>("PixelMapInput");
    fGenieGenModuleLabel  = pset.get<std::string>("GenieGenModuleLabel");
    fWriteMapTH2    = pset.get<bool>       ("WriteMapTH2");

  }

  //......................................................................
  void CVNEventDump::beginJob()
  {


    art::ServiceHandle<art::TFileService> tfs;

    fTrainTree = tfs->make<TTree>("CVNTrainTree", "Training records");
    fTrainTree->Branch("train", "cvn::TrainingData", &fTrain);


  }

  //......................................................................
  void CVNEventDump::endJob()
  {

  }

  //......................................................................
  void CVNEventDump::analyze(const art::Event& evt)
  {


    //    art::ServiceHandle<cheat::BackTracker>& bt = fBT;




    // get the slices

    art::Handle< std::vector< cvn::PixelMap > > pixelmapListHandle;
    std::vector< art::Ptr< cvn::PixelMap > > pixelmaplist;
    if (evt.getByLabel(fPixelMapInput, fPixelMapInput, pixelmapListHandle))
      art::fill_ptr_vector(pixelmaplist, pixelmapListHandle);

    //std::cout<<fPixelMapInput<<std::endl;
    //std::cout<<evt.getByLabel(fPixelMapInput, fPixelMapInput, pixelmapListHandle)<<std::endl;
    unsigned short nhits=  pixelmaplist.size();

    //std::cout<<"pixel maps: "<<nhits<<std::endl;

    InteractionType interaction = kOther;



    //simb::MCTruth
    //simb::MCTruth truth;

    //if(fBT->HaveTruthInfo()){




    // * monte carlo
    art::Handle< std::vector<simb::MCTruth> > mctruthListHandle;
    std::vector<art::Ptr<simb::MCTruth> > mclist;
    if(evt.getByLabel(fGenieGenModuleLabel,mctruthListHandle))
      art::fill_ptr_vector(mclist, mctruthListHandle);

    //unsigned short nmclist=  mclist.size();

    //std::cout<<"mctruth: "<<nmclist<<std::endl;

    art::Ptr<simb::MCTruth> truth = mclist[0];
    simb::MCNeutrino truthN=truth->GetNeutrino();
    //truth = mclist[0];

    interaction = GetInteractionType(truthN);
    float nuEnergy = 0;
    float lepEnergy = 0;
    //if(truth.NeutrinoSet()){
    nuEnergy = truthN.Nu().E();
    lepEnergy = truthN.Lepton().E();
    //}

    if(nhits>0){
      TrainingData train(interaction, nuEnergy, lepEnergy, *pixelmaplist[0]);

      if (fWriteMapTH2) WriteMapTH2(evt, 0, train.fPMap);

      fTrain = &train;
      fTrainTree->Fill();
    }



}

  //----------------------------------------------------------------------



  void CVNEventDump::WriteMapTH2(const art::Event& evt, int slice, const PixelMap& pm)
  {
      std::stringstream name;
      name << "PixelMap_r" << evt.run() << "_s" << evt.subRun()<< "_e" << evt.event() << "_sl" << slice;
      std::stringstream nameL;
      nameL << "PixelTruthMap_r" << evt.run() << "_s" << evt.subRun()<< "_e" << evt.event() << "_sl" << slice;
      std::stringstream nameX;
      nameX << "PixelMap_X_r" << evt.run() << "_s" << evt.subRun()<< "_e" << evt.event() << "_sl" << slice;
      std::stringstream nameY;
      nameY << "PixelMap_Y_r" << evt.run() << "_s" << evt.subRun()<< "_e" << evt.event() << "_sl" << slice;
      std::stringstream nameZ;
      nameZ << "PixelMap_Z_r" << evt.run() << "_s" << evt.subRun()<< "_e" << evt.event() << "_sl" << slice;
      TH2F* hist  = pm.ToTH2();
      TH2F* histL = pm.ToLabTH2();
      TH2F* histX = pm.SingleViewToTH2(0);
      TH2F* histY = pm.SingleViewToTH2(1);
      TH2F* histZ = pm.SingleViewToTH2(2);
      hist->SetName(name.str().c_str());
      histL->SetName(nameL.str().c_str());
      histX->SetName(nameX.str().c_str());
      histY->SetName(nameY.str().c_str());
      histZ->SetName(nameZ.str().c_str());

      art::ServiceHandle<art::TFileService> tfs;

      TH2F* histWrite = tfs->make<TH2F>(*hist);
      histWrite->Write();
      TH2F* histWriteL = tfs->make<TH2F>(*histL);
      histWriteL->GetZaxis()->SetRangeUser(0,10);
      histWriteL->Write();
      TH2F* histWriteX = tfs->make<TH2F>(*histX);
      histWriteX->Write();
      TH2F* histWriteY = tfs->make<TH2F>(*histY);
      histWriteY->Write();
      TH2F* histWriteZ = tfs->make<TH2F>(*histZ);
      histWriteZ->Write();

      delete hist;
      delete histWrite;
      delete histL;
      delete histWriteL;
      delete histX;
      delete histWriteX;
      delete histY;
      delete histWriteY;
      delete histZ;
      delete histWriteZ;

  }

DEFINE_ART_MODULE(cvn::CVNEventDump)
} // end namespace cvn
////////////////////////////////////////////////////////////////////////







