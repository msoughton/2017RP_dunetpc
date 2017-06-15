////////////////////////////////////////////////////////////////////////
// Class:       RecoEff
// Module Type: analyzer
// File:        RecoEff_module.cc
// Author:      Dorota Stefan
//
////////////////////////////////////////////////////////////////////////

#include "art/Framework/Core/EDAnalyzer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Handle.h"
#include "art/Framework/Principal/Run.h"
#include "art/Framework/Principal/SubRun.h"
#include "art/Framework/Services/Registry/ServiceHandle.h"
#include "art/Framework/Services/Optional/TFileService.h"
#include "canvas/Utilities/InputTag.h"
#include "fhiclcpp/ParameterSet.h"
#include "messagefacility/MessageLogger/MessageLogger.h"

#include "canvas/Persistency/Common/FindManyP.h"

#include "lardataobj/RecoBase/Hit.h"
#include "lardataobj/RecoBase/Track.h"
#include "larsim/MCCheater/BackTracker.h"
#include "nusimdata/SimulationBase/MCParticle.h"

#include "TH1.h"
#include "TTree.h"

namespace proto
{
	class RecoEff;
}

class proto::RecoEff : public art::EDAnalyzer {
public:
  explicit RecoEff(fhicl::ParameterSet const & p);

  // Plugins should not be copied or assigned.
  RecoEff(RecoEff const &) = delete;
  RecoEff(RecoEff &&) = delete;
  RecoEff & operator = (RecoEff const &) = delete;
  RecoEff & operator = (RecoEff &&) = delete;

  // Required functions.
  void analyze(art::Event const & e) override;
  void beginJob() override;
  void reconfigure(fhicl::ParameterSet const& p) override;

private:

  // Declare member data here.
  void ResetVars();
  
  TH1D* fHist1;
  TH1D* fHist2;
  
  TTree *fTree;
  TTree *fTreeTracks;
  
  int fRun;
  int fEvent;
  int fNRecoTracks;
  int fNMCTracks;
  int fNRecoHits;
  int fNMCHits;
  double fEnGen;
  double fEkGen;
  double fT0;
  double fNominator;
  double fDenominator;
  
  std::string fSimulationLabel;
  std::string fHitModuleLabel;
	std::string fTrackModuleLabel;
  
  std::unordered_map< int, const simb::MCParticle* > fParticleMap;
  std::unordered_map< const simb::MCParticle*, std::vector<recob::Hit> > fHitMap;
  std::unordered_map< const simb::MCParticle*, std::vector<recob::Hit> > fHitMapFiltered;
  std::unordered_map< const simb::MCParticle*, int> fMatchMap;

};

proto::RecoEff::RecoEff(fhicl::ParameterSet const & p)
  :
  EDAnalyzer(p)  // ,
 // More initializers here.
{
	reconfigure(p);
}

void proto::RecoEff::beginJob()
{
	art::ServiceHandle<art::TFileService> tfs;
	
	fHist1 = tfs->make<TH1D>("hist1","all hits", 100., 0., 3500.);
	fHist2 = tfs->make<TH1D>("hist2","hits", 100., 0., 3500.);
	
	fTree = tfs->make<TTree>("efficiency", "reco efficiency tree");
	fTree->Branch("fRun", &fRun, "fRun/I");
	fTree->Branch("fEvent", &fEvent, "fEvent/I");
	fTree->Branch("fEnGen", &fEnGen, "fEnGen/D");
	fTree->Branch("fEkGen", &fEkGen, "fEkGen/D");
	fTree->Branch("fT0", &fT0, "fT0/D");
	fTree->Branch("fNRecoTracks", &fNRecoTracks, "fNRecoTracks/I");
	fTree->Branch("fNMCTracks", &fNMCTracks, "fNMCTracks/I");
	fTree->Branch("fNominator", &fNominator, "fNominator/D");
	fTree->Branch("fDenominator", &fDenominator, "fDenominator/D");
	
	fTreeTracks = tfs->make<TTree>("trk efficiency", "reco");
	fTreeTracks->Branch("fNMCHits", &fNMCHits, "fNMCHits/I");
	fTreeTracks->Branch("fNRecoHits", &fNRecoHits, "fNRecoHits/I");

}

void proto::RecoEff::reconfigure(fhicl::ParameterSet const & p)
{
	fSimulationLabel = p.get< std::string >("SimulationLabel");
	fHitModuleLabel = p.get< std::string >("HitModuleLabel");
	fTrackModuleLabel = p.get< std::string >("TrackModuleLabel");
}

