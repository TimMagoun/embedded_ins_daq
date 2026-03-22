# Control And Sensor Modules
## Configuration, Session Control, and Sensor Lifecycle

---

## Module Map In This Document

- `config_manager`
- `session_controller`
- `sensor_manager`
- `sensor_driver_*`
- `local_control_service`
- `fault_manager`

---

## `config_manager`

### Responsibility

- load startup configuration from SD card
- parse and validate configuration
- publish an immutable runtime configuration object

### Public Interface

- `load_config() -> ConfigLoadResult`
- `get_runtime_config() -> const RuntimeConfig*`
- `get_config_hash() -> ConfigHash`

### Contract

- returns success only if the configuration is syntactically and semantically valid
- runtime configuration is immutable after session start
- no session may start without valid configuration

### Memory Ownership

- owns parsed configuration structures
- stores configuration in internal RAM

---

## `session_controller`

### Responsibility

- manage the global lifecycle state machine
- coordinate start, stop, and fault transitions
- create unique session identity
- gate recording on sensor readiness

### Public Interface

- `request_start(source)`
- `request_stop(source)`
- `get_state() -> SessionState`
- `get_session_info() -> SessionInfo`
- `notify_fault(FaultEvent)`

### Contract

- only `session_controller` may change global session state
- start is accepted only from `READY`
- stop is idempotent
- `FAULTED` suppresses healthy-recording indicators
- transition into active recording requires readiness approval from `sensor_manager` for all required sensors

### Outbound Events

- session start event
- session stop event
- state transition notifications
- session metadata for log headers
- sensor-preparation requests to `sensor_manager`

### Memory Ownership

- owns `SessionInfo`
- stores current state and counters in internal RAM

---

## `sensor_manager`

### Responsibility

- manage pre-record sensor lifecycle for each configured port
- select the correct sensor driver for each sensor type
- enforce a shared readiness contract across heterogeneous devices
- gate session start until all required sensors are ready

### Public Interface

- `sensor_manager_init(RuntimeConfig)`
- `sensor_manager_prepare_all(SessionInfo) -> SensorPrepareResult`
- `sensor_manager_abort_prepare()`
- `sensor_manager_teardown_all()`
- `sensor_manager_get_port_state(port_id) -> SensorLifecycleState`
- `sensor_manager_get_summary() -> SensorReadinessSummary`

### Shared Lifecycle Contract

Every driver-managed sensor uses the same normalized state model:

- `UNCONFIGURED`
- `PROBING`
- `CONFIGURING`
- `WAITING_READY`
- `READY`
- `FAILED`

### Contract

- every enabled port is assigned exactly one sensor policy
- a port is either driver-managed or `raw_capture_only`
- a sensor marked `required_for_start = true` must reach `READY` before recording begins
- a sensor marked `raw_capture_only` is treated as ready without active initialization
- all initialization uses bounded retry and timeout policy from configuration

### Memory Ownership

- owns per-port sensor runtime state tables in internal RAM
- owns retry counters, timeout state, and readiness summary in internal RAM

---

## `sensor_driver_*`

### Responsibility

- implement sensor-type-specific control-plane behavior for one protocol or device family

Examples:

- `sensor_driver_gnss_ubx`
- `sensor_driver_imu_uart_ascii`
- `sensor_driver_raw_uart`

### Public Interface

- `probe(port_id, SensorProfile) -> SensorProbeResult`
- `configure(port_id, SensorProfile) -> SensorConfigureResult`
- `verify_ready(port_id, SensorProfile) -> SensorReadyResult`
- `arm_for_recording(port_id, SensorProfile) -> SensorArmResult`
- `stop(port_id)`
- `get_capabilities() -> SensorDriverCapabilities`

### Contract

- all drivers implement the same lifecycle interface
- different devices may use different command sequences and readiness criteria
- drivers operate only in the pre-record control path in revision 1
- drivers must not write directly to SD or mutate session state

### Practical Meaning

- a GNSS driver may send protocol-specific commands and wait for ACK or output-mode confirmation
- an IMU driver may send register transactions and wait for status confirmation
- both still report normalized readiness through the same interface

---

## `local_control_service`

### Responsibility

- handle button input and console commands
- translate local operator intent into session requests

### Public Interface

- `local_control_init()`
- `local_control_poll_console()`
- `local_control_handle_button_event(...)`

### Contract

- local control never bypasses `session_controller`
- console access is advisory and non-authoritative during fault conditions

### Memory Ownership

- owns small input buffers in internal RAM

---

## `fault_manager`

### Responsibility

- normalize faults from all producers
- deduplicate and classify faults
- publish fault records and visible system responses

### Public Interface

- `fault_publish(FaultEvent)`
- `fault_get_counters() -> FaultCounters`

### Contract

- every detected loss event must be represented by a fault event
- repeated identical faults may be rate-limited in the status log but not hidden from binary accounting

### Memory Ownership

- fault counters and recent-fault cache live in internal RAM

