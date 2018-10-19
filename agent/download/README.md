# Broadcom proprietary src code required to build olt-brcm

The following vendor proprietary source files are required to build. These files are available from Broadcom under NDA through a common CSP.

- `SW-BCM68620_<VER>.zip` - Broadcom BAL source and Maple SDK.
- `sdk-all-<SDK_VER>.tar.gz` - Broadcom Qumran SDK.
- `ACCTON_BAL_<BAL_VER>-<ACCTON_VER>.patch` - Accton/Edgecore's patch.
- `OPENOLT_BAL_<BAL_VER>.patch` - ONF's patch to BAL (to enable BAL to compile with C++ based openolt)

Copy the above four files to this folder before running make in the top level directory.
