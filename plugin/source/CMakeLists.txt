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
    # INDIVIDUAL AUDIO MODULE TARGETS (Header-Only for Fast Builds)
    # ============================================================================
    # These are INTERFACE libraries - they provide headers and source files
    # but don't compile separately, avoiding duplicate JUCE compilation
    
    # Chorus Module
    add_library(WizardCore_Chorus INTERFACE)
    
    target_sources(WizardCore_Chorus INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/Chorus/Chorus.cpp
    )
    
    target_include_directories(WizardCore_Chorus INTERFACE 
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    
    target_link_libraries(WizardCore_Chorus INTERFACE 
        juce::juce_audio_utils 
        juce::juce_dsp
    )
    
    # LFO Module  
    add_library(WizardCore_LFO INTERFACE)
    
    target_sources(WizardCore_LFO INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/LFO/LFO.cpp
    )
    
    target_include_directories(WizardCore_LFO INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    
    target_link_libraries(WizardCore_LFO INTERFACE
        juce::juce_audio_utils
        juce::juce_dsp
    )
    
    # DelayLine Module
    add_library(WizardCore_DelayLine INTERFACE)
    
    target_sources(WizardCore_DelayLine INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/DelayLine/DelayLine.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/DelayLine/BBDelayLine/BBDelayLine.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/DelayLine/DigitalDelayLine/DigitalDelayLine.cpp
    )
    
    target_include_directories(WizardCore_DelayLine INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    
    target_link_libraries(WizardCore_DelayLine INTERFACE
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