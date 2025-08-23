# Mr. Wizard API Design Rules

# Do not modify this file without initial approval (per change) from the developer.

## Preferred Nomenclature Consistency
    - WaveShape (not Waveform)
    - When referencing rate, clarify rate type:
      - Modulation Rate? LFO Frequency? Etc.

## General Practices

- New development should never break potential legacy code.  When so, the designer should be alerted before moving forward.
  
- Use JUCE native methods, constants and practices where applicable. 
  
- All modules should have individual setters/getters to update each parameter individually, as well as a single updateParameters method to pass all arguments in one go, with any one of the arguments being optional.

- "Key Parameters" are all critical values which may be of interest to be exposed to either end-users or developers to tune the sound or behavior of a product o r module.  Default key parameter values should be initialized in a "configure" array in the header, and then the initializations should reference this array.  No key parameter values should be hardwired into the code.  This includes relevant module parameters but does NOT mundane values like mathematical constants or intermediate calculations.

- Maintain modularity and DRY principles.  When parameters are assigned internal to a class, they should use the designated internal setters/getters rather than additional longform code, when possible.

- For user facing parameters, prefer Degrees over Radians.

# Cursor Instructions

- Due to the general workflow, the software is not built in Cursor or VSCode but is ported to Visual Studio.  Because of this, you cannot actually run the code you build.  You do not need to spend effort attempting to run the code.

- In general, conform new development or relevant updates to these design rules - but do not go out of your way to tear up old code to conform it.

- Refer to the associated MD files when implementing development, but refrain from spending time updating it until asked.

- Include copious comments and explanations.

- When adding new content, consider method positioning in order to keep relevant methods sectioned together.  DSP should remain together; setters/getters remain together, helper/utility functions remain together, etc.  For existing code, do not spend effort on moving code around if it is not the subject of discussion.

- Variables and intermediate values should be heavily clarified, showing details and units.