# VR-Forces C2SIM Interfaces

* (Legacy) Interface to VRF v4.9
    * [Binaries](Install-C2SIM-VRF47)
    * [Source](c2simVRF47interface)
* Interface to VRF 5.0.2 - patched v2.33
    This version is derived from the main (OpenC2SIM repo'sv2.33)[https://github.com/OpenC2SIM/https---github.com-OpenC2SIM-MSG201CWIX/blob/main/c2simVRFinterfacev2.33.zip], patched to work with the latest Apache ActiveMQ versions (5.18.x, 6.x); it also removes a 10 character limitation for entity names that breaks STP's reoundtrip loading of TaskOrgs expressed in C2SIM - most of the entities have names longer than 10 characters.
    NOTE: just the HLA version has been patched, as this is the one that has been activelly developed and tested during recent CWIX events.
    * [Binaries](Install-C2SIM-VRFv2.33_patched)
    * [Source](c2simVRFinterfacev2.33-patched)
