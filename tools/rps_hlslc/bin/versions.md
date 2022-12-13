# Compiler Tools Version History

## RPS-HLSLC

### 0.1.1.1218
- Fix linker error when node def is missing queue familiy specifier.
- Cubemap support, added resource_flags::cubemap_compatible.
- Null resource / view handle support. Eg. resource r = null; if (is_null(r)) { foo(); }

### 0.1.1.1208
- Fix compiler error messages not displayed when building from Visual Studio.

### 0.1.0.0
- Initial version
