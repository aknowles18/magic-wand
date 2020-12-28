# Reverse Engineering Philips Hue Bluetooth Lightbulb

### Setup & Investigation Steps
1. Buy the Lightbulbs with Bluetooth
2. Download Lightbulb or another similar App
3. Determine which characteristics and services handle on/off and dimness.
 ### Service: `b8843add-0000-4aa1-8794-c3f462030bda`             
 #### Purpose: Unknown, ble_firmware_update?
 | Characteristic | Descritpion |
 | --- | --- |
 | b8843add-0004-4aa1-8794-c3f462030bda | |
 | b8843add-0003-4aa1-8794-c3f462030bda | |
 | b8843add-0002-4aa1-8794-c3f462030bda | |
 | b8843add-0001-4aa1-8794-c3f462030bda | |

---

 ### Service: `932c32bd-0000-47a2-835a-a8d455b859dd`
 #### Purpose: light_control - startupConfiguration, combinedControl
 | Characteristic | Descritpion |
 | --- | --- |
 | 932c32bd-0001-47a2-835a-a8d455b859dd | unk, 00 01 03 01 02 99 00 02 02 F4 01 03 02 01 00 |
 | 932c32bd-0002-47a2-835a-a8d455b859dd | Light state, 01/00 |
 | 932c32bd-0003-47a2-835a-a8d455b859dd | Brightness |
 | 932c32bd-0004-47a2-835a-a8d455b859dd | unk, E9 00 |
 | 932c32bd-0005-47a2-835a-a8d455b859dd | Color |
 | 932c32bd-0006-47a2-835a-a8d455b859dd | unk, write-only |
 | 932c32bd-0007-47a2-835a-a8d455b859dd | everything, includes color and brightness combined_light_control_port |
 | 932c32bd-1005-47a2-835a-a8d455b859dd | Also everything, but last four bytes FF FF FF FF - default powerloss state? combined_light_control_port_factory? |

---

 ### Service: `0000fe0f-0000-1000-8000-00805f9b34fb`
 #### Purpose: device_configuration_service_info - ProximityPairingSetup somewhere
 | Characteristic | Descritpion |
 | --- | --- |
 | 97fe6561-0001-4f62-86e9-b71ee2da3d22 | zigbee address, 64 e2 7a 08 01 88 17 00 |
 | 97fe6561-0003-4f62-86e9-b71ee2da3d22 | userDefinedDeviceName, "Lamp" |
 | 97fe6561-0004-4f62-86e9-b71ee2da3d22 |  |
 | 97fe6561-0008-4f62-86e9-b71ee2da3d22 |  |
 | 97fe6561-a001-4f62-86e9-b71ee2da3d22 | write-only |
 | 97fe6561-2004-4f62-86e9-b71ee2da3d22 | write-only |
 | 97fe6561-2002-4f62-86e9-b71ee2da3d22 | write-only |
 | 97fe6561-2001-4f62-86e9-b71ee2da3d22 | unk, 0A |

---

 ### Service: `0000180a-0000-1000-8000-00805f9b34fb`
 #### Purpose: 
 | Characteristic | Descritpion |
 | --- | --- |
 | 00002a28-0000-1000-8000-00805f9b34fb | fw version |
 | 00002a24-0000-1000-8000-00805f9b34fb | model |
 | 00002a29-0000-1000-8000-00805f9b34fb | manufacturer |  

---

- My research / playing around with it
   ### 1. On/Off
   - Service: `932c32bd-0000-47a2-835a-a8d455b859dd`
   - Characteristic: `932c32bd-0002-47a2-835a-a8d455b859dd`
   - Values: 
        1. Off: `0x00`
        2. On:  `0x01`
   ### 2. Dimness
   - Service: `932c32bd-0000-47a2-835a-a8d455b859dd`
   - Characteristic: `0x932C32BD-0007-47A2-835A-A8D455B859DD`
   - Values: 
        1. Needs to be a very similar hex color value. The hex color of the lamp is: `0201CD` (bright orange-yellow). Next I chose a dim version of this color: `020102`. Next, I chose a different pink color to see if i could change colors using this characteristic. I tried `CD017D`, the color did not change.
   ### 3. Change Color
   - Service: `932c32bd-0000-47a2-835a-a8d455b859dd`
   - Characteristic: `0x932C32BD-0005-47A2-835A-A8D455B859DD`
   - Values: 
        1. 
