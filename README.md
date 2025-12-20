# Custom MiSTer Frontend

A custom user experience for [MiSTer FPGA](https://github.com/MiSTer-devel/Main_MiSTer), featuring a modernized interface with multiple view modes and enhanced navigation.

## Features

- **Grid View** - Browse cores and games in a visual grid layout
- **Wheel View** - Navigate with a carousel-style interface
- **NFC/Zaparoo Support** - Integrated NFC card scanning with visual feedback
- **Modern UI** - Refreshed visual design while maintaining MiSTer compatibility

## UI Preview

### Main Interface
![Main Preview](ui_preview/preview.png)

### Grid View
![Grid View](ui_preview/preview.png_Grid.png)

### Wheel View
![Wheel View](ui_preview/preview.png_Wheel.png)

### NFC States
| Idle | Scanning | Card Detected | Disconnected |
|------|----------|---------------|--------------|
| ![NFC Idle](ui_preview/preview_nfc_Idle.png) | ![NFC Scanning](ui_preview/preview_nfc_Scanning.png) | ![NFC Card Detected](ui_preview/preview_nfc_CardDetected.png) | ![NFC Disconnected](ui_preview/preview_nfc_Disconnected.png) |

### Zaparoo Overlay
![Zaparoo Overlay](ui_preview/preview_zaparoo_overlay.png)

## Building

To compile this application, follow the [MiSTer cross-compiling guide](https://mister-devel.github.io/MkDocs_MiSTer/developer/mistercompile/#general-prerequisites-for-arm-cross-compiling).

## Credits

Based on the official [Main_MiSTer](https://github.com/MiSTer-devel/Main_MiSTer) project. For MiSTer documentation and wiki, see [here](https://github.com/MiSTer-devel/Wiki_MiSTer/wiki).
