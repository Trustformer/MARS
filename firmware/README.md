# MARS firmware

The emulator's state machine (`../emulator/c/mars.c`) as bare-metal firmware, with
mbedTLS (`../third_party/mbedtls`, v3.6.7) in place of OpenSSL.

Changes to the upstream emulator, all compiled in only on request:

- `mars.c`: `MARS_BAREMETAL` (byte swap without `<arpa/inet.h>`), `MARS_QUIET`
  (compiles out the debug prints, which print PS, DP and every derived key), and
  `MARS_PROFILE_PRD` (production key labels).
- `hw_sha2.h`: `PROFILE_COUNT_PCR` can be set from the build.
- `hw_sha2_mbedtls.{c,h}` (new): `hw_sha2.{c,h}` with each OpenSSL call replaced by
  its mbedTLS counterpart.

`make host-check` compares the OpenSSL and mbedTLS builds call for call over every
`MARS_*` entry point (derived keys included) and against `test/stage3.expected`.
`make spike-check` runs the bare-metal builds on spike against the host. Completing
a hash sequence that is not open is unspecified in the emulator and depends on the
crypto backend; `test/mars_diff.c` prints those calls outside the compared stream.
