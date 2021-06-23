/**
 * @file   MyEnergyAnalysis_module.cc
 * @brief  A file to read and analyze art::Event records from a DUNE FD MC file,
 * @author Wei Shi (wei.shi.1@stonybrook.edu)
 *
 * Adapted from https://cdcvs.fnal.gov/redmine/projects/larsoft/wiki/_AnalysisExample_
 */

// Include headers: starting from LArSoft and going up the software
// layers (nusimdata, art, etc.), ending with C++ is standard.

// LArSoft includes
#include "larcore/Geometry/Geometry.h"
#include "larcorealg/Geometry/GeometryCore.h"
#include "larcoreobj/SimpleTypesAndConstants/geo_types.h"
#include "lardata/DetectorInfoServices/DetectorClocksService.h"
#include "lardataobj/RecoBase/Cluster.h"
#include "lardataobj/RecoBase/Hit.h"
#include "lardataobj/Simulation/SimChannel.h"
#include "larsim/Simulation/LArG4Parameters.h"
#include "nusimdata/SimulationBase/MCParticle.h"
#include "nusimdata/SimulationBase/MCTruth.h"

// Framework includes
#include "art/Framework/Core/EDAnalyzer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Handle.h"
#include "art/Framework/Services/Registry/ServiceHandle.h"
#include "art_root_io/TFileService.h"
#include "canvas/Persistency/Common/FindManyP.h"
#include "canvas/Utilities/Exception.h"

// Utility libraries
#include "cetlib/pow.h" // cet::sum_of_squares()
#include "fhiclcpp/types/Atom.h"
#include "fhiclcpp/types/Table.h"
#include "messagefacility/MessageLogger/MessageLogger.h"

// ROOT includes
#include "TH1.h"
#include "TLorentzVector.h"
#include "TTree.h"
#include "TVector3.h"

// C++ includes
#include <cmath>
#include <map>

namespace {

  // This is a local namespace. Stuff declared here will not be
  // visible beyond this file. We will define functions at the end,
  // but we declare them here so that the module can freely use them.

  /// Utility function to get the diagonal of the detector
  double DetectorDiagonal(geo::GeometryCore const& geom);

  /// Comparison routine for using std::lower/upper_bound to search
  /// TDCIDE vectors.
  bool TDCIDETimeCompare(const sim::TDCIDE&, const sim::TDCIDE&);

} // local namespace

// An outside package call this module like lar::example::MyEnergyAnalysis

namespace lar {
  namespace example {

    // BEGIN MyEnergyAnalysis group
    // -----------------------------------------------
    // class definition
    /**
     * This class produces a ROOT tree that contains information
     * from the generated/simulated and reconstructed particles.
     *
     * Configuration parameters
     * =========================
     *
     * - *SimulationLabel* (string, default: "largeant"): tag of the input data
     *   product with the detector simulation information (typically an instance
     *   of the LArG4 module)
     *
     * - *HitLabel* (string, mandatory): tag of the input data product with
     *   reconstructed hits
     *
     * - *ClusterLabel* (string, mandatory): tag of the input data product with
     *   reconstructed clusters
     *
     * - *PDGcode* (integer, mandatory): particle type (PDG ID) to be selected;
     *   only primary particles of this type will be considered
     *
     * - *BinSize* (real, mandatory): dx [cm] used for dE/dx calculation
     *
     */
    class MyEnergyAnalysis : public art::EDAnalyzer {
    public:

      // This structure describes the configuration parameters of the module.
      // Any missing or unknown parameters will generate a configuration error.

      struct Config {

        // Save some typing:
        using Name = fhicl::Name;
        using Comment = fhicl::Comment;

        // One Atom for each parameter
        fhicl::Atom<art::InputTag> SimulationLabel{
          Name("SimulationLabel"),
          Comment("tag of the input data product with the detector simulation "
                  "information")};

        fhicl::Atom<art::InputTag> HitLabel{
          Name("HitLabel"),
          Comment("tag of the input data product with reconstructed hits")};

        fhicl::Atom<art::InputTag> ClusterLabel{
          Name("ClusterLabel"),
          Comment("tag of the input data product with reconstructed clusters")};

