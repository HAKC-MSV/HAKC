Layout for spatial fields in a 64 bit mask

<!-- @formatter:off --> 

63                        15                 0 Bits   
┌─────────────────────────┬──────────────────┐   
│          48             │       16         │ Field size    
│─────────────────────────┼──────────────────│   
│                         │                  │   
│     Compartment ID      │    Division ID   │ Spatial Compartmentalization 
│                         │                  │   
└─────────────────────────┴──────────────────┘
63                        15        7        0


Reserved values:
No Enforcement Compartment: Compartment = 0, Division = 0

The 'No Enforcement' values are special values that will have hakc code 'ignore' those symbols.
Since both "No Enforcement" values are 0, one can simply check if a field is zero in the Spatio-Temporal case. 
The exact values for each field can simply be extracted from the mask. 

<!-- @formatter:on -->

