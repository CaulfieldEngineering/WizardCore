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
    add_library(WizardCore::Chorus STATIC
        Chorus/Chorus.cpp
    )
    
    target_include_directories(WizardCore::Chorus PUBLIC 
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    
    target_link_libraries(WizardCore::Chorus PUBLIC 
        juce::juce_audio_utils 
        juce::juce_dsp
    )
    
    # LFO Module  
    add_library(WizardCore::LFO STATIC
        LFO/LFO.cpp
    )
    
    target_include_directories(WizardCore::LFO PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    
    target_link_libraries(WizardCore::LFO PUBLIC
        juce::juce_audio_utils
        juce::juce_dsp
    )
    
    # DelayLine Module
    add_library(WizardCore::DelayLine STATIC
        DelayLine/DelayLine.cpp
        DelayLine/BBDelayLine/BBDelayLine.cpp
        DelayLine/DigitalDelayLine/DigitalDelayLine.cpp
    )
    
    target_include_directories(WizardCore::DelayLine PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    
    target_link_libraries(WizardCore::DelayLine PUBLIC
        juce::juce_audio_utils
        juce::juce_dsp
    )
    
    # ============================================================================
    # UMBRELLA TARGET (optional convenience target)
    # ============================================================================
    
    # Create an interface target that includes all audio modules
    add_library(WizardCore::AudioModules INTERFACE)
    target_link_libraries(WizardCore::AudioModules INTERFACE
        WizardCore::Chorus
        WizardCore::LFO
        WizardCore::DelayLine
    )
    
    message(STATUS "WizardOne audio modules targets created successfully")
    
endif() 