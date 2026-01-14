cmake_minimum_required(VERSION 3.16)
project(BetaDecayViz LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Works with SFML 2.5 style packages.
# If you use SFML 3, adjust find_package and target names as needed.
find_package(SFML 3 REQUIRED COMPONENTS Graphics Window System)

add_executable(BetaDecayViz main.cpp)
target_link_libraries(BetaDecayViz PRIVATE SFML::Graphics SFML::Window SFML::System)
