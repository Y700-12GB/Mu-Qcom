/** @file

  Copyright (c) 2011-2015, ARM Limited. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiPei.h>

#include <Library/ArmMmuLib.h>
#include <Library/ArmPlatformLib.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/PlatformMemoryMapLib.h>

#define SIZE_KB ((UINTN)(1024))
#define SIZE_MB ((UINTN)(SIZE_KB * 1024))
#define SIZE_GB ((UINTN)(SIZE_MB * 1024))
#define SIZE_MB_BIG(_Size,_Value) ((_Size) > ((_Value) * SIZE_MB))
#define SIZE_MB_SMALL(_Size,_Value) ((_Size) < ((_Value) * SIZE_MB))
#define SIZE_MB_IN(_Min,_Max,_Size) \
  if (SIZE_MB_BIG((MemoryTotal), (_Min)) && SIZE_MB_SMALL((MemoryTotal), (_Max)))\
    Mem = Mem##_Size##G, MemGB = _Size

VOID
BuildMemoryTypeInformationHob (
  VOID
  );

STATIC
VOID
InitMmu (
  IN ARM_MEMORY_REGION_DESCRIPTOR  *MemoryTable
  )
{
  VOID           *TranslationTableBase;
  UINTN          TranslationTableSize;
  RETURN_STATUS  Status;

  // Note: Because we called PeiServicesInstallPeiMemory() before to call InitMmu() the MMU Page Table resides in
  //      DRAM (even at the top of DRAM as it is the first permanent memory allocation)
  Status = ArmConfigureMmu (MemoryTable, &TranslationTableBase, &TranslationTableSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Error: Failed to enable MMU\n"));
  }
}

STATIC
VOID AddHob(PARM_MEMORY_REGION_DESCRIPTOR_EX Desc)
{
  BuildResourceDescriptorHob(
      Desc->ResourceType, Desc->ResourceAttribute, Desc->Address, Desc->Length);

  if (Desc->ResourceType == EFI_RESOURCE_SYSTEM_MEMORY ||
      Desc->MemoryType == EfiRuntimeServicesData)
  {
    BuildMemoryAllocationHob(Desc->Address, Desc->Length, Desc->MemoryType);
  }
}

/*++

Routine Description:



Arguments:

  FileHandle  - Handle of the file being invoked.
  PeiServices - Describes the list of possible PEI Services.

Returns:

  Status -  EFI_SUCCESS if the boot mode could be set

--*/
EFI_STATUS
EFIAPI
MemoryPeim (
  IN EFI_PHYSICAL_ADDRESS  UefiMemoryBase,
  IN UINT64                UefiMemorySize
  )
{

  PARM_MEMORY_REGION_DESCRIPTOR_EX MemoryDescriptorEx =
      GetPlatformMemoryMap();
  ARM_MEMORY_REGION_DESCRIPTOR
        MemoryTable[MAX_ARM_MEMORY_REGION_DESCRIPTOR_COUNT];
  UINTN Index = 0;
  UINTN MemoryTotal = 0;

#if RAM_SIZE == 4
  DeviceMemoryAddHob Mem = Mem4G;
  UINT8 MemGB = 4;
#elif RAM_SIZE == 6
  DeviceMemoryAddHob Mem = Mem6G;
  UINT8 MemGB = 6;
#elif RAM_SIZE == 8
  DeviceMemoryAddHob Mem = Mem8G;
  UINT8 MemGB = 8;
#elif RAM_SIZE == 10
  DeviceMemoryAddHob Mem = Mem10G;
  UINT8 MemGB = 10;
#elif RAM_SIZE == 12
  DeviceMemoryAddHob Mem = Mem12G;
  UINT8 MemGB = 12;
#endif

  // Memory   Min    Max   Config
#if RAM_SIZE == 4
  SIZE_MB_IN (3072,  4608, 4);
#elif RAM_SIZE == 6
  SIZE_MB_IN (5120,  6656, 6);
#elif RAM_SIZE == 8
  SIZE_MB_IN (7168,  8704, 8);
#elif RAM_SIZE == 10
  SIZE_MB_IN (9216,  10752, 10);
#elif RAM_SIZE == 12
  SIZE_MB_IN (11520, 12488, 12);
#endif

  DEBUG((EFI_D_INFO, "Select Config: %d GiB\n", MemGB));

  // Ensure PcdSystemMemorySize has been set
  ASSERT (PcdGet64 (PcdSystemMemorySize) != 0);

  // Run through each memory descriptor
  while (MemoryDescriptorEx->Length != 0) {
    switch (MemoryDescriptorEx->HobOption) {
#if RAM_SIZE == 4
    case Mem4G:
#elif RAM_SIZE == 6
    case Mem6G:
#elif RAM_SIZE == 8
    case Mem8G:
#elif RAM_SIZE == 10
    case Mem10G:
#elif RAM_SIZE == 12
    case Mem12G:
#endif
      if (MemoryDescriptorEx->HobOption != Mem) {
        MemoryDescriptorEx++;
        continue;
      }
    case AddMem:
    case AddDev:
    case HobOnlyNoCacheSetting:
      AddHob(MemoryDescriptorEx);
      break;
    case NoHob:
    default:
      goto update;
    }

    if (MemoryDescriptorEx->HobOption == HobOnlyNoCacheSetting) {
      MemoryDescriptorEx++;
      continue;
    }

  update:
    ASSERT(Index < MAX_ARM_MEMORY_REGION_DESCRIPTOR_COUNT);

    MemoryTable[Index].PhysicalBase = MemoryDescriptorEx->Address;
    MemoryTable[Index].VirtualBase  = MemoryDescriptorEx->Address;
    MemoryTable[Index].Length       = MemoryDescriptorEx->Length;
    MemoryTable[Index].Attributes   = MemoryDescriptorEx->ArmAttributes;

    Index++;
    MemoryDescriptorEx++;
  }

  // Last one (terminator)
  ASSERT(Index < MAX_ARM_MEMORY_REGION_DESCRIPTOR_COUNT);
  MemoryTable[Index].PhysicalBase = 0;
  MemoryTable[Index].VirtualBase  = 0;
  MemoryTable[Index].Length       = 0;
  MemoryTable[Index].Attributes   = 0;

  // Build Memory Allocation Hob
  DEBUG((EFI_D_INFO, "Configure MMU In \n"));
  InitMmu (MemoryTable);
  DEBUG((EFI_D_INFO, "Configure MMU Out \n"));

  if (FeaturePcdGet (PcdPrePiProduceMemoryTypeInformationHob)) {
    // Optional feature that helps prevent EFI memory map fragmentation.
    BuildMemoryTypeInformationHob ();
  }

  return EFI_SUCCESS;
}
