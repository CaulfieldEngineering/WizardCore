# Mandatory line, sets the minimum version of CMake that should be used with this repository.
# I specified 3.22 because I trust it. However, currently I have 3.26 installed on my machine.
# To verify your version run
# $ cmake --version
# =====================================================================
cmake_minimum_required(VERSION 3.22)


# Include the configuration file
# =====================================================================
include(${CMAKE_CURRENT_SOURCE_DIR}/config.cmake)


# Use the variables defined in config.cmake
# =====================================================================
message(STATUS "CMakeLists.txt: Setting configuration variables.")
project(${wPLUGIN_NAME} VERSION ${wPROJECT_VERSION})

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Always use the newest C++ standard on green-field projects if possible.
# =====================================================================
message(STATUS "CMakeLists.txt: Assigning C++ standard.")
set(CMAKE_CXX_STANDARD 23)
if (APPLE)
    # On Mac, we need to wait for a new JUCE version that fixes the compilation issue
    set(CMAKE_CXX_STANDARD 20)
endif()




# I like to download the dependencies to the same folder as the project.
# If you want to install them system wide, set CPM_SOURCE_CACHE with the path to the dependencies
# either as an environment variable or pass it to the cmake script with -DCPM_SOURCE_CACHE=<path>.
# =====================================================================
set(LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libs)


# Downloads CPM if not already downloaded. CPM is an easy-to-use package manager nicely integrated with CMake.
# =====================================================================
include(cmake/cpm.cmake)

# This command allows running tests from the "build" folder (the one where CMake generates the project to).
# =====================================================================
enable_testing()

# Conditional Building: Detect if WizardOne is being built directly or imported
# =====================================================================
# This automatic detection allows WizardOne to serve dual purposes:
# 1. Standalone plugin project (when built directly)
# 2. Audio module library (when imported by other projects via CPM)
#
# How the detection works:
# - CMAKE_PROJECT_NAME = the top-level project being built (never changes)
# - PROJECT_NAME = the current project being processed (this project)
# 
# Examples:
# - Building WizardOne directly: CMAKE_PROJECT_NAME="WizardOne", PROJECT_NAME="WizardOne" → Equal
# - ChorusSandbox imports WizardOne: CMAKE_PROJECT_NAME="ChorusSandbox", PROJECT_NAME="WizardOne" → Not equal

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    # ============================================================================
    # DIRECT BUILD MODE: Building WizardCore as the main project
    # ============================================================================
    # This happens when someone runs: cd WizardCore && cmake -S . -B build
    # Or when CI runs to verify the library compiles.
    # We only build the audio modules — there is no standalone plugin.

    message(STATUS "WizardOne: Building as main project (audio modules only)")

    CPMAddPackage(
        NAME JUCE
        GIT_TAG 8.0.8
        GITHUB_REPOSITORY juce-framework/JUCE
        SOURCE_DIR ${LIB_DIR}/juce
    )

    add_subdirectory(plugin/source)

else()
    # ============================================================================
    # LIBRARY MODE: WizardOne imported as audio module provider
    # ============================================================================
    # This happens when another project imports WizardOne via CPM, such as:
    # CPMAddPackage(NAME WizardOne GITHUB_REPOSITORY CaulfieldEngineering/WizardOne)
    # Only create the WizardCore:: audio module targets, skip the full plugin
    
    message(STATUS "WizardOne: Building as subproject (audio modules only)")
    message(STATUS "  → Main project: ${CMAKE_PROJECT_NAME}")
    message(STATUS "  → Creating WizardCore:: targets for external use")
    
    add_subdirectory(plugin/source)  # Only the audio modules (Chorus, LFO, DelayLine)
    
endif()

# Adds all the targets configured in the "test" folder.
# =====================================================================
#add_subdirectory(test)

