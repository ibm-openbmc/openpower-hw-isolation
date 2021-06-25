# OpenPOWER-Hardware-Isolation
In OpenPOWER based system, a user or application can be isolate hardware and the respective
isolated hardware will be ignored to init during the next boot of the host.

**Note:** 
- System must be a power-off state when a user want to isolate hardware.
- The isolated hardware details will be consider only in the next boot of the host to isolate.
