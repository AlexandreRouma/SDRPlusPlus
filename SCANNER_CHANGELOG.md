# Scanner Module Changelog

## Version 1.1.0 - Enhanced Scanner Features

### New Features
- **Frequency Blacklist System**
  - Add frequencies to skip during scanning
  - Configurable tolerance range around blacklisted frequencies
  - Individual frequency removal or clear all functionality
  - Real-time blacklist management in UI

- **Settings Persistence**
  - Automatic saving of all scanner configuration
  - JSON-based configuration storage (`scanner_config.json`)
  - Settings restored on application restart
  - Integration with SDR++ ConfigManager system

- **Enhanced User Interface**
  - Reset button for returning to start frequency
  - Improved frequency input controls
  - Better status display and information
  - Enhanced blacklist management interface

### Improvements
- **Frequency Wrapping Logic**
  - Better handling of frequency range boundaries
  - Improved scan direction changes
  - More robust frequency calculations
  - Better error handling for edge cases

- **Scanner Logic**
  - Enhanced signal detection algorithm
  - Improved frequency stepping
  - Better timing control for tuning and lingering
  - More reliable scan loop operation

### Technical Enhancements
- **Code Quality**
  - Better error handling and logging
  - Improved memory management
  - Enhanced thread safety
  - Better integration with SDR++ core

- **Configuration Management**
  - Uses SDR++ ConfigManager for persistence
  - Automatic configuration file creation
  - Default value handling
  - Configuration validation

### Bug Fixes
- Fixed scanner getting stuck at frequency range boundaries
- Improved frequency calculation accuracy
- Better handling of edge cases in scanning logic
- Enhanced error recovery mechanisms

## Version 1.0.0 - Base Scanner Functionality

### Initial Features
- Basic frequency scanning between start and stop frequencies
- Configurable scan interval and signal threshold
- Automatic signal detection and tuning
- Basic user interface for scanner control

### Core Capabilities
- Frequency range scanning
- Signal level detection
- Automatic frequency tuning
- Configurable timing parameters

---

## Development Notes

### Implementation Details
- **Language**: C++17
- **UI Framework**: ImGui
- **Configuration**: JSON with ConfigManager
- **Threading**: Multi-threaded with worker thread
- **Integration**: SDR++ module system

### Build Requirements
- CMake 3.5+
- C++17 compatible compiler
- SDR++ core dependencies
- ImGui library

### Testing
- Tested on macOS ARM (Apple Silicon)
- Verified with various frequency ranges
- Blacklist functionality validated
- Settings persistence confirmed working

---

## Future Enhancements

### Planned Features
- **Scan Patterns**: Customizable scanning sequences
- **Signal Recording**: Automatic recording of detected signals
- **Advanced Filtering**: More sophisticated signal detection
- **Integration**: Better integration with other SDR++ modules

### Potential Improvements
- **Performance**: Further CPU usage optimization
- **UI**: Enhanced visualization and controls
- **Automation**: Scheduled scanning capabilities
- **Export**: Scan results export functionality
