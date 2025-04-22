# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.1.0] - 2025-04-23

### Added

- Support for deletion URLs provided by hosts to remove uploaded files
- New `delete-file` command to delete files from remote hosts using deletion URLs
- New `delete-upload` command to remove upload records from local history
- Visual indicators in the upload history for files with deletion URLs
- Improved error handling for network operations

## [1.0.0] - 2025-04-22

### Added

- Initial release of Hostman.
- Core functionality for uploading files via command-line.
- Support for multiple configurable hosting services using JSON configuration.
- Management commands for hosts (`add-host`, `remove-host`, `list-hosts`, `set-default-host`).
- Upload history tracking using SQLite (`list-uploads`).
- Secure API key storage with encryption (AES-256-GCM).
- Basic configuration management (`config get`, `config set`).
- Visual progress bar during uploads (requires ncurses).
- Detailed logging to a configurable file.
- Support for different authentication types (e.g., bearer token).
- Support for multipart form data uploads.
- Ability to specify static form fields in host configuration.
- JSON path parsing for extracting the uploaded URL from the host response.
- Build system using CMake.
- Basic command-line interface parsing.
````