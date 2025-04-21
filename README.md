# Hostman - File Upload Command-Line Tool

Hostman is a robust, cross-platform command-line application for uploading files (primarily images) to various hosting services. It prioritizes usability, scriptability, and maintainability.

## Features

- Upload files to multiple configurable hosting services
- Manage hosting service configurations
- Track upload history using SQLite
- Secure API key storage with encryption
- Visual progress bar during uploads
- Detailed logging

## Installation

### Prerequisites

- C compiler (GCC or Clang)
- CMake (3.12+)
- libcurl
- cJSON or jansson
- SQLite3
- (Optional) ncurses for TUI features

### Building from Source

```bash
# Clone the repository
git clone https://github.com/yourusername/hostman.git
cd hostman

# Create build directory
mkdir build && cd build

# Configure and build
cmake ..
make

# Install (optional)
sudo make install
```

## Usage
### First Run

When running Hostman for the first time, it will guide you through setting up your first host configuration.
Basic Commands
```bash
# Upload a file using the default host
hostman upload path/to/file.png

# Upload with a specific host
hostman upload --host anonhost_personal path/to/file.png

# List all configured hosts
hostman list-hosts

# Add a new host (interactive)
hostman add-host

# Remove a host
hostman remove-host anonhost_personal

# Set default host
hostman set-default-host anonhost_personal

# View upload history
hostman list-uploads

# View upload history with pagination
hostman list-uploads --page 2 --limit 10

# View/modify configuration
hostman config get log_level
hostman config set log_level DEBUG
```

## Configuration

Hostman uses a JSON configuration file located at `$HOME/.config/hostman/config.json`. The structure is as follows:
```json
{
  "version": 1,
  "default_host": "anonhost_personal",
  "log_level": "INFO",
  "log_file": "/path/to/log/file.log",
  "hosts": {
    "anonhost_personal": {
      "api_endpoint": "https://anon.love/api/upload",
      "auth_type": "bearer",
      "api_key_name": "Authorization",
      "api_key_encrypted": "...",
      "request_body_format": "multipart",
      "file_form_field": "file",
      "static_form_fields": {
        "public": "false"
      },
      "response_url_json_path": "url"
    }
  }
}
```

## Security

API keys are encrypted in the configuration file. By default, Hostman uses AES-256-GCM encryption with a key derived from file permissions.

## Troubleshooting

###Common Issues

    Upload fails: Check your API key and internet connection
    Permission denied: Ensure proper file permissions for the config directory
    Missing libraries: Install required dependencies

## Logs

Check the log file specified in your configuration for detailed error information.

## License

This project is licensed under the MIT License.