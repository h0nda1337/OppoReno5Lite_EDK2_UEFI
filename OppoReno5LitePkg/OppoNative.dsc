## @file
# Original experimental platform composition. Generic UEFI/ARM services are EDK2 core.
# SPDX-License-Identifier: BSD-2-Clause
##
[Defines]
  PLATFORM_NAME = OppoNative
  PLATFORM_GUID = 54154669-3897-4fc7-9918-df552715fd43
  PLATFORM_VERSION = 0.1
  DSC_SPECIFICATION = 0x0001001B
  OUTPUT_DIRECTORY = .build/OppoNative
  SUPPORTED_ARCHITECTURES = AARCH64
  BUILD_TARGETS = RELEASE
  SKUID_IDENTIFIER = DEFAULT
  FLASH_DEFINITION = OppoReno5LitePkg/OppoNative.fdf
  DEFINE OPPO_ENTRY_PROBE = FALSE
  DEFINE OPPO_MMU_PROBE = FALSE
  DEFINE OPPO_SD_READ_LAB = FALSE
  DEFINE OPPO_SD_BOOT_LAB = FALSE
  DEFINE OPPO_SD_POWER_LAB = FALSE
  DEFINE OPPO_EXTRA_MEMORY_LAB = FALSE
  DEFINE OPPO_SD_FAST_READ_LAB = FALSE
  DEFINE OPPO_ACPI_PROFILE_LAB = FALSE
  DEFINE OPPO_ACPI_INSTALL_LAB = FALSE
  DEFINE OPPO_CPU_PROFILE_LAB = FALSE

!include MdePkg/MdeLibs.dsc.inc

[BuildOptions]
  GCC:*_*_*_CC_FLAGS = -fno-jump-tables
  GCC:*_*_*_DLINK_FLAGS = -z common-page-size=0x1000

[BuildOptions.common.EDKII.DXE_RUNTIME_DRIVER]
  GCC:*_*_*_DLINK_FLAGS = -z common-page-size=0x10000

