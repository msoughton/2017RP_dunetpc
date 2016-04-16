// BasicTrigger.h
//
// Michael Baird
// March 2016
//
// Basic trigger data product class for the DAQ trigger service framework.
//

#ifndef BASICTRIGGER_H
#define BASICTRIGGER_H

// DUNETPC specific includes
#include "dune/DAQTriggerSim/TriggerDataProducts/TriggerTypes.h"

// Framework includes

// C++ includes
#ifndef __GCCXML__
#include <iosfwd> // std::ostream
#endif



namespace triggersim {

  class BasicTrigger {
    
  public:
    
    // Constructor...
    BasicTrigger(unsigned int trigtype = kNullTrigger,
		 bool trigissued = false);
    
    // Destructor...
    ~BasicTrigger();

    // Set and Get functions for the trigger type
    void setTrigType(unsigned int trigtype);
    unsigned int TrigType() const;

    // Set and Get functions for the trigger decision variable
    void setTrigDecision(bool trigdecision);
    bool TrigDecision() const;

    friend std::ostream& operator << (std::ostream& o, BasicTrigger const& bt);
    friend bool          operator <  (BasicTrigger const& a, BasicTrigger const& b);    

  private:
    
    // Parameters.
    unsigned int fTrigType;
    bool         fTrigDecision;
    
  };
} // end namespace triggersim

#endif