void proto::RecoEff::analyze(art::Event const & e)
{
  // Implementation of required member function here.
  ResetVars();
  
  fRun = e.run();
  fEvent = e.id().event();
  
  // MC particle list
  auto particleHandle = e.getValidHandle< std::vector<simb::MCParticle> >(fSimulationLabel);
  for (auto const& p : *particleHandle)
  {
  	fParticleMap[p.TrackId()] = &p;
  	if (p.Process() == "primary")
  	{
  		fEnGen = p.P();
  		fEkGen = (std::sqrt(p.P()*p.P() + p.Mass()*p.Mass()) - p.Mass()) * 1000; // MeV
  		fT0 = p.T();
  	}
  }
  
  art::ServiceHandle<cheat::BackTracker> bt;
  
  // hits 
  std::map<int, std::vector<recob::Hit> > trackID_vHitsMap;
  std::map<int, double> trackID_sumen;
  
  const auto& hitListHandle = *e.getValidHandle< std::vector<recob::Hit> >(fHitModuleLabel);
	for (auto const &h : hitListHandle)
	{
		std::map<int, double> particleID_E;
		for (auto const & id : bt->HitToTrackID(h)) // loop over std::vector< sim::TrackIDE >
		{
			if ((id.trackID > 0) && (abs((bt->TrackIDToParticle(id.trackID))->PdgCode()) != 11)) // we will count only MC tracks
			{
				particleID_E[id.trackID] += id.energy;
			}
		}
		
		int besthit_id = 0; 
		double maxhit_e = 0.0;
	
		for (auto const & contrib : particleID_E)
		{
			if (contrib.second > maxhit_e) // find hit ID corresponding to max energy
			{
				maxhit_e = contrib.second;
				besthit_id = contrib.first;
			}
		}
		
		if (maxhit_e > 0)
		{
			fHitMap[bt->TrackIDToParticle(besthit_id)].push_back(h);
			trackID_vHitsMap[besthit_id].push_back(h);
			trackID_sumen[besthit_id] += maxhit_e;
		}
	}
	
	//for (auto const &p : fHitMap)
	std::map<int, std::vector< recob::Hit > > trackID_vHitsMapF;
	for (auto const &p : trackID_vHitsMap)
	{
		size_t collhits = 0;
		size_t indhits = 0;
		for (auto const &h : p.second)
		{
			if (h.View() == 2)
			{
				collhits++;
			}
			else
			{
				indhits++;
			}
		}
		if ((collhits > 5) && (indhits > 5))
		{
			fNMCTracks++;
			fDenominator++;
			trackID_vHitsMapF[p.first] = p.second;
			fNMCHits = p.second.size();
			fHist1->Fill(p.second.size());
		}
		
	}
	
	
	std::cout << " event: " << fEvent << " denominator: " << fDenominator << std::endl;
	for (auto const &p : trackID_vHitsMapF)
	{
		std::cout << " : id " << p.first << " size " << p.second.size() << " en: " << trackID_sumen[p.first] << std::endl;
	}
	
	// tracks
	const auto trkHandle = e.getValidHandle< std::vector<recob::Track> >(fTrackModuleLabel);
	art::FindManyP< recob::Hit > hitsFromTracks(trkHandle, e, fTrackModuleLabel);
	
	fNRecoTracks = trkHandle->size();
	std::cout << " fNRecoTracks " << fNRecoTracks << std::endl;
	for (size_t t = 0; t < trkHandle->size(); ++t) // loop over tracks
	{
		std::map<int, double> trkID_E;
		for (size_t h = 0; h < hitsFromTracks.at(t).size(); ++h) // loop over hits
		{
			for (auto const & id : bt->HitToTrackID(hitsFromTracks.at(t)[h])) // loop over std::vector< sim::TrackIDE >, for a hit
			{
				// check if mcparticle is in fhitmapfiltered
				//if (fHitMapFiltered.find(bt->TrackIDToParticle(id.trackID)) != fHitMapFiltered.end())
				if (trackID_vHitsMapF.find(id.trackID) != trackID_vHitsMapF.end())
				{ 
					trkID_E[id.trackID] += id.energy;
				}
			}
		}
		
		double max_e = 0.0; double tot_e = 0.0;
		int best_id = 0;
		for (std::map<int, double>::iterator it = trkID_E.begin(); it != trkID_E.end(); ++it)
		{
			tot_e += it->second; // sum total energy in these hits
			if (it->second > max_e) //&& (it->second > particleID_E[it->first]*0.5)) // find track ID corresponding to max energy and check if max_e has more than 0.5 energy w.r.t mc truth.
			{
				max_e = it->second;
				best_id = it->first;
			}
		}
		
		if ((max_e > 0) && (max_e > 0.5*trackID_sumen[best_id]) && (tot_e > 0.0)) // found something reasonable and it is a track
		{
			fMatchMap[bt->TrackIDToParticle(best_id)] = 1;
			//fNMCHits = fHitMapFiltered[bt->TrackIDToParticle(best_id)].size();
			fNMCHits = trackID_vHitsMapF[best_id].size();
			fNRecoHits = hitsFromTracks.at(t).size();
			//fHist2->Fill(fHitMapFiltered[bt->TrackIDToParticle(best_id)].size());
			fHist2->Fill(trackID_vHitsMapF[best_id].size());
			std::cout << " max_e " << max_e << " trackID_sumen[best_id]: " << trackID_sumen[best_id] <<  std::endl;
			fNominator++;
		}
			
		fTreeTracks->Fill();
	}
	
	std::cout << " nominator: " << fNominator << std::endl;
	fTree->Fill();
}

void proto::RecoEff::ResetVars()
{
	fRun = 0;
	fEvent = 0;
	fNRecoTracks = 0;
	fNMCTracks = 0;
	fNMCHits = 0;
	fNRecoHits = 0;
	fEnGen = 0.0;
	fEkGen = 0.0;
	fT0 = 0.0;
	fNominator = 0.0;
	fDenominator = 0.0;
	fParticleMap.clear();
	fHitMap.clear();
	fHitMapFiltered.clear();
	fMatchMap.clear();
}

DEFINE_ART_MODULE(proto::RecoEff)