[LibraryClasses]
  BaseLib|MdePkg/Library/BaseLib/BaseLib.inf
  BaseMemoryLib|MdePkg/Library/BaseMemoryLib/BaseMemoryLib.inf
  ArmLib|MdePkg/Library/ArmLib/ArmBaseLib.inf
  ArmMmuLib|UefiCpuPkg/Library/ArmMmuLib/ArmMmuBaseLib.inf
  ArmSmcLib|MdePkg/Library/ArmSmcLib/ArmSmcLib.inf
  ArmGenericTimerCounterLib|ArmPkg/Library/ArmGenericTimerVirtCounterLib/ArmGenericTimerVirtCounterLib.inf
  CacheMaintenanceLib|ArmPkg/Library/ArmCacheMaintenanceLib/ArmCacheMaintenanceLib.inf
  CpuExceptionHandlerLib|ArmPkg/Library/ArmExceptionLib/ArmExceptionLib.inf
  DefaultExceptionHandlerLib|ArmPkg/Library/DefaultExceptionHandlerLib/DefaultExceptionHandlerLib.inf
  IoLib|MdePkg/Library/BaseIoLibIntrinsic/BaseIoLibIntrinsic.inf
  TimerLib|ArmPkg/Library/ArmArchTimerLib/ArmArchTimerLib.inf
  SerialPortLib|MdePkg/Library/BaseSerialPortLibNull/BaseSerialPortLibNull.inf
  DebugLib|MdePkg/Library/BaseDebugLibNull/BaseDebugLibNull.inf
  DebugPrintErrorLevelLib|MdePkg/Library/BaseDebugPrintErrorLevelLib/BaseDebugPrintErrorLevelLib.inf
  DebugAgentLib|MdeModulePkg/Library/DebugAgentLibNull/DebugAgentLibNull.inf
  PerformanceLib|MdePkg/Library/BasePerformanceLibNull/BasePerformanceLibNull.inf
  PrintLib|MdePkg/Library/BasePrintLib/BasePrintLib.inf
  PcdLib|MdePkg/Library/DxePcdLib/DxePcdLib.inf
  PeCoffLib|MdePkg/Library/BasePeCoffLib/BasePeCoffLib.inf
  PeCoffGetEntryPointLib|MdePkg/Library/BasePeCoffGetEntryPointLib/BasePeCoffGetEntryPointLib.inf
  PeCoffExtraActionLib|MdePkg/Library/BasePeCoffExtraActionLibNull/BasePeCoffExtraActionLibNull.inf
  UefiDecompressLib|MdePkg/Library/BaseUefiDecompressLib/BaseUefiDecompressLib.inf
  ExtractGuidedSectionLib|MdePkg/Library/DxeExtractGuidedSectionLib/DxeExtractGuidedSectionLib.inf
  HobLib|MdePkg/Library/DxeHobLib/DxeHobLib.inf
  MemoryAllocationLib|MdePkg/Library/UefiMemoryAllocationLib/UefiMemoryAllocationLib.inf
  UefiDriverEntryPoint|MdePkg/Library/UefiDriverEntryPoint/UefiDriverEntryPoint.inf
  UefiApplicationEntryPoint|MdePkg/Library/UefiApplicationEntryPoint/UefiApplicationEntryPoint.inf
  DxeCoreEntryPoint|MdePkg/Library/DxeCoreEntryPoint/DxeCoreEntryPoint.inf
  UefiLib|MdePkg/Library/UefiLib/UefiLib.inf
  UefiBootServicesTableLib|MdePkg/Library/UefiBootServicesTableLib/UefiBootServicesTableLib.inf
  UefiRuntimeServicesTableLib|MdePkg/Library/UefiRuntimeServicesTableLib/UefiRuntimeServicesTableLib.inf
  UefiRuntimeLib|MdePkg/Library/UefiRuntimeLib/UefiRuntimeLib.inf
  DxeServicesTableLib|MdePkg/Library/DxeServicesTableLib/DxeServicesTableLib.inf
  DxeServicesLib|MdePkg/Library/DxeServicesLib/DxeServicesLib.inf
  DevicePathLib|MdePkg/Library/UefiDevicePathLib/UefiDevicePathLib.inf
  ReportStatusCodeLib|MdePkg/Library/BaseReportStatusCodeLibNull/BaseReportStatusCodeLibNull.inf
  SecurityManagementLib|MdeModulePkg/Library/DxeSecurityManagementLib/DxeSecurityManagementLib.inf
  ImagePropertiesRecordLib|MdeModulePkg/Library/ImagePropertiesRecordLib/ImagePropertiesRecordLib.inf
  UefiHiiServicesLib|MdeModulePkg/Library/UefiHiiServicesLib/UefiHiiServicesLib.inf
  HiiLib|MdeModulePkg/Library/UefiHiiLib/UefiHiiLib.inf
  SortLib|MdeModulePkg/Library/UefiSortLib/UefiSortLib.inf
  BmpSupportLib|MdeModulePkg/Library/BaseBmpSupportLib/BaseBmpSupportLib.inf
  FrameBufferBltLib|MdeModulePkg/Library/FrameBufferBltLib/FrameBufferBltLib.inf
  FileHandleLib|MdePkg/Library/UefiFileHandleLib/UefiFileHandleLib.inf
  ShellLib|ShellPkg/Library/UefiShellLib/UefiShellLib.inf
  ShellCommandLib|ShellPkg/Library/UefiShellCommandLib/UefiShellCommandLib.inf
  HandleParsingLib|ShellPkg/Library/UefiHandleParsingLib/UefiHandleParsingLib.inf
  PciLib|MdePkg/Library/BasePciLibPciExpress/BasePciLibPciExpress.inf
  PciExpressLib|MdePkg/Library/BasePciExpressLib/BasePciExpressLib.inf
  AuthVariableLib|MdeModulePkg/Library/AuthVariableLibNull/AuthVariableLibNull.inf
  VarCheckLib|MdeModulePkg/Library/VarCheckLib/VarCheckLib.inf
  VariableFlashInfoLib|MdeModulePkg/Library/BaseVariableFlashInfoLib/BaseVariableFlashInfoLib.inf
  VariablePolicyLib|MdeModulePkg/Library/VariablePolicyLib/VariablePolicyLibRuntimeDxe.inf
  VariablePolicyHelperLib|MdeModulePkg/Library/VariablePolicyHelperLib/VariablePolicyHelperLib.inf
  TpmMeasurementLib|MdeModulePkg/Library/TpmMeasurementLibNull/TpmMeasurementLibNull.inf
  OppoFdtLib|OppoReno5LitePkg/Library/OppoFdtLib/OppoFdtLib.inf
  OppoAcpiProbeLib|OppoReno5LitePkg/Library/OppoAcpiProbeLib/OppoAcpiProbeLib.inf
  OppoAcpiLib|OppoReno5LitePkg/Library/OppoAcpiLib/OppoAcpiLib.inf
  OppoAcpiInstallLib|OppoReno5LitePkg/Library/OppoAcpiInstallLib/OppoAcpiInstallLib.inf
  OppoCpuProbeLib|OppoReno5LitePkg/Library/OppoCpuProbeLib/OppoCpuProbeLib.inf
  OppoPlatformLib|OppoReno5LitePkg/Library/OppoPlatformLib/OppoPlatformLib.inf
  OppoHandoffLib|OppoReno5LitePkg/Library/OppoHandoffLib/OppoHandoffLib.inf
  OppoFrameBufferLib|OppoReno5LitePkg/Library/OppoFrameBufferLib/OppoFrameBufferLib.inf
  OppoMemoryPlanLib|OppoReno5LitePkg/Library/OppoMemoryPlanLib/OppoMemoryPlanLib.inf
  OppoMsdcLib|OppoReno5LitePkg/Library/OppoMsdcLib/OppoMsdcLib.inf
  OppoSdPowerLib|OppoReno5LitePkg/Library/OppoSdPowerLib/OppoSdPowerLib.inf
  OppoBootLogLib|OppoReno5LitePkg/Library/OppoDebugLib/OppoBootLogLib.inf