        fhicl::Atom<int> PDGcode{
          Name("PDGcode"),
          Comment("particle type (PDG ID) of the primary particle to be selected")};

        fhicl::Atom<double> BinSize{Name("BinSize"),
                                    Comment("dx [cm] used for the dE/dx calculation")};

      }; // Config

      using Parameters = art::EDAnalyzer::Table<Config>;

      /// Constructor: configures the module (see the Config structure above)
      explicit MyEnergyAnalysis(Parameters const& config);

      // This method is called once, at the start of the job. In this
      // example, it will define the histograms and n-tuples we'll
      // write.
      virtual void beginJob() override;

      // This method is called once, at the start of each run. It's a
      // good place to read databases or files that may have
      // run-dependent information.
      virtual void beginRun(const art::Run& run) override;

      // The analysis routine, called once per event.
      virtual void analyze(const art::Event& event) override;

    private:

      // The parameters we'll read from the .fcl file.
      art::InputTag fSimulationProducerLabel; // The name of the producer that tracked simulated particles through the detector
      art::InputTag fHitProducerLabel;        // The name of the producer that created hits
      art::InputTag fClusterProducerLabel;    // The name of the producer that created clusters
      int fSelectedPDG;                       // PDG code of particle we'll focus on
      double fBinSize;                        // For dE/dx work: the value of dx.

      // The n-tuple we'll create.
      TTree* fNtuple;

      // Event info
      int fEvent;  // number of the event being processed
      int fRun;    // number of the run being processed
      int fSubRun; // number of the sub-run being processed

      // Variables related to simulation
      int fSimPDG;                       // MCParticle PDG ID
      int fSimTrackID;                   // GEANT ID of the particle being processed
      double fSim_mu_start_vx;           // x position of the muon trajectory start
      double fSim_mu_start_vy;           // y .....................................
      double fSim_mu_start_vz;           // z .....................................
      double fSim_mu_end_vx;             // x position of the muon trajectory end
      double fSim_mu_end_vy;             // y ...................................
      double fSim_mu_end_vz;             // z ...................................
      double fSim_mu_start_px;           // x momentum of the muon trajectory start
      double fSim_mu_start_py;           // y .....................................
      double fSim_mu_start_pz;           // z .....................................
      double fSim_mu_end_px;             // x momentum of the muon trajectory end
      double fSim_mu_end_py;             // y ...................................
      double fSim_mu_end_pz;             // z ...................................
      double fSim_mu_start_4position[4]; // (x,y,z,t) of the muon trajectory start
      double fSim_mu_end_4position[4];   // ................................ end
      double fSim_mu_start_4mommenta[4]; // (Px,Py,Pz,E) of the muon trajectory start
      double fSim_mu_end_4mommenta[4];   // ................................... end
      double fSim_mu_track_length;       // muon track length
      std::vector<double> fSimdEdxBins;  // The vector that will be used to accumulate sim dE/dx values as a function of range

      // Variables related to reconstruction
      int fRecoPDG;                      // PDG ID of the particle being processed
      int fRecoTrackID;                  // GEANT ID of the particle being processed
      int fRecoNdEdxBins;                // Number of dE/dx bins in a given reco track.
      std::vector<double> fRecodEdxBins; // The vector that will be used to accumulate reco dE/dx values as a function of range.

      // Other variables that will be shared between different methods.
      geo::GeometryCore const* fGeometryService; // pointer to Geometry provider
      double fElectronsToGeV;                    // conversion factor for no. of ionization electrons to energy deposited in GeV
      int fTriggerOffset;                        // (units of ticks) time of expected neutrino event

    }; // class MyEnergyAnalysis

    // END MyEnergyAnalysis group
    // -------------------------------------------------

    //-----------------------------------------------------------------------
    // class implementation

    //-----------------------------------------------------------------------
    // Constructor

