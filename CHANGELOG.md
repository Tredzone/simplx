# Change Log

The format is based on [Keep a Changelog](http://keepachangelog.com/) </br>
Date format: YYYY-MM-DD </br>
Sub-sections: Added, Removed, Depreceted </br>

<!---
## [Unreleased]
### Added
### Removed
### Deprecated
### Refactored
--->

## [2.7.1] - 2019-04-24

### Added
- tcp connector comments cleanup
- IWaitCondition::wait_for()
  
### Removed
- TREDZONE_ENGINE_VERSION_BRANCH


## [2.7.0] -2019-04-08

### Added
- asynchronous TCP/IP communication layer
  - HTTP server layer
  

## [2.6.10] - 2019-03-22

- CWE and readability fixes

  
## [2.6.9] - 2019-03-15

- upgraded to gcc 8.2 & clang 4.0 compatibility
- removed Actor::Event::SerialBuffer from actor.h
- renamed SerialBufferChain to SerialBuffer
- rewrote SerialBuffer using ::mmap()
  - no longer templetized
  - no longer has constructor arguments


### Deprecated

- removed TREDZONE_SDK_IS_COMPILER_COMPATIBLE macro
- removed TREDZONE_SDK_IS_DEBUG_RELEASE_COMPATIBLE macro
- removed SDK_ARCHITECTURE constant
- removed checkRuntimeCompatibility() static function
- removed RuntimeCompatibilityExceptions:
  E_COMPILER_VERSION
  E_DEBUG_RELEASE
- removed TRZ_DEBUG() macro


### Enterprise
- ServerTCPListenSocket & EngineToEngineSocketConnectorActor constructors take host (string) and port (int)
  - for server, use "0.0.0.0" to listen on any interface
- added e2e unit tests
- changed e2e protocol headers to use 32-bit event type IDs


## [2.6.8] - 2019-01-31

### bug fixes
- fixed unused variable warning treated as error


## [2.6.7] - 2019-01-07

### bug fixes
- fixed template resolution bug appearing under gcc 6/7 in include/trz/engine/actor.h in ActorWrapper::delete() where traversal of diamond-class inheritance would yield a corrupt pointer and trigger a segfault


## [2.6.6] - 2018-12-18

### Added
- variadic Event::Pipe::push<Event>(...)
  - accompanying documentation
- asynchronous KeyboardActor
  - accompanying tutorial using new IWaitCondition for deterministic engine exit
- #define TRACE_REF for Actor lifetime tracing
  - will save one log file per core upon engine destruction
  - accompanying documentation
- #undef TREDZONE_CHECK_CYCLICAL_REFS to skip cyclical reference checks in RELEASE mode (enabled by default)
- docker_test.sh Bash script will now stop on compilation error
- Raspberry Pi ARM support under Raspbian


### Refactored
- AsyncService -> Service (alias removed)
- #define DEBUG/NDEBUG/RELEASE to allow for Linux & Microsoft practices
- LogTypeEnum::DEBUG renamed to LogTypeEnum::LOG_T_DEBUG to side-step any previous #define DEBUG 

### Deprecated
- TREDZONE_CPP11_SUPPORT (C++11 is required)


## [2.6.5] - 2018-06-25

### Added
- util subdir
- TimerActor

### Refactored
- AsyncActor -> Actor (alias removed)
- AsyncEngine -> Engine (alias removed)
- namespace trz -> tredzone (alias removed)
- TimerProxy::onTimeOut() -> TimerProxy::onTimeout()  (note casing!)
- bool TREDZONE_SDK_COMPATIBLE() -> removed


## [2.6.4] - 2015-06-01
Initial public OSS git version


