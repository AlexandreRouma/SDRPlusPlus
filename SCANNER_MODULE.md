# SDR++ Scanner Module Documentation

## Overview

The Scanner module is a powerful automated frequency scanning tool for SDR++ that automatically detects and tunes to signals across configurable frequency ranges. It's designed to be both powerful and user-friendly, with features like frequency blacklisting and automatic settings persistence.

## Features

### Core Scanning Capabilities
- **Automated Frequency Scanning**: Continuously scans between user-defined start and stop frequencies
- **Signal Detection**: Automatically detects signals above configurable threshold levels
- **Smart Tuning**: Automatically tunes to detected signals with configurable timing
- **Frequency Range Management**: Handles frequency boundaries with intelligent wrapping

### Advanced Features
- **Frequency Blacklist**: Skip unwanted frequencies with configurable tolerance ranges
- **Settings Persistence**: All configuration automatically saves to `scanner_config.json`
- **Reset Functionality**: Return to start frequency at any time
- **Performance Monitoring**: Real-time status display and logging

## Configuration

### Basic Settings
| Parameter | Description | Default | Range |
|-----------|-------------|---------|-------|
| **Start Frequency** | Beginning of scan range | 88.0 MHz | 0 Hz - 6 GHz |
| **Stop Frequency** | End of scan range | 108.0 MHz | 0 Hz - 6 GHz |
| **Scan Interval** | Frequency step size | 100 kHz | 1 kHz - 1 MHz |
| **Signal Threshold** | Minimum signal level | -50 dB | -100 dB to 0 dB |

### Timing Settings
| Parameter | Description | Default | Range |
|-----------|-------------|---------|-------|
| **Tuning Time** | Wait time after tuning | 250 ms | 100 ms - 5000 ms |
| **Linger Time** | Time on detected signal | 1000 ms | 100 ms - 10000 ms |

### Blacklist Settings
| Parameter | Description | Default | Range |
|-----------|-------------|---------|-------|
| **Blacklist Tolerance** | Frequency range around blacklisted frequencies | 1.0 kHz | 100 Hz - 100 kHz |

## User Interface

### Main Controls
- **Start/Stop Button**: Begin or halt scanning operation
- **Reset Button**: Return scanner to start frequency
- **Frequency Inputs**: Set start, stop, and interval frequencies
- **Threshold Slider**: Adjust signal detection sensitivity

### Blacklist Management
- **Add Frequency**: Input frequency to add to blacklist
- **Remove Individual**: Remove specific blacklisted frequencies
- **Clear All**: Remove all blacklisted frequencies
- **Tolerance Setting**: Configure blacklist frequency range

### Status Display
- **Current Frequency**: Shows active scanning frequency
- **Scan Direction**: Indicates upward or downward scanning
- **Signal Status**: Shows if signal is detected
- **Performance Info**: Displays scan cycles and status

## Usage Guide

### Getting Started
1. **Enable Module**: Add scanner module in SDR++ Module Manager
2. **Set Frequency Range**: Configure start and stop frequencies
3. **Adjust Parameters**: Set appropriate interval and threshold
4. **Start Scanning**: Click start button to begin automated scanning

### Optimizing Performance
- **Scan Interval**: Smaller intervals provide more thorough coverage but slower scanning
- **Signal Threshold**: Set appropriately to avoid false triggers from noise
- **Tuning Time**: Balance between responsiveness and stability
- **Linger Time**: Longer times allow more signal analysis

### Blacklist Management
- **Add Interference**: Identify and blacklist known interference sources
- **Set Tolerance**: Configure appropriate frequency ranges around blacklisted frequencies
- **Regular Maintenance**: Review and update blacklist as needed

### Troubleshooting
- **Scanner Stuck**: Use reset button if scanner gets stuck at range boundaries
- **False Triggers**: Adjust signal threshold to reduce false detections
- **Slow Performance**: Increase scan interval for faster scanning
- **Settings Not Saving**: Check file permissions for `scanner_config.json`

## Technical Details

### Architecture
- **Multi-threaded Design**: Separate worker thread for scanning operations
- **Real-time Processing**: 10Hz scan loop for responsive operation
- **FFT Integration**: Uses SDR++ waterfall data for signal detection
- **ConfigManager Integration**: Leverages SDR++ configuration system

### Performance Characteristics
- **CPU Usage**: Moderate during active scanning, low when idle
- **Memory Usage**: Minimal memory footprint
- **Response Time**: <100ms frequency switching capability
- **Accuracy**: Frequency accuracy depends on SDR hardware

### File Structure
```
scanner_config.json          # Module configuration file
scanner.dylib               # Compiled module (macOS)
scanner.so                  # Compiled module (Linux)
scanner.dll                 # Compiled module (Windows)
```

## Development

### Building from Source
```bash
# Navigate to SDR++ source directory
cd SDRPlusPlus

# Create build directory
mkdir build && cd build

# Configure with CMake
cmake .. -DOPT_BUILD_SCANNER=ON

# Build scanner module
make scanner
```

### Source Code Structure
- **main.cpp**: Main module implementation
- **ScannerModule Class**: Core scanning logic
- **Worker Thread**: Background scanning operations
- **UI Integration**: ImGui interface components

### Key Functions
- `worker()`: Main scanning loop
- `findSignal()`: Signal detection algorithm
- `saveConfig()` / `loadConfig()`: Configuration persistence
- `menuHandler()`: User interface management

## Changelog

### Recent Enhancements
- **Frequency Blacklist**: Added ability to skip unwanted frequencies
- **Settings Persistence**: Automatic saving/loading of all configuration
- **Reset Functionality**: Added reset button for better user control
- **Frequency Wrapping**: Improved handling of frequency range boundaries
- **Performance Monitoring**: Added real-time status display

### Future Improvements
- **Scan Patterns**: Customizable scanning patterns and sequences
- **Signal Recording**: Automatic recording of detected signals
- **Advanced Filtering**: More sophisticated signal detection algorithms
- **Integration**: Better integration with other SDR++ modules

## Support

For issues, feature requests, or questions about the scanner module:
- **GitHub Issues**: Report bugs or request features
- **Discord Server**: Get help from the SDR++ community
- **Documentation**: Check this file for usage information

## License

The Scanner module is part of SDR++ and follows the same licensing terms as the main project.
