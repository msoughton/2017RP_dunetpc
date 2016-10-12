////////////////////////////////////////////////////////////////////////
/// \file  ChannelMapCRMAlg.h
/// \brief Interface to algorithm class for a dual-phase detector channel mapping
///
/// \version $Id:  $
/// \author  brebel@fnal.gov vgalymov@ipnl.in2p3.fr
////////////////////////////////////////////////////////////////////////
#ifndef GEO_CHANNELMAPCRMALG_H
#define GEO_CHANNELMAPCRMALG_H

#include <vector>
#include <set>
#include <iostream>

#include "larcoreobj/SimpleTypesAndConstants/readout_types.h"
#include "larcoreobj/SimpleTypesAndConstants/geo_types.h"
#include "larcore/Geometry/ChannelMapAlg.h"
#include "dune/Geometry/GeoObjectSorterCRM.h"
#include "fhiclcpp/ParameterSet.h"

namespace geo{

  class ChannelMapCRMAlg : public ChannelMapAlg{

  public:
    

    ChannelMapCRMAlg(fhicl::ParameterSet const& p);
    
    void                     Initialize( GeometryData_t& geodata ) override;
    void                     Uninitialize();
    std::vector<WireID>      ChannelToWire(raw::ChannelID_t channel)     const;
    unsigned int             Nchannels()                                 const;
    /// @brief Returns the number of channels in the specified ROP
    /// @return number of channels in the specified ROP, 0 if non-existent
    /// @todo Needs to be implemented
    virtual unsigned int Nchannels(readout::ROPID const& ropid) const override;
   
   

    //@{
    virtual double WireCoordinate
      (double YPos, double ZPos, geo::PlaneID const& planeID) const override;
    virtual double WireCoordinate(double YPos, double ZPos,
                                 unsigned int PlaneNo,
                                 unsigned int TPCNo,
                                 unsigned int cstat) const
      { return WireCoordinate(YPos, ZPos, geo::PlaneID(cstat, TPCNo, PlaneNo)); }
    //@}
    
    //@{
    virtual WireID NearestWireID
      (const TVector3& worldPos, geo::PlaneID const& planeID) const override;
    virtual WireID NearestWireID(const TVector3& worldPos,
                                 unsigned int    PlaneNo,
                                 unsigned int    TPCNo,
                                 unsigned int    cstat) const override
      { return NearestWireID(worldPos, geo::PlaneID(cstat, TPCNo, PlaneNo)); }
    //@}
    
    //@{
    virtual raw::ChannelID_t PlaneWireToChannel
      (geo::WireID const& wireID) const override;
    virtual raw::ChannelID_t PlaneWireToChannel(unsigned int plane,
                                                unsigned int wire,
                                                unsigned int tpc,
                                                unsigned int cstat) const override
      { return PlaneWireToChannel(geo::WireID(cstat, tpc, plane, wire)); }
    //@}
    
    virtual View_t                   View( raw::ChannelID_t const channel )       const override;
    virtual SigType_t                SignalType( raw::ChannelID_t const channel ) const override;
    virtual std::set<View_t>  const& Views()                                      const override;
    virtual std::set<PlaneID> const& PlaneIDs()                                   const override;
  
    
    
    //
    // TPC set interface
    //
    /// @name TPC set mapping
    /// @{
    /**
     * @brief Returns the total number of TPC sets in the specified cryostat
     * @param cryoid cryostat ID
     * @return number of TPC sets in the cryostat, or 0 if no cryostat found
     */
    virtual unsigned int NTPCsets(readout::CryostatID const& cryoid) const override;
    
    /// Returns the largest number of TPC sets any cryostat in the detector has
    virtual unsigned int MaxTPCsets() const override;
    
    /// Returns whether we have the specified TPC set
    /// @return whether the TPC set is valid and exists
    virtual bool HasTPCset(readout::TPCsetID const& tpcsetid) const override;
    
    /// Returns the ID of the TPC set tpcid belongs to
    virtual readout::TPCsetID TPCtoTPCset(geo::TPCID const& tpcid) const override;
    
    /**
     * @brief Returns a list of ID of TPCs belonging to the specified TPC set
     * @param tpcsetid ID of the TPC set to convert into TPC IDs
     * @return the list of TPCs, empty if TPC set is invalid
     *
     * Note that the check is performed on the validity of the TPC set ID, that
     * does not necessarily imply that the TPC set specified by the ID actually
     * exists. Check the existence of the TPC set first (HasTPCset()).
     * Behaviour on valid, non-existent TPC set IDs is undefined.
     */
    virtual std::vector<geo::TPCID> TPCsetToTPCs
      (readout::TPCsetID const& tpcsetid) const override;
    
