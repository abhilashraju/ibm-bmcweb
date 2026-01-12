# IBM BMCWeb Project

This project uses bmcweb from [ibm-openbmc/bmcweb](git@github.com:ibm-openbmc/bmcweb.git) as a subproject dependency.

## Architecture Overview

```mermaid
graph TB
    subgraph Clients["Client Applications"]
        Browser["Web Browser<br/>(HTTPS)"]
        REST["REST Client<br/>(Redfish API)"]
        WebUI["WebUI-Vue<br/>(Frontend)"]
    end

    subgraph IBMBMCWeb["IBM BMCWeb Application"]
        Main["webserver_main.cpp<br/>(Entry Point)"]
        
        subgraph IBMExtensions["IBM Extensions<br/>(Outside bmcweblib)"]
            BMCGroup["redfish/bmc_group.hpp<br/>(BMC Group Management)"]
            IBMApp["ibm_app.hpp<br/>(IBM Application Logic)"]
        end
        
        subgraph BMCWebSub["BMCWeb Subproject<br/>(ibm-openbmc/bmcweb)"]
            BMCWebLib["bmcweblib<br/>(Static Library)"]
            
            subgraph WebServer["Web Server Core"]
                HTTP["HTTP/HTTPS Server<br/>(Boost.Beast)"]
                WS["WebSocket Support"]
                Auth["Authentication &<br/>Authorization"]
                Session["Session Management"]
            end
            
            subgraph RedfishCore["Redfish Core Libraries"]
                Account["Account Service"]
                System["System Management"]
                Event["Event Service"]
                Update["Update Service"]
                License["License Service<br/>(IBM Extension)"]
            end
        end
    end

    subgraph Dependencies["System Dependencies"]
        DBus["D-Bus<br/>(sdbusplus)"]
        SSL["OpenSSL<br/>(TLS/SSL)"]
        SystemD["systemd<br/>(logging)"]
        XML["tinyxml2<br/>(XML)"]
        Zlib["zlib<br/>(compression)"]
        JSON["nlohmann_json<br/>(JSON)"]
        PAM["PAM<br/>(auth)"]
        Zstd["libzstd<br/>(compression)"]
    end

    subgraph BMCServices["BMC Services"]
        InventoryMgr["Inventory Manager"]
        NetworkMgr["Network Manager"]
        UserMgr["User Manager"]
        LogMgr["Log Manager"]
        UpdateMgr["Update Manager"]
        LicenseMgr["License Manager"]
    end

    Browser --> Main
    REST --> Main
    WebUI --> Main
    
    Main -->|"calls run()"| BMCWebLib
    Main -->|"registers"| BMCGroup
    Main -->|"uses"| IBMApp
    
    BMCGroup -->|"extends"| RedfishCore
    IBMApp -->|"configures"| BMCWebLib
    
    BMCWebLib --> WebServer
    BMCWebLib --> RedfishCore
    
    WebServer --> HTTP
    WebServer --> WS
    WebServer --> Auth
    WebServer --> Session
    
    RedfishCore --> Account
    RedfishCore --> System
    RedfishCore --> Event
    RedfishCore --> Update
    RedfishCore --> License
    
    BMCWebLib -->|"uses"| DBus
    BMCWebLib -->|"uses"| SSL
    BMCWebLib -->|"uses"| SystemD
    BMCWebLib -->|"uses"| XML
    BMCWebLib -->|"uses"| Zlib
    BMCWebLib -->|"uses"| JSON
    Auth -->|"uses"| PAM
    BMCWebLib -->|"uses"| Zstd
    
    BMCGroup -->|"uses"| DBus
    
    DBus -->|"IPC"| InventoryMgr
    DBus -->|"IPC"| NetworkMgr
    DBus -->|"IPC"| UserMgr
    DBus -->|"IPC"| LogMgr
    DBus -->|"IPC"| UpdateMgr
    DBus -->|"IPC"| LicenseMgr

    classDef clientStyle fill:#e1f5ff,stroke:#01579b,stroke-width:2px
    classDef appStyle fill:#fff3e0,stroke:#e65100,stroke-width:2px
    classDef ibmExtStyle fill:#b3e5fc,stroke:#0277bd,stroke-width:3px
    classDef libStyle fill:#f3e5f5,stroke:#4a148c,stroke-width:2px
    classDef depStyle fill:#e8f5e9,stroke:#1b5e20,stroke-width:2px
    classDef serviceStyle fill:#fce4ec,stroke:#880e4f,stroke-width:2px
    classDef ibmExtBgStyle fill:#00F33F,stroke:#0277bd,stroke-width:3px
    classDef bmcwebBgStyle fill:#f3e5f5,stroke:#4a148c,stroke-width:2px
    
    class Browser,REST,WebUI clientStyle
    class Main appStyle
    class BMCGroup,IBMApp ibmExtStyle
    class BMCWebLib,WebServer,RedfishCore,HTTP,WS,Auth,Session,Account,System,Event,Update,License libStyle
    class DBus,SSL,SystemD,XML,Zlib,JSON,PAM,Zstd depStyle
    class InventoryMgr,NetworkMgr,UserMgr,LogMgr,UpdateMgr,LicenseMgr serviceStyle
    class IBMExtensions ibmExtBgStyle
    class BMCWebSub bmcwebBgStyle
```

### Component Description

#### IBM BMCWeb Application Layer
- **webserver_main.cpp**: Entry point that initializes and starts the BMCWeb server
- **IBM Extensions (Outside bmcweblib)**:
  - **redfish/bmc_group.hpp**: BMC Group Management - Custom Redfish endpoint implementation for managing BMC groups, implemented outside the bmcweblib to allow IBM-specific customizations
  - **ibm_app.hpp**: IBM-specific application logic and configuration

