# Manannan

Manannan is a small Linux and macOS service which periodically reconciles a value from a
runtime-loaded source plugin with a runtime-loaded registry plugin. Its initial
plugins discover the public IPv4 address through Amazon Check IP and maintain an
AWS Route 53 `A` record.

The service scans configured directories for `.so` files. Every plugin exports
one versioned descriptor containing its name, kind, factory functions, and YAML
root. Manannan resolves that dotted root against the loaded YAML document and
passes the resulting `YAML::Node` to the plugin unchanged. Plugin configuration
therefore does not leak into the service core.

## Dependencies

- A C++20 compiler and CMake 3.20+
- yaml-cpp 0.9+
- libcurl
- OpenSSL libcrypto

On Debian or Ubuntu:

```sh
sudo apt install cmake g++ libyaml-cpp-dev libcurl4-openssl-dev libssl-dev
```

## Build and install

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
sudo cmake --install build
```

Copy `config/manannan.yaml`, set its hosted zone and record, and supply a
least-privilege IAM access key using `AWS_ACCESS_KEY_ID` and
`AWS_SECRET_ACCESS_KEY`. Temporary credentials may also use `AWS_SESSION_TOKEN`.
The IAM identity needs `route53:ListResourceRecordSets` and
`route53:ChangeResourceRecordSets` for the hosted zone.

Set `manannan.log_level` to `debug`, `info`, `warn`, or `error`. The
`LOG_LEVEL` environment variable accepts the same values and overrides the
configuration file. At `info`, Manannan reports plugin loads and successful
address changes; routine checks and plugin operations are logged at `debug`.

The core blocks in `pselect(2)` until the next monotonic deadline. There is no
polling sleep, and `SIGINT` and `SIGTERM` wake it immediately on either Linux or
macOS.

## Plugin contract

Include `manannan/plugin.h`, implement `SourcePlugin` or `RegistryPlugin`, and
export `manannan_plugin_descriptor`. Keep the plugin loaded for the entire
lifetime of every object it creates. The current ABI uses C++ types and therefore
requires plugins to use an ABI-compatible compiler, standard library, and
yaml-cpp build. The exported discovery symbol itself has C linkage.

The supplied logger classes have been retained and moved to the
`manannan::loggers` namespace.
