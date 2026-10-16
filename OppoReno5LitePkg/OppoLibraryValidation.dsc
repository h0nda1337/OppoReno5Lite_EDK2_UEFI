## @file
# Original library build description. Produces no firmware volume or boot image.
# SPDX-License-Identifier: BSD-2-Clause
##
[Defines]
  PLATFORM_NAME           = OppoLibraryValidation
  PLATFORM_GUID           = 7317dbb6-bfaa-41f6-8999-12bd15b02418
  PLATFORM_VERSION        = 0.1
  DSC_SPECIFICATION       = 0x0001001B
  OUTPUT_DIRECTORY        = .build/OppoLibraryValidation
  SUPPORTED_ARCHITECTURES = AARCH64
  BUILD_TARGETS           = DEBUG|RELEASE
  SKUID_IDENTIFIER        = DEFAULT

[LibraryClasses]
  OppoFdtLib|OppoReno5LitePkg/Library/OppoFdtLib/OppoFdtLib.inf
  OppoPlatformLib|OppoReno5LitePkg/Library/OppoPlatformLib/OppoPlatformLib.inf

[Components]
  OppoReno5LitePkg/Library/OppoPlatformLib/OppoPlatformLib.inf
  OppoReno5LitePkg/Library/OppoBgrtLib/OppoBgrtLib.inf
  OppoReno5LitePkg/Library/OppoFrameBufferLib/OppoFrameBufferLib.inf
  OppoReno5LitePkg/Library/OppoFdtLib/OppoFdtLib.inf
  OppoReno5LitePkg/Library/OppoHandoffLib/OppoHandoffLib.inf
  OppoReno5LitePkg/Library/OppoMemoryPlanLib/OppoMemoryPlanLib.inf