[LibraryClasses.common.SEC]
  DebugLib|OppoReno5LitePkg/Library/OppoDebugLib/OppoDebugLib.inf
  PcdLib|MdePkg/Library/BasePcdLibNull/BasePcdLibNull.inf
  HobLib|EmbeddedPkg/Library/PrePiHobLib/PrePiHobLib.inf
  MemoryAllocationLib|EmbeddedPkg/Library/PrePiMemoryAllocationLib/PrePiMemoryAllocationLib.inf
  PrePiLib|EmbeddedPkg/Library/PrePiLib/PrePiLib.inf
  PrePiHobListPointerLib|ArmPlatformPkg/Library/PrePiHobListPointerLib/PrePiHobListPointerLib.inf
  ExtractGuidedSectionLib|EmbeddedPkg/Library/PrePiExtractGuidedSectionLib/PrePiExtractGuidedSectionLib.inf

[LibraryClasses.common.DXE_CORE]
  DebugLib|OppoReno5LitePkg/Library/OppoDebugLib/OppoDebugCoreLib.inf
  HobLib|MdePkg/Library/DxeCoreHobLib/DxeCoreHobLib.inf
  MemoryAllocationLib|MdeModulePkg/Library/DxeCoreMemoryAllocationLib/DxeCoreMemoryAllocationLib.inf

[PcdsFeatureFlag]
  gOppoNativeTokenSpaceGuid.PcdNativeEntryProbe|$(OPPO_ENTRY_PROBE)
  gOppoNativeTokenSpaceGuid.PcdNativeMmuProbe|$(OPPO_MMU_PROBE)
  gOppoNativeTokenSpaceGuid.PcdNativeSdReadLab|$(OPPO_SD_READ_LAB)
  gOppoNativeTokenSpaceGuid.PcdNativeSdBootLab|$(OPPO_SD_BOOT_LAB)
  gOppoNativeTokenSpaceGuid.PcdNativeSdPowerLab|$(OPPO_SD_POWER_LAB)
  gOppoNativeTokenSpaceGuid.PcdNativeExtraMemoryLab|$(OPPO_EXTRA_MEMORY_LAB)
  gOppoNativeTokenSpaceGuid.PcdNativeSdFastReadLab|$(OPPO_SD_FAST_READ_LAB)
  gOppoNativeTokenSpaceGuid.PcdNativeAcpiProfileLab|$(OPPO_ACPI_PROFILE_LAB)
  gOppoNativeTokenSpaceGuid.PcdNativeAcpiInstallLab|$(OPPO_ACPI_INSTALL_LAB)
  gOppoNativeTokenSpaceGuid.PcdNativeCpuProfileLab|$(OPPO_CPU_PROFILE_LAB)
  gEmbeddedTokenSpaceGuid.PcdPrePiProduceMemoryTypeInformationHob|FALSE
  gArmTokenSpaceGuid.PcdRemapUnusedMemoryNx|FALSE
  gEfiMdeModulePkgTokenSpaceGuid.PcdHiiOsRuntimeSupport|FALSE
  gEfiShellPkgTokenSpaceGuid.PcdShellRequireHiiPlatform|TRUE

[PcdsFixedAtBuild]
  gArmTokenSpaceGuid.PcdFdBaseAddress|0x40080000
  gArmTokenSpaceGuid.PcdFdSize|0x800000
  gArmTokenSpaceGuid.PcdFvBaseAddress|0x40081000
  gArmTokenSpaceGuid.PcdFvSize|0x7ff000
  gArmTokenSpaceGuid.PcdSystemMemoryBase|0x43000000
  gArmTokenSpaceGuid.PcdSystemMemorySize|0x4000000
  gArmTokenSpaceGuid.PcdGicDistributorBase|0x0c000000
  gArmTokenSpaceGuid.PcdGicRedistributorsBase|0x0c040000
  gArmTokenSpaceGuid.PcdGicInterruptInterfaceBase|0
  gArmTokenSpaceGuid.PcdMonitorConduitHvc|FALSE
  gEfiMdeModulePkgTokenSpaceGuid.PcdEmuVariableNvModeEnable|TRUE
  gEfiMdeModulePkgTokenSpaceGuid.PcdVariableStoreSize|0x10000
  gEfiMdeModulePkgTokenSpaceGuid.PcdFlashNvStorageVariableSize|0x10000
  gEfiMdeModulePkgTokenSpaceGuid.PcdFirmwareVersionString|L"OPPO native experimental 0.1"
  gEfiShellPkgTokenSpaceGuid.PcdShellSupportLevel|3
  gEfiShellPkgTokenSpaceGuid.PcdShellPageBreakDefault|FALSE

[PcdsPatchableInModule]
  gEfiMdeModulePkgTokenSpaceGuid.PcdConOutColumn|0
  gEfiMdeModulePkgTokenSpaceGuid.PcdConOutRow|0

[Components]
  OppoReno5LitePkg/Sec/OppoSec.inf
  MdeModulePkg/Core/Dxe/DxeMain.inf
  MdeModulePkg/Universal/PCD/Dxe/Pcd.inf {
    <LibraryClasses>
      # The protocol producer must not depend on its own PCD protocol.
      PcdLib|MdePkg/Library/BasePcdLibNull/BasePcdLibNull.inf
  }
  ArmPkg/Drivers/CpuDxe/CpuDxe.inf
  ArmPkg/Drivers/ArmGicDxe/ArmGicDxe.inf
  ArmPkg/Drivers/TimerDxe/TimerDxe.inf
  MdeModulePkg/Core/RuntimeDxe/RuntimeDxe.inf
  MdeModulePkg/Universal/SecurityStubDxe/SecurityStubDxe.inf
  MdeModulePkg/Universal/WatchdogTimerDxe/WatchdogTimer.inf
  EmbeddedPkg/MetronomeDxe/MetronomeDxe.inf
  EmbeddedPkg/EmbeddedMonotonicCounter/EmbeddedMonotonicCounter.inf
  MdeModulePkg/Universal/Variable/RuntimeDxe/VariableRuntimeDxe.inf
  MdeModulePkg/Universal/DevicePathDxe/DevicePathDxe.inf
  MdeModulePkg/Universal/Console/ConSplitterDxe/ConSplitterDxe.inf
  MdeModulePkg/Universal/Console/GraphicsConsoleDxe/GraphicsConsoleDxe.inf
  MdeModulePkg/Universal/HiiDatabaseDxe/HiiDatabaseDxe.inf
  MdeModulePkg/Universal/Disk/UnicodeCollation/EnglishDxe/EnglishDxe.inf
  MdeModulePkg/Universal/FvSimpleFileSystemDxe/FvSimpleFileSystemDxe.inf
  OppoReno5LitePkg/Drivers/OppoRuntimeDxe/OppoRuntimeDxe.inf
  OppoReno5LitePkg/Drivers/OppoGopDxe/OppoGopDxe.inf
!if $(OPPO_SD_READ_LAB) == TRUE
  OppoReno5LitePkg/Drivers/OppoSdDxe/OppoSdDxe.inf {
    <LibraryClasses>
      DebugLib|OppoReno5LitePkg/Library/OppoDebugLib/OppoDebugLib.inf
  }
  MdeModulePkg/Universal/Disk/DiskIoDxe/DiskIoDxe.inf
  MdeModulePkg/Universal/Disk/PartitionDxe/PartitionDxe.inf
  FatPkg/EnhancedFatDxe/Fat.inf
!endif
  OppoReno5LitePkg/Drivers/OppoBdsDxe/OppoBdsDxe.inf {
    <LibraryClasses>
      DebugLib|OppoReno5LitePkg/Library/OppoDebugLib/OppoDebugLib.inf
  }
  ShellPkg/Application/Shell/Shell.inf {
    <LibraryClasses>
      DebugLib|OppoReno5LitePkg/Library/OppoDebugLib/OppoDebugLib.inf
      NULL|ShellPkg/Library/UefiShellLevel1CommandsLib/UefiShellLevel1CommandsLib.inf
      NULL|ShellPkg/Library/UefiShellLevel2CommandsLib/UefiShellLevel2CommandsLib.inf
      NULL|ShellPkg/Library/UefiShellLevel3CommandsLib/UefiShellLevel3CommandsLib.inf
      NULL|ShellPkg/Library/UefiShellDriver1CommandsLib/UefiShellDriver1CommandsLib.inf
    <PcdsFixedAtBuild>
      # Shell installs its protocols in UefiMain, after library constructors.
      # Its explicit ShellInitialize must bind them after installation.
      gEfiShellPkgTokenSpaceGuid.PcdShellLibAutoInitialize|FALSE
  }
