### Isolate the hardware from the next system boot through Redfish

#### To manually guard the Core and DIMM through Redfish:

- Core:

```
PATCH https://${bmc}/redfish/v1/Systems/system/Processors/dcmN-cpuN/SubProcessors/coreN -d '{"Enabled" : false }'
```

- DIMM:

```
PATCH https://${bmc}/redfish/v1/Systems/system/Memory/dimmN -d '{"Enabled" : false }'
```

#### To manually un-guard the guarded Core and DIMM through Redfish:

- Core:

```
PATCH https://${bmc}/redfish/v1/Systems/system/Processors/dcmN-cpuN/SubProcessors/coreN -d '{"Enabled" : true }'
```

- DIMM:

```
PATCH https://${bmc}/redfish/v1/Systems/system/Memory/dimmN -d '{"Enabled" : true }'
```

**N** - Instance number of the resource

#### To list all guard records through Redfish:

```
GET https://${bmc}/redfish/v1/Systems/system/LogServices/HardwareIsolation/Entries
```

#### To list single guard record through Redfish:

```
GET https://${bmc}/redfish/v1/Systems/system/LogServices/HardwareIsolation/Entries/N
```

**N** - Guard record entry id

#### To invalidate(mark as resolved) all guard records through Redfish:

```
POST https://${bmc}/redfish/v1/Systems/system/LogServices/HardwareIsolation/Actions/LogService.ClearLog
```

#### To invalidate single guard record through Redfish:

```
DELETE https://${bmc}/redfish/v1/Systems/system/LogServices/HardwareIsolation/Entries/N
```

**N** - Guard record entry id

#### Isolated hardware record details

```
GET https://${bmc}/redfish/v1/Systems/system/LogServices/HardwareIsolation/Entries/5

{
  "@odata.id": "/redfish/v1/Systems/system/LogServices/HardwareIsolation/Entries/5”,            <-- Record entry path
  "@odata.type": "#LogEntry.v1_9_0.LogEntry",
  "AdditionalDataURI": "/redfish/v1/Systems/system/LogServices/EventLog/Entries/1/attachment",  <-- BMC Error log
  "Created": "2021-09-09T11:53:56+00:00",                                                       <-- Isolated time
  "EntryType": "Event",
  "Id": “5”,                                                                                    <-- Record Entry id
  "Links": {
    "OriginOfCondition": {
      "@odata.id": "/redfish/v1/Systems/system/Processors/dcm0-cpu0"                            <-- The isolated FRU/Subunit FRU parent URI
    }
  },
  "Message": "Memory Controller",                                                               <-- Name of the isolated hardware
  "Name": "Hardware Isolation Entry",
  "Resolved": false,                                                                            <-- The isolated hardware is resolved or not
  "Severity": "Critical"                                                                        <-- The isolated hardware severity
}
```
**FYI:** In the OpenPOWER based IBM system, if the `AdditionalDataURI` property is not present and `Severity` is __Warning__ then,
the resource is isolated manually by the user.