#### BMCWeb Subproject
- **bmcweblib**: Core web server implementation using Boost.Beast for HTTP/HTTPS
- **Web Server Core**: HTTP/HTTPS server, WebSocket support, authentication, and session management
- **Redfish Core**: Standard Redfish API endpoints for BMC management (Account, System, Event, Update, License services)

#### System Integration
- **D-Bus (sdbusplus)**: Inter-process communication with BMC services
- **OpenSSL**: Provides TLS/SSL encryption for HTTPS connections
- **systemd**: System logging and service management integration
- **Client Applications**: Web browsers, REST clients, and the WebUI-Vue frontend

### Data Flow

1. Client sends HTTPS request to BMCWeb server
2. BMCWeb authenticates request using PAM
3. Request is routed to appropriate Redfish endpoint
4. Redfish handler communicates with BMC services via D-Bus
5. Response is formatted as JSON and returned to client

## Project Structure

```
ibm-bmcweb/
├── meson.build                    # Main build configuration
├── redfish/                       # Redfish extensions
│   └── bmc_group.hpp             # BMC group definitions
├── service/                       # Systemd service files
│   └── ibm-bmweb.service.in      # Service template
├── src/                           # Source files
│   ├── boost_asio.cpp            # Boost ASIO implementation
│   ├── ibm_app.hpp               # IBM application header
│   ├── ibmwebserver_run.cpp      # IBM webserver implementation
│   ├── ibmwebserver_run.hpp      # IBM webserver header
│   └── webserver_main.cpp        # Main entry point
└── subprojects/                   # Meson subproject dependencies
    ├── bmcweb.wrap               # BMCWeb subproject (ibm-openbmc/bmcweb)
    └── boost                      # Boost subproject
```

## Prerequisites

- Meson >= 1.3.0
- C++ compiler with C++23 support (GCC 14.2+ or Clang 17+)
- Required system libraries:
  - openssl
  - libzstd
  - sdbusplus
  - systemd
  - tinyxml2
  - zlib
  - nlohmann_json
  - pam

## Build Instructions

### Initial Setup

```bash
cd /mnt/hgfs/work/work/ibm/sources/ibm-bmcweb

meson setup builddir \
  -Dbmcweb:werror=false \
  -Dbmcweb:tests=disabled \
  -Dbmcweb:audit-events=disabled \
  -Dbmcweb:hypervisor-serial-socket=disabled \
  -Dbmcweb:bmc-shell-socket=disabled
```

### Build

```bash
meson compile -C builddir ibm-bmcweb
```

### Clean Build (if needed)

```bash
rm -rf builddir
meson setup builddir \
  -Dbmcweb:werror=false \
  -Dbmcweb:tests=disabled \
  -Dbmcweb:audit-events=disabled \
  -Dbmcweb:hypervisor-serial-socket=disabled \
  -Dbmcweb:bmc-shell-socket=disabled
meson compile -C builddir ibm-bmcweb
```

### Remote Build (via SSH)

```bash
ssh myubuntu "cd /mnt/hgfs/work/work/ibm/sources/ibm-bmcweb && \
  meson setup builddir \
    -Dbmcweb:werror=false \
    -Dbmcweb:tests=disabled \
    -Dbmcweb:audit-events=disabled \
    -Dbmcweb:hypervisor-serial-socket=disabled \
    -Dbmcweb:bmc-shell-socket=disabled && \
  meson compile -C builddir ibm-bmcweb"
```

## Build Configuration Options

| Option | Value | Description |
|--------|-------|-------------|
| `bmcweb:werror` | `false` | Disables treating warnings as errors |
| `bmcweb:tests` | `disabled` | Skips building tests |
| `bmcweb:audit-events` | `disabled` | Disables audit events feature (requires audit library) |
| `bmcweb:hypervisor-serial-socket` | `disabled` | Disables hypervisor serial socket (depends on audit-events) |
| `bmcweb:bmc-shell-socket` | `disabled` | Disables BMC shell socket (depends on audit-events) |

## Build Output

- **Executable**: `builddir/ibm-bmcweb`
- **Size**: ~397MB (debug build with symbols)
- **Dependencies**: Links with bmcweblib static library from the bmcweb subproject

## Reconfigure Options

To change build options after initial setup:

```bash
meson configure builddir -Dbmcweb:option=value
meson compile -C builddir ibm-bmcweb
```

## Troubleshooting

### Missing Dependencies

If you encounter missing dependency errors, ensure all required system libraries are installed:

```bash
# Ubuntu/Debian
sudo apt-get install libssl-dev libzstd-dev libsystemd-dev \
  libtinyxml2-dev zlib1g-dev nlohmann-json3-dev libpam0g-dev

# For sdbusplus, you may need to build from source or install from OpenBMC repositories
```

### Compiler Warnings

The build may show warnings from GCC 14.2's strict static analysis (null-pointer dereference warnings in boost and bmcweb code). These are false positives and can be safely ignored as we've disabled `-Werror`.

### Subproject Updates

To update the bmcweb subproject to the latest version:

```bash
cd subprojects
rm -rf bmcweb
cd ..
meson setup --wipe builddir
```

## Notes

- The project uses bmcweb as a header-only library with a static library (bmcweblib) for compiled sources
- The `webserver_run.cpp` in `src/` is a reference copy; the actual implementation comes from bmcweblib
- The `webserver_main.cpp` provides a simple main() function that calls the run() function from bmcweb