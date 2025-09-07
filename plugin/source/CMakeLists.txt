# WizardOne Audio Modules CMake Configuration
# This file creates library targets for the WizardOne audio modules
# so they can be used as external dependencies by other projects

cmake_minimum_required(VERSION 3.22)

# Only create targets if this is being used as a subproject
# (i.e., when included by CPM or add_subdirectory)
if(NOT CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)

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
    # CREATE ALIASES WITH :: NAMESPACE
    # ============================================================================
    
    # Create aliases that follow modern CMake conventions
    add_library(WizardCore::Chorus ALIAS WizardCore_Chorus)
    add_library(WizardCore::LFO ALIAS WizardCore_LFO)
    add_library(WizardCore::DelayLine ALIAS WizardCore_DelayLine)
    
    # ============================================================================
    # UMBRELLA TARGET (optional convenience target)
    # ============================================================================
    
    # Create an interface target that includes all audio modules
    add_library(WizardCore_AudioModules INTERFACE)
    target_link_libraries(WizardCore_AudioModules INTERFACE
        WizardCore_Chorus
        WizardCore_LFO
        WizardCore_DelayLine
    )
    
    add_library(WizardCore::AudioModules ALIAS WizardCore_AudioModules)
    
    message(STATUS "WizardOne audio modules targets created successfully")
    
endif() 