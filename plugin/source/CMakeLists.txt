# WizardOne Audio Modules CMake Configuration
# ============================================================================
# This file creates reusable CMake library targets for the WizardOne audio modules
# so they can be used as clean external dependencies by other projects.
#
# Created targets:
# - WizardCore_Chorus, WizardCore_LFO, WizardCore_DelayLine (base targets)
# - WizardCore::Chorus, WizardCore::LFO, WizardCore::DelayLine (modern aliases)
# - WizardCore_AudioModules, WizardCore::AudioModules (umbrella targets)
#
# Usage in external projects:
# target_link_libraries(MyProject PRIVATE WizardCore::Chorus WizardCore::LFO)

cmake_minimum_required(VERSION 3.22)

    # ============================================================================
    # INDIVIDUAL AUDIO MODULE TARGETS
    # ============================================================================
    
    # Chorus Module
    add_library(WizardCore_Chorus STATIC
        Chorus/Chorus.cpp
    )
    
    target_include_directories(WizardCore_Chorus PUBLIC 
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    
    target_link_libraries(WizardCore_Chorus PUBLIC 
        juce::juce_audio_utils 
        juce::juce_dsp
    )
    
    # LFO Module  
    add_library(WizardCore_LFO STATIC
        LFO/LFO.cpp
    )
    
    target_include_directories(WizardCore_LFO PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    
    target_link_libraries(WizardCore_LFO PUBLIC
        juce::juce_audio_utils
        juce::juce_dsp
    )
    
    # DelayLine Module
    add_library(WizardCore_DelayLine STATIC
        DelayLine/DelayLine.cpp
        DelayLine/BBDelayLine/BBDelayLine.cpp
        DelayLine/DigitalDelayLine/DigitalDelayLine.cpp
    )
    
    target_include_directories(WizardCore_DelayLine PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    
    target_link_libraries(WizardCore_DelayLine PUBLIC
        juce::juce_audio_utils
        juce::juce_dsp
    )
    
    # ============================================================================
    # CREATE MODERN CMAKE ALIASES WITH :: NAMESPACE
    # ============================================================================
    # These aliases follow modern CMake conventions (like juce::juce_audio_utils)
    # and provide a clean, professional interface for external projects
    
    add_library(WizardCore::Chorus ALIAS WizardCore_Chorus)
    add_library(WizardCore::LFO ALIAS WizardCore_LFO)
    add_library(WizardCore::DelayLine ALIAS WizardCore_DelayLine)
    
    # ============================================================================
    # UMBRELLA TARGET (convenience target for importing all modules at once)
    # ============================================================================
    # This allows external projects to link all audio modules with a single target:
    # target_link_libraries(MyProject PRIVATE WizardCore::AudioModules)
    
    add_library(WizardCore_AudioModules INTERFACE)
    target_link_libraries(WizardCore_AudioModules INTERFACE
        WizardCore_Chorus      # Includes Chorus module
        WizardCore_LFO         # Includes LFO module  
        WizardCore_DelayLine   # Includes DelayLine module
    )
    
    # Create modern alias for the umbrella target
    add_library(WizardCore::AudioModules ALIAS WizardCore_AudioModules)
    
message(STATUS "WizardOne audio modules targets created successfully") 