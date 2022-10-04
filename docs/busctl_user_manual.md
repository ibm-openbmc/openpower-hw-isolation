### Isolate the hardware from the next system boot through D-Bus

#### 1. Create [Method](https://github.com/openbmc/phosphor-dbus-interfaces/blob/master/yaml/xyz/openbmc_project/HardwareIsolation/Create.interface.yaml#L7)
  - This method is used to isolate the hardware without a bmc error log.
  - This method returns the created isolated hardware entry object path.
```
busctl call org.open_power.HardwareIsolation /xyz/openbmc_project/hardware_isolation \
            xyz.openbmc_project.HardwareIsolation.Create Create os \
            <Hardware_Inventory_DBus_Object_Path> \
            <Severity>
```
- **org.open_power.HardwareIsolation**
  
  The DBus service name and that can be get by using the mapper [GetObject](https://github.com/openbmc/phosphor-dbus-interfaces/blob/master/yaml/xyz/openbmc_project/ObjectMapper.interface.yaml#L7) method with the following HardwareIsolation service D-Bus root object path [`/xyz/openbmc_project/hardware_isolation`]
  
- **/xyz/openbmc_project/hardware_isolation**

  The HardwareIsolation service D-Bus root object path
  
- **xyz.openbmc_project.HardwareIsolation.Create Create os**

  `xyz.openbmc_project.HardwareIsolation.Create` D-Bus interface name which contains `Create` method that expects `os` as inputs which are mentioned below
  
  - **<Hardware_Inventory_DBus_Object_Path>** - `o` (object)

    The hardware BMC inventory object path which is needs to isolate from the next system boot.
  
  - **\<Severity\>** - `s` (string)

    The hardware isolation severity which is defined in [xyz.openbmc_project.HardwareIsolation.Entry](https://github.com/openbmc/phosphor-dbus-interfaces/blob/master/yaml/xyz/openbmc_project/HardwareIsolation/Entry.interface.yaml#L22)
  
**E.g.:**
  
```
busctl call org.open_power.HardwareIsolation /xyz/openbmc_project/hardware_isolation \
            xyz.openbmc_project.HardwareIsolation.Create Create os \
            /xyz/openbmc_project/inventory/system/chassis/motherboard/dimm0 \   <-- BMC inventory object path
            xyz.openbmc_project.HardwareIsolation.Entry.Type.Manual             <-- Severity Type
            
o "/xyz/openbmc_project/hardware_isolation/entry/1"                             <-- Returned entry object path
```

#### 2. CreateWithErrorLog [Method](https://github.com/openbmc/phosphor-dbus-interfaces/blob/master/yaml/xyz/openbmc_project/HardwareIsolation/Create.interface.yaml#L35)
  - This method is used to isolate the hardware with a bmc error log.
  - This method returns the created isolated hardware entry object path.
```
busctl call org.open_power.HardwareIsolation /xyz/openbmc_project/hardware_isolation \
            xyz.openbmc_project.HardwareIsolation.Create CreateWithErrorLog oso \
            <Hardware_Inventory_DBus_Object_Path> \
            <Severity> \
            <BMC_Error_Log_Object_Path>
```
- **org.open_power.HardwareIsolation**
  
  The DBus service name and that can be get by using the mapper [GetObject](https://github.com/openbmc/phosphor-dbus-interfaces/blob/master/yaml/xyz/openbmc_project/ObjectMapper.interface.yaml#L7) method with the following HardwareIsolation service D-Bus root object path [`/xyz/openbmc_project/hardware_isolation`]
  
- **/xyz/openbmc_project/hardware_isolation**

  The HardwareIsolation service D-Bus root object path
  
- **xyz.openbmc_project.HardwareIsolation.Create CreateWithErrorLog oso**

  `xyz.openbmc_project.HardwareIsolation.Create` D-Bus interface name which contains `CreateWithErrorLog` method that expects `oso` as inputs which are mentioned below.
  
  - **<Hardware_Inventory_DBus_Object_Path>** - `o` (object)

    The hardware BMC inventory object path which is needs to isolate from the next system boot.
  
  - **\<Severity\>** - `s` (string)

    The hardware isolation severity which is defined in [xyz.openbmc_project.HardwareIsolation.Entry](https://github.com/openbmc/phosphor-dbus-interfaces/blob/master/yaml/xyz/openbmc_project/HardwareIsolation/Entry.interface.yaml#L22)
    
  - **<BMC_Error_Log_Object_Path>** - `o` (object)

    The BMC error log caused the isolation of hardware.
  
**E.g.:**
  
```
busctl call org.open_power.HardwareIsolation /xyz/openbmc_project/hardware_isolation \
            xyz.openbmc_project.HardwareIsolation.Create CreateWithErrorLog oso \
            /xyz/openbmc_project/inventory/system/chassis/motherboard/dimm0 \   <-- BMC inventory object path
            xyz.openbmc_project.HardwareIsolation.Entry.Type.Critical           <-- Severity Type
            /xyz/openbmc_project/logging/entry/1                                <-- BMC Error log object path
            
o "/xyz/openbmc_project/hardware_isolation/entry/1"                             <-- Returned entry object path
```

#### 3. CreateWithEntityPath [Method](https://github.com/ibm-openbmc/phosphor-dbus-interfaces/blob/1020/yaml/org/open_power/HardwareIsolation/Create.interface.yaml#L7)
  - This method is used to isolate the hardware with a bmc error log.
  - This method will be useful to isolate all the isolatable hardware which are present in the PHAL (POWER Hardware Abstraction Layer) device tree, this device tree contains the `ATTR_PHYS_BIN_PATH` (aka EntityPath) attribute that can be used to pass as the EntityPath in this method.
  - This method returns the created isolated hardware entry object path.
```
busctl call org.open_power.HardwareIsolation /xyz/openbmc_project/hardware_isolation \
            org.open_power.HardwareIsolation.Create CreateWithEntityPath ayso \
            <PHAL_DevTree_Entity_Path> \
            <Severity> \
            <BMC_Error_Log_Object_Path>
```
- **org.open_power.HardwareIsolation**
  
  The DBus service name and that can be get by using the mapper [GetObject](https://github.com/openbmc/phosphor-dbus-interfaces/blob/master/yaml/xyz/openbmc_project/ObjectMapper.interface.yaml#L7) method with the following HardwareIsolation service D-Bus root object path [`/xyz/openbmc_project/hardware_isolation`]
  
- **/xyz/openbmc_project/hardware_isolation**

  The HardwareIsolation service D-Bus root object path
  
- **org.open_power.HardwareIsolation.Create CreateWithEntityPath ayso**

  `org.open_power.HardwareIsolation.Create` D-Bus interface name which contains `CreateWithEntityPath` method that expects `ayso` as inputs which are mentioned below.
  
  - **<PHAL_DevTree_Entity_Path>** - `ay` (array of bytes)

    The hardware entity path which is needs to isolate from the next system boot.
  
  - **\<Severity\>** - `s` (string)

    The hardware isolation severity which is defined in [xyz.openbmc_project.HardwareIsolation.Entry](https://github.com/openbmc/phosphor-dbus-interfaces/blob/master/yaml/xyz/openbmc_project/HardwareIsolation/Entry.interface.yaml#L22)
    
  - **<BMC_Error_Log_Object_Path>** - `o` (object)

    The BMC error log caused the isolation of hardware.
  
**E.g.:1** Command line
  
```
busctl call org.open_power.HardwareIsolation /xyz/openbmc_project/hardware_isolation \
            org.open_power.HardwareIsolation.Create CreateWithEntityPath ayso \
            21 0x23 0x01 0x00 0x02 0x00 0x03 0x00 0x00 0x00 0x00 0x00 0x00 0x00 \
               0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 \                        <-- PHAL entity path (21B - size)
            xyz.openbmc_project.HardwareIsolation.Entry.Type.Critical           <-- Severity Type
            /xyz/openbmc_project/logging/entry/1                                <-- BMC Error log object path
            
o "/xyz/openbmc_project/hardware_isolation/entry/1"                             <-- Returned entry object path
```

**Tips**

- To get the entity path of the hardware from the PHAL device tree. 

```
attributes read <target> <attribute>

attributes read p10.dimm:k0:n0:s0:p00:c00 ATTR_PHYS_BIN_PATH

ATTR_PHYS_BIN_PATH<21> = 0x23 0x01 0x00 0x02 0x00 0x03 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
```

- For PHAL based OpenBMC application

**Note:** Feel free to write your logic by using the respective bmc dependency libraries as per needs. This is just an example.

```
#include <vector>
#include <sdbusplus/server/object.hpp>
#include "xyz/openbmc_project/HardwareIsolation/Entry/server.hpp"

extern "C" {
#include <libpdbg.h>
}

#include "attributes_info.H"

int main()
{
   try
   {
       // Make sure the right PHAL device tree is set/init before trying to init libpdbg
       
       if (!pdbg_targets_init(NULL))
       {
          // Failed to init libpdbg
       }
       
       struct pdbg_target *fruTgt;
       pdbg_for_each_class_target("dimm", fruTgt) // Use appropriate PDBG api to get the required targets
       {
           ...
           if(failed)
           {
              // Need to isolate the respective hardware
              {
                 ATTR_PHYS_BIN_PATH_Type physBinPath;
                 if (DT_GET_PROP(ATTR_PHYS_BIN_PATH, fruTgt, physBinPath))
                 {
                    // failed to read the ATTR_PHYS_BIN_PATH
                 }
                 
                 std::vector<uint8_t> hwPath(physBinPath, physBinPath + sizeof(physBinPath) / sizeof(physBinPath[0]));
                 auto type = sdbusplus::xyz::openbmc_project::HardwareIsolation::server::convertForMessage(
                        sdbusplus::xyz::openbmc_project::HardwareIsolation::server::Entry::Type::Critical);
                 
                 // Create BMC error log and use that created error log
                 std::string errLog("/xyz/openbmc_project/logging/entry/1";
                 
                 // Prepare D-Bus message and send
                 std::variant<std::vector<uint8_t>> v = hwPath;
                 method.append(v, type, errLog);
                 auto reply = bus.call(method);
                 
                 // Read D-Bus message response
                 sdbusplus::message::object_path resp;
                 reply.read(resp);
                 
                 // resp.str   <-- Returned hardware isolation entry object path
              }
           }
       }
   }
   catch (std::exception& e)
   {
       // Exception
   }
}
```
