# RPS-HLSLC Compiler Command Line Reference

## General usage:
```bash
rps-hlslc.exe [options] <input .rpsl file>
```

## Options:

| Option             | Description                                                                                                                     | Default Value |
|--------------------|---------------------------------------------------------------------------------------------------------------------------------|---------------|
| -m                 | Override output module name. By default, the input file name excluding extension is used.                                       | ""            |
| -od                | Override output directory.                                                                                                      | "."           |
| -td                | Override intermediate directory for temporary files.                                                                            | "."           |
| -cbe               | Output C file using LLVM C backend.                                                                                             | true          |
| -obj               | Output obj file compiled with llc.                                                                                              | false         |
| -asm               | Output asm file compiled with llc.                                                                                              | false         |
| -dxil_bin          | Dump original DXIL blob (DXC output).                                                                                           | false         |
| -dxil              | Dump original DXIL text (DXC output).                                                                                           | false         |
| -rps-bc            | Dump processed LLVM Bitcode blob (Stripped DXILProgramHeader). This can be loaded from newer versions of LLVM and used for JIT. | false         |
| -Zi                | Enable debug info.                                                                                                              | true          |
| -O                 | Optimization Level (0-3).                                                                                                       | 3             |
| -rpsll             | Dump processed LLVM IL text (RPS transform pass output).                                                                        | false         |
| -dot-cfg           | Dump CFG. Mainly for debug purposes.                                                                                            | false         |
| -ast-dmp           | Dump AST. Mainly for debug purposes.                                                                                            | false         |
| -rps-dump-metadata | Dump RPS Metadata. Mainly for debug purposes.                                                                                   | false         |
| -rps-target-dll    | Target DLL. Use when intend to compile the RPSL module as a standalone DLL.                                                     | false         |