    /// Returns the ID of the first TPC belonging to the specified TPC set
    virtual geo::TPCID FirstTPCinTPCset
      (readout::TPCsetID const& tpcsetid) const override;
    
    /// @} TPC set mapping
    
    
    //
    // Readout plane interface
    //
    /// @name Readout plane mapping
    /// @{
    /**
     * @brief Returns the total number of ROP in the specified TPC set
     * @param tpcsetid TPC set ID
     * @return number of readout planes in the TPC set, or 0 if no TPC set found
     * 
     * Note that this methods explicitly check the existence of the TPC set.
     */
    virtual unsigned int NROPs(readout::TPCsetID const& tpcsetid) const override;
    
    /// Returns the largest number of ROPs a TPC set in the detector has
    virtual unsigned int MaxROPs() const override;
    
    /// Returns whether we have the specified ROP
    /// @return whether the readout plane is valid and exists
    virtual bool HasROP(readout::ROPID const& ropid) const override;
    
    /// Returns the ID of the ROP planeid belongs to
    virtual readout::ROPID WirePlaneToROP
      (geo::PlaneID const& planeid) const override;
    
    /// Returns a list of ID of planes belonging to the specified ROP
    virtual std::vector<geo::PlaneID> ROPtoWirePlanes
      (readout::ROPID const& ropid) const override;
    
    /// Returns the ID of the first plane belonging to the specified ROP
    virtual geo::PlaneID FirstWirePlaneInROP
      (readout::ROPID const& ropid) const override;
    
    /**
     * @brief Returns a list of ID of TPCs the specified ROP spans
     * @param ropid ID of the readout plane
     * @return the list of TPC IDs, empty if readout plane ID is invalid
     *
     * Note that this check is performed on the validity of the readout plane
     * ID, that does not necessarily imply that the readout plane specified by
     * the ID actually exists. Check if the ROP exists with HasROP().
     * The behaviour on non-existing readout planes is undefined.
     */
    virtual std::vector<geo::TPCID> ROPtoTPCs
      (readout::ROPID const& ropid) const override;
    
    /// Returns the ID of the ROP the channel belongs to
    /// @throws cet::exception (category: "Geometry") if non-existent channel
    virtual readout::ROPID ChannelToROP(raw::ChannelID_t channel) const override;
    
    /**
     * @brief Returns the ID of the first channel in the specified readout plane
     * @param ropid ID of the readout plane
     * @return ID of first channel, or raw::InvalidChannelID if ID is invalid
     * 
     * Note that this check is performed on the validity of the readout plane
     * ID, that does not necessarily imply that the readout plane specified by
     * the ID actually exists. Check if the ROP exists with HasROP().
     * The behaviour for non-existing readout planes is undefined.
     */
    virtual raw::ChannelID_t FirstChannelInROP
      (readout::ROPID const& ropid) const override;
    
    /// @} Readout plane mapping
  
  private:
    
    unsigned int                  fNcryostat;      ///< number of cryostats in the detector
    unsigned int                  fNchannels;      ///< number of channels in the detector
    raw::ChannelID_t              fTopChannel;     ///< book keeping highest channel #
    std::vector<unsigned int>     fNTPC;           ///< number of TPCs in each cryostat
    std::set<View_t>              fViews;          ///< vector of the views present in the detector
    std::set<PlaneID>             fPlaneIDs;       ///< vector of the PlaneIDs present in the detector
    PlaneInfoMap_t<float>         fFirstWireProj;  ///< Distance (0,0,0) to first wire          
                                                   ///< along orth vector per plane per TPC
    PlaneInfoMap_t<float>         fOrthVectorsY;   ///< Unit vectors orthogonal to wires in
    PlaneInfoMap_t<float>         fOrthVectorsZ;   ///< each plane - stored as 2 components
                                                   ///< to avoid having to invoke any bulky
                                                   ///< TObjects / CLHEP vectors etc         
    PlaneInfoMap_t<float>         fWireCounts;     ///< Number of wires in each plane - for
                                                   ///< range checking after calculation   
    TPCInfoMap_t<unsigned int>    fNPlanes;        ///< Number of planes in each TPC - for
                                                   ///< range checking after calculation   
    PlaneInfoMap_t<unsigned int>  fPlaneBaselines; ///< The number of wires in all the 
                                                   ///< tpcs and planes up to this one 
                                                   ///< in the heirachy
    PlaneInfoMap_t<unsigned int>  fWiresPerPlane;  ///< The number of wires in this plane 
                                                   ///< in the heirachy
    geo::GeoObjectSorterCRM  fSorter;              ///< class to sort geo objects
    
    
    /// Retrieved the wire cound for the specified plane ID
    unsigned int WireCount(geo::PlaneID const& id) const
    { return AccessElement(fWireCounts, id); }
    
  };
}
#endif // GEO_CHANNELMAPCRMDALG_H

