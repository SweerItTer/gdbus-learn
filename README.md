# dbus_exercise

Two-project `gdbus` sample with a shared common layer:

- `Common/`: public interfaces and RAII utilities
- `ServiceProject/`: service-side implementation
- `ClientProject/`: client-side implementation

The current MVP keeps one method:

- bus name: `com.example.Training`
- object path: `/com/example/Training`
- method: `Ping(string) -> string`
