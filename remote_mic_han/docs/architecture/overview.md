# Architecture Overview

```text
UI / CLI
   ↓
Application Coordinator
   ↓
Domain Interfaces
   ↑
Windows Infrastructure
```

The current framework establishes only the coordinator, runtime paths, logger, and initial interfaces. BLE, protocol, audio decoding, audio routing, HID, and action dispatch are added behind interfaces after offline fixtures exist.

Dependencies point inward. UI does not own transport or audio objects. Infrastructure does not call UI directly. The future audio path must use bounded buffers and a defined stop/drain policy.