    MyEnergyAnalysis::MyEnergyAnalysis(Parameters const& config)
      : EDAnalyzer(config)
      , fSimulationProducerLabel(config().SimulationLabel())
      , fHitProducerLabel(config().HitLabel())
      , fClusterProducerLabel(config().ClusterLabel())
      , fSelectedPDG(config().PDGcode())
      , fBinSize(config().BinSize())
    {
      // Get a pointer to the geometry service provider.
      fGeometryService = lar::providerFrom<geo::Geometry>();

      auto const clock_data = art::ServiceHandle<detinfo::DetectorClocksService const>()->DataForJob();
      fTriggerOffset = trigger_offset(clock_data);

      // Tell beforehand all the data the module is going to read ("consumes") or
      // might read ("may_consume").
      consumes<std::vector<simb::MCParticle>>(fSimulationProducerLabel);
      consumes<std::vector<sim::SimChannel>>(fSimulationProducerLabel);
      consumes<art::Assns<simb::MCTruth, simb::MCParticle>>(fSimulationProducerLabel);
      consumes<std::vector<recob::Hit>>(fHitProducerLabel);
      consumes<std::vector<recob::Cluster>>(fClusterProducerLabel);
      consumes<art::Assns<recob::Cluster, recob::Hit>>(fHitProducerLabel);
    }

    //-----------------------------------------------------------------------
    void MyEnergyAnalysis::beginJob()
    {
      // Get the detector length
      const double detectorLength = DetectorDiagonal(*fGeometryService);
      std::cout << "Detector length=" << detectorLength << " cm" << std::endl;

      // Access art's TFileService, which will handle creating and writing
      // histograms and n-tuples for us.
      art::ServiceHandle<art::TFileService const> tfs;

      // Define n-tuples
      fNtuple = tfs->make<TTree>("MyTree", "MyTree");

      fNtuple->Branch("Event",                 &fEvent,                  "Event/I");
      fNtuple->Branch("SubRun",                &fSubRun,                 "SubRun/I");
      fNtuple->Branch("Run",                   &fRun,                    "Run/I");
      // Simulation branches Sim*
      // muon position
      fNtuple->Branch("Sim_mu_start_vx",       &fSim_mu_start_vx,        "Sim_mu_start_vx/D");
      fNtuple->Branch("Sim_mu_start_vy",       &fSim_mu_start_vy,        "Sim_mu_start_vy/D");
      fNtuple->Branch("Sim_mu_start_vz",       &fSim_mu_start_vz,        "Sim_mu_start_vz/D");
      fNtuple->Branch("Sim_mu_end_vx",         &fSim_mu_end_vx,          "Sim_mu_end_vx/D");
      fNtuple->Branch("Sim_mu_end_vy",         &fSim_mu_end_vy,          "Sim_mu_end_vy/D");
      fNtuple->Branch("Sim_mu_end_vz",         &fSim_mu_end_vz,          "Sim_mu_end_vz/D");
      // muon momentum
      fNtuple->Branch("Sim_mu_start_px",       &fSim_mu_start_px,        "Sim_mu_start_px/D");
      fNtuple->Branch("Sim_mu_start_py",       &fSim_mu_start_py,        "Sim_mu_start_py/D");
      fNtuple->Branch("Sim_mu_start_pz",       &fSim_mu_start_pz,        "Sim_mu_start_pz/D");
      fNtuple->Branch("Sim_mu_end_px",         &fSim_mu_end_px,          "Sim_mu_end_px/D");
      fNtuple->Branch("Sim_mu_end_py",         &fSim_mu_end_py,          "Sim_mu_end_py/D");
      fNtuple->Branch("Sim_mu_end_pz",         &fSim_mu_end_pz,          "Sim_mu_end_pz/D");
      // Write arrays giving the address of the array: simply the array name.
      fNtuple->Branch("Sim_mu_start_4position", fSim_mu_start_4position, "Sim_mu_start_4position[4]/D");
      fNtuple->Branch("Sim_mu_end_4position",   fSim_mu_end_4position,   "Sim_mu_end_4position[4]/D");
      fNtuple->Branch("Sim_mu_start_4mommenta", fSim_mu_start_4mommenta, "Sim_mu_start_4mommenta[4]/D");
      fNtuple->Branch("Sim_mu_end_4mommenta",   fSim_mu_end_4mommenta,   "Sim_mu_end_4mommenta[4]/D");
      fNtuple->Branch("Sim_mu_track_length",   &fSim_mu_track_length,    "Sim_mu_track_length/D");
      // ROOT branches can contain std::vector objects.
      fNtuple->Branch("dEdx",                  &fSimdEdxBins);

      // Reconstruction branches
      fNtuple->Branch("NdEdx",                 &fRecoNdEdxBins,          "NdEdx/I");
      fNtuple->Branch("dEdx",                  &fRecodEdxBins);
    }

    //-----------------------------------------------------------------------
    void MyEnergyAnalysis::beginRun(const art::Run& /*run*/)
    {
      // Conversion factor for no. of ionization electrons to energy deposited in GeV
      // The ultimate source of this conversion factor is
      // ${LARCOREOBJ_INC}/larcoreobj/SimpleTypesAndConstants/PhysicalConstants.h.
      art::ServiceHandle<sim::LArG4Parameters const> larParameters;
      fElectronsToGeV = 1. / larParameters->GeVToElectrons();
    }

    //-----------------------------------------------------------------------
    void MyEnergyAnalysis::analyze(const art::Event& event)
    {
      // Fetching basic event information.
      fEvent = event.id().event();
      fRun = event.run();
      fSubRun = event.subRun();

      // Read multiple objects associated with one event:
      // define a "handle" to point to a vector of the objects.
      art::Handle<std::vector<simb::MCParticle>> particleHandle;

      // Then fill the vector with all the objects
      if (!event.getByLabel(fSimulationProducerLabel, particleHandle)) {
        // If no MCParticles in an event, throw an exception to force this module to stop.
        throw cet::exception("MyEnergyAnalysis") << " No simb::MCParticle objects in this event - " << " Line " << __LINE__ << " in file " << __FILE__ << std::endl;
      }

      // Get all the simulated channels for the event. These channels
      // include the energy deposited for each simulated track.
      auto simChannelHandle = event.getValidHandle<std::vector<sim::SimChannel>>(fSimulationProducerLabel);

      // Create a map pf MCParticle to its track ID
      std::map<int, const simb::MCParticle*> particleMap;

      // Loop over the list of particles in the event
      for (auto const& particle : (*particleHandle)) {

        // For the methods you can call for MCParticle, see ${NUSIMDATA_INC}/nusimdata/SimulationBase/MCParticle.h.
        fSimTrackID = particle.TrackId();

        // Add the address of the MCParticle to the map, with the track ID as the key.
        particleMap[fSimTrackID] = &particle;

        // Only for primary particles in the event whose PDG codes match a value supplied in the .fcl file
        fSimPDG = particle.PdgCode();
        if (particle.Process() != "primary" || abs(fSimPDG) != fSelectedPDG) continue;

        // A particle has a trajectory, consisting of a set of 4-positions and 4-mommenta.
        const size_t numberTrajectoryPoints = particle.NumberTrajectoryPoints();

        // For trajectories, as for vectors and arrays, the first point is #0, not #1.
        const int last = numberTrajectoryPoints - 1;
        const TLorentzVector& positionStart = particle.Position(0);
        const TLorentzVector& positionEnd = particle.Position(last);
        const TLorentzVector& momentumStart = particle.Momentum(0);
        const TLorentzVector& momentumEnd = particle.Momentum(last);

        // Fill position and momentum components.
        fSim_mu_start_vx = particle.Vx(0);
        fSim_mu_start_vy = particle.Vy(0);
        fSim_mu_start_vz = particle.Vz(0);
        fSim_mu_end_vx = particle.Vx(last);
        fSim_mu_end_vy = particle.Vy(last);
        fSim_mu_end_vz = particle.Vz(last);
        fSim_mu_start_px = particle.Px(0);
        fSim_mu_start_py = particle.Py(0);
        fSim_mu_start_pz = particle.Pz(0);
        fSim_mu_end_px = particle.Px(last);
        fSim_mu_end_py = particle.Py(last);
        fSim_mu_end_pz = particle.Pz(last);

        // Fill arrays with the 4-values.
        positionStart.GetXYZT(fSim_mu_start_4position);
        positionEnd.GetXYZT(fSim_mu_end_4position);
        momentumStart.GetXYZT(fSim_mu_start_4mommenta);
        momentumEnd.GetXYZT(fSim_mu_end_4mommenta);

        // Calculate length using spherical cooridnate system: assume straight track? time negligible?
        const double trackLength = (positionEnd - positionStart).Rho();
        fSim_mu_track_length = trackLength;

        // std::cout << "track ID=" << fSimTrackID << " (PDG ID: " << fSimPDG << ") " << trackLength << " cm long, momentum " << momentumStart.P() << " GeV/c, has " << numberTrajectoryPoints << " trajectory points" << std::endl;

        // Initialize the vector of dE/dx bins to be empty.
        fSimdEdxBins.clear();

        // Loop over the SimChannel objects in the event to look at the energy deposited by particle's track.
        for (auto const& channel : (*simChannelHandle)) {

          // Get the numeric ID associated with this channel.
          // See methods at https://internal.dunescience.org/doxygen/SimChannel_8h_source.html
          auto const channelNumber = channel.Channel();

          // Note: There is more than one plane that reacts to charge in the TPC. We only want to include the
          // energy from the collection plane: geo::kCollection is defined in
          // ${LARCOREOBJ_INC}/larcoreobj/SimpleTypesAndConstants/geo_types.h
          if (fGeometryService->SignalType(channelNumber) != geo::kCollection) continue;

          // Each channel has a map inside it that connects a time slice to energy deposits in the detector.
          // The full type of this map is std::map<unsigned short, std::vector<sim::IDE>>; we'll use "auto" here
          auto const& timeSlices = channel.TDCIDEMap();

          for (auto const& timeSlice : timeSlices) {

            // For the timeSlices map, the 'first' is a time slice number; The 'second' is a vector of IDE objects.
            auto const& energyDeposits = timeSlice.second;

            // An "energy deposit" object stores how much charge/energy was deposited in a small volume, by which particle, and where.
            // The type of 'energyDeposit' will be sim::IDE, here use auto.
            for (auto const& energyDeposit : energyDeposits) {

              // Check if the track that deposited the energy matches the track of the particle.
              // See methods at https://nusoft.fnal.gov/larsoft/doxsvn/html.1.7.1/classsim_1_1IDE.html
              if (energyDeposit.trackID != fSimTrackID) continue;

              // Get the (x,y,z) of the energy deposit.
              TVector3 location(energyDeposit.x, energyDeposit.y, energyDeposit.z);

              // Distance from the start of the track.
              const double distance = (location - positionStart.Vect()).Mag();

              // Into which bin of the dE/dx array do we add the energy?
              const unsigned int bin = (unsigned int)(distance / fBinSize);

              // Is the dE/dx array long enough to include this bin?
              if (fSimdEdxBins.size() < bin + 1) {

                // Increase the array size to accomodate the new bin, padding it with zeros.
                fSimdEdxBins.resize(bin + 1, 0.);
              }

              // Add the energy deposit to that bin.
              fSimdEdxBins[bin] += energyDeposit.numElectrons * fElectronsToGeV;

            } // end For each energy deposit
          }   // end For each time slice
        }     // end For each SimChannel

        // Write TTree
        fNtuple->Fill();

      } // end loop over all particles in the event.

      // Reading the reco Hits. A reco Hit is a 2D object in a plane:
      // see ${LARDATAOBJ_INC}/lardataobj/RecoBase/Hit.h
      // We don't use art::ValidHandle here because there might be no hits in the input;
      // e.g., we ran the simulation but not the reconstruction, may as well skip this module.
      art::Handle<std::vector<recob::Hit>> hitHandle;
      if (!event.getByLabel(fHitProducerLabel, hitHandle)) return;

      // Create a map with track ID as the key to hold vectors of dE/dx information.
      std::map<int, std::vector<double>> dEdxMap;

      auto const clock_data = art::ServiceHandle<detinfo::DetectorClocksService const>()->DataFor(event);

      for (auto const& hit : (*hitHandle)) {

        // The channel associated with this hit.
        auto hitChannelNumber = hit.Channel();

        // We have a hit. For this example let's just focus on the hits in the collection plane.
        if (fGeometryService->SignalType(hitChannelNumber) != geo::kCollection) continue;

        // std::cout << "Hit in collection plane" << std::endl;

        // In reconstruction, the channel waveforms are truncated. So
        // we have to adjust the Hit TDC ticks to match those of the
        // SimChannels, which were created during simulation.
        typedef sim::SimChannel::StoredTDC_t TDC_t;
        TDC_t start_tdc = clock_data.TPCTick2TDC(hit.StartTick());
        TDC_t end_tdc = clock_data.TPCTick2TDC(hit.EndTick());
        TDC_t hitStart_tdc = clock_data.TPCTick2TDC(hit.PeakTime() - 3. * hit.SigmaPeakTime());
        TDC_t hitEnd_tdc = clock_data.TPCTick2TDC(hit.PeakTime() + 3. * hit.SigmaPeakTime());

        start_tdc = std::max(start_tdc, hitStart_tdc);
        end_tdc = std::min(end_tdc, hitEnd_tdc);

        // Search the SimChannels for matching channel number, then look
        // at the tracks inside the channel.
        for (auto const& channel : (*simChannelHandle)) {
          auto simChannelNumber = channel.Channel();
          if (simChannelNumber != hitChannelNumber) continue;

          // std::cout << "Matched channel no.: " << simChannelNumber << std::endl;

          // The time slices in this sim channel.
          auto const& timeSlices = channel.TDCIDEMap();

          // Find the range of time slices in the sim channel that correspond to the range of hit times.
          sim::TDCIDE startTime;
          sim::TDCIDE endTime;
          startTime.first = start_tdc;
          endTime.first = end_tdc;
          // Find a pointer to the first time slice with time >= start_tdc.
          auto const startPointer = std::lower_bound(timeSlices.begin(), timeSlices.end(), startTime, TDCIDETimeCompare);
          // Find the last time slice with time < end_tdc.
          auto const endPointer = std::upper_bound(startPointer, timeSlices.end(), endTime, TDCIDETimeCompare);

          // Did we find anything? If not, skip.
          if (startPointer == timeSlices.end() || startPointer == endPointer) continue;
          // std::cout << "Time slice start = " << (*startPointer).first << ", end = " << (*endPointer).first  << std::endl;

          // Loop over the time slices we found that match the hit times.
          for (auto slicePointer = startPointer; slicePointer != endPointer; slicePointer++) {
            auto const timeSlice = *slicePointer;

            // Loop over the energy deposits from sim channel.
            auto const& energyDeposits = timeSlice.second;
            for (auto const& energyDeposit : energyDeposits) {

              // Search the map for the track ID associated with this energy deposit.
              auto search = particleMap.find(energyDeposit.trackID);

              // Did we find this track ID in the particle map?
              if (search == particleMap.end()) continue;

              // "search" points to a pair in the map: <track ID, MCParticle*>
              int trackID = (*search).first;
              const simb::MCParticle& particle = *((*search).second);

              // Does it match the user input?
              if (particle.Process() != "primary" || abs(particle.PdgCode()) != fSelectedPDG) continue;

              // Determine the dE/dx of this particle.
              // Question: This is still sim energy deposits? Just that integrated the reco hit time slices?
              const TLorentzVector& positionStart = particle.Position(0);
              TVector3 location(energyDeposit.x, energyDeposit.y, energyDeposit.z);
              double distance = (location - positionStart.Vect()).Mag();
              unsigned int bin = int(distance / fBinSize);
              double energy = energyDeposit.numElectrons * fElectronsToGeV;

              auto& track_dEdX = dEdxMap[trackID];
              if (track_dEdX.size() < bin + 1) {
                track_dEdX.resize(bin + 1, 0);
              }

              track_dEdX[bin] += energy;

            } // loop over energy deposits
          }   // loop over time slices
        }     // for each SimChannel
      }       // for each Hit

      // We have a map of dE/dx vectors. Write each one of them to the
      // reconstruction n-tuple.
      for (const auto& dEdxEntry : dEdxMap) {
        // Here, the map entries are <first,second>=<track ID, dE/dx vector>
        fRecoTrackID = dEdxEntry.first;

        // This is an example of how we'd pick out the PDG code if
        // there are multiple particle types or tracks in a single
        // event allowed in the n-tuple.
        fRecoPDG = particleMap[fRecoTrackID]->PdgCode();

        // Get the number of bins for this track.
        const std::vector<double>& dEdx = dEdxEntry.second;
        fRecoNdEdxBins = dEdx.size();

        // Copy this track's dE/dx information.
        fRecodEdxBins = dEdx;

        // At this point, we've filled in all the reconstruction
        // n-tuple's variables. Write it.
        fNtuple->Fill();
      }

      // In loops above, what links the information in simb::MCParticle and sim::SimChannel is the
      // track ID number assigned by the LArG4 simulation; what links
      // sim::SimChannel and recob::Hit is the channel ID.

      // In general, objects in the LArSoft reconstruction chain are linked using the art::Assns class:
      // <https://cdcvs.fnal.gov/redmine/projects/larsoft/wiki/Using_art_in_LArSoft#artAssns>

      // The following statement will find the simb::MCTruth associated with the simb::MCParticle
      const art::FindManyP<simb::MCTruth> findManyTruth(particleHandle, event, fSimulationProducerLabel);

      if (!findManyTruth.isValid()) {
        std::cout << "findManyTruth simb::MCTruth for simb::MCParticle failed!" << std::endl;
      }

      size_t particle_index = 0; // only look at first particle in particleHandle's vector.
      auto const& truth = findManyTruth.at(particle_index);

      // Make sure there's no problem.
      if (truth.empty()) {
        std::cout << "Particle ID=" << particleHandle->at(particle_index).TrackId() << " has no primary!" << std::endl;
      }

      // std::cout << "Sec. particle ID=" << particleHandle->at(particle_index).TrackId() << " <-> primary gen PDG=" << truth[0]->GetParticle(0).PdgCode() << std::endl;

      // Example to find what hits are associated with clusters.
      art::Handle<std::vector<recob::Cluster>> clusterHandle;
      if (!event.getByLabel(fClusterProducerLabel, clusterHandle)) return;

      // Note that this is not as trivial a query as the one with MCTruth, since it's
      // possible for a hit to be assigned to more than one cluster.
      const art::FindManyP<recob::Hit> findManyHits(clusterHandle, event, fClusterProducerLabel);

      if (!findManyHits.isValid()) {
        std::cout << "findManyHits recob::Hit for recob::Cluster failed;" << " cluster label='" << fClusterProducerLabel << "'"<< std::endl;
      }

      // Loop over the clusters to see the hits associated with each one.
      /*
      for (size_t cluster_index = 0; cluster_index != clusterHandle->size(); cluster_index++) {

        auto const& hits = findManyHits.at(cluster_index);

        // A vector of pointers to the hits associated with the cluster,
        std::cout << "Cluster ID=" << clusterHandle->at(cluster_index).ID() << " has " << hits.size() << " hits" << std::endl;
      }*/

    } // MyEnergyAnalysis::analyze()

    // This macro has to be defined for this module to be invoked from a
    // .fcl file; see MyEnergyAnalysis.fcl for more information.
    DEFINE_ART_MODULE(MyEnergyAnalysis)

  } // namespace example
} // namespace lar

// Back to our local namespace.
namespace {

  // Define a local function to calculate the detector diagonal.
  double DetectorDiagonal(geo::GeometryCore const& geom)
  {
    const double length = geom.DetLength();
    const double width = 2. * geom.DetHalfWidth();
    const double height = 2. * geom.DetHalfHeight();

    return std::sqrt(cet::sum_of_squares(length, width, height));
  } // DetectorDiagonal()

  // Define a comparison function to use in std::upper_bound and
  // std::lower_bound searches above.
  bool TDCIDETimeCompare(const sim::TDCIDE& lhs, const sim::TDCIDE& rhs)
  {
    return lhs.first < rhs.first;
  }
} // local namespace
