/* SPDX-License-Identifier: BSD-2-Clause */
/* Real original serializer/installer, synthetic UEFI allocation/configuration services. */
#include <Library/OppoAcpiInstallLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <IndustryStandard/Acpi60.h>
#include <Guid/Acpi.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

EFI_GUID gEfiAcpiTableGuid=EFI_ACPI_TABLE_GUID, gEfiAcpi10TableGuid=ACPI_10_TABLE_GUID;
EFI_BOOT_SERVICES *gBS; EFI_SYSTEM_TABLE *gST;
static EFI_BOOT_SERVICES services; static EFI_SYSTEM_TABLE system_table;
static EFI_CONFIGURATION_TABLE entries[2];
static UINTN scenario, page_allocs, page_frees, pool_allocs, pool_frees, installs, map_calls;
static VOID *page, *map_buffer;
VOID *EFIAPI ZeroMem(VOID *p,UINTN n) { return memset(p,0,n); }
VOID *EFIAPI CopyMem(VOID *a,CONST VOID *b,UINTN n) { return memmove(a,b,n); }
INTN EFIAPI CompareMem(CONST VOID *a,CONST VOID *b,UINTN n) { return memcmp(a,b,n); }

static EFI_STATUS EFIAPI allocate_pages(EFI_ALLOCATE_TYPE a,EFI_MEMORY_TYPE t,UINTN n,EFI_PHYSICAL_ADDRESS *b)
{
  assert(a==AllocateAnyPages && t==EfiACPIReclaimMemory && n==1 && b);page_allocs++;
  if(scenario==1)return EFI_OUT_OF_RESOURCES;
  page=aligned_alloc(EFI_PAGE_SIZE,n*EFI_PAGE_SIZE);assert(page);memset(page,0xa5,n*EFI_PAGE_SIZE);
  *b=(UINTN)page;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI free_pages(EFI_PHYSICAL_ADDRESS b,UINTN n)
{
  assert(b==(UINTN)page && n==1);page_frees++;
  if(scenario==13)return EFI_DEVICE_ERROR;
  free(page);page=NULL;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI allocate_pool(EFI_MEMORY_TYPE t,UINTN n,VOID **p)
{
  assert(t==EfiBootServicesData && n<=1024*1024);pool_allocs++;
  if(scenario==8)return EFI_OUT_OF_RESOURCES;
  map_buffer=malloc(n);assert(map_buffer);memset(map_buffer,0,n);*p=map_buffer;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI free_pool(VOID *p)
{
  assert(p==map_buffer);pool_frees++;
  if(scenario==9)return EFI_DEVICE_ERROR;
  free(p);map_buffer=NULL;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI memory_map(UINTN *bytes,EFI_MEMORY_DESCRIPTOR *map,UINTN *key,UINTN *stride,UINT32 *version)
{
  map_calls++;*key=123;*stride=sizeof(*map)+8;*version=EFI_MEMORY_DESCRIPTOR_VERSION;
  if(scenario==3)return EFI_DEVICE_ERROR;
  if(!map){*bytes=*stride;if(scenario==4)*stride=1;if(scenario==5)*bytes=2*1024*1024;return EFI_BUFFER_TOO_SMALL;}
  if(scenario==10 && map_calls==2){*bytes=*stride*10;return EFI_BUFFER_TOO_SMALL;}
  if(scenario==11){*bytes=*stride*10;return EFI_BUFFER_TOO_SMALL;}
  assert(*bytes>=*stride);*bytes=*stride;
  map->Type=scenario==6?EfiBootServicesData:EfiACPIReclaimMemory;
  map->PhysicalStart=(UINTN)page+(scenario==7?EFI_PAGE_SIZE:0);
  map->NumberOfPages=1;
  if(scenario==12)(*version)++;
  if(scenario==14)(*bytes)--;
  if(scenario==15){map->PhysicalStart=MAX_UINT64-EFI_PAGE_MASK;map->NumberOfPages=2;}
  return EFI_SUCCESS;
}
static UINT8 sum(CONST VOID *p,UINTN n)
{ CONST UINT8 *b=p;UINT8 s=0;for(UINTN i=0;i<n;i++)s=(UINT8)(s+b[i]);return s; }
static VOID checksum(VOID *p,UINTN n)
{ UINT8 *b=p;b[9]=0;b[9]=(UINT8)(0-sum(p,n)); }
static VOID validate_tables(VOID *pointer)
{
  assert(pointer==page);UINT8 *b=page;
  EFI_ACPI_6_0_ROOT_SYSTEM_DESCRIPTION_POINTER *r=pointer;
  assert(r->Signature==EFI_ACPI_6_0_ROOT_SYSTEM_DESCRIPTION_POINTER_SIGNATURE && r->Revision==2);
  assert(sum(r,20)==0 && sum(r,r->Length)==0 && r->RsdtAddress==0);
  EFI_ACPI_DESCRIPTION_HEADER *x=(VOID *)(UINTN)r->XsdtAddress;
  assert(x->Signature==SIGNATURE_32('X','S','D','T') && x->Length==60 && sum(x,x->Length)==0);
  UINT64 pointers[3];memcpy(pointers,(UINT8 *)x+sizeof(*x),sizeof(pointers));
  UINT32 signatures[]={SIGNATURE_32('F','A','C','P'),SIGNATURE_32('A','P','I','C'),SIGNATURE_32('G','T','D','T')};
  for(UINTN i=0;i<3;i++) {
    assert(pointers[i]>=(UINTN)page && pointers[i]<(UINTN)page+EFI_PAGE_SIZE);
    EFI_ACPI_DESCRIPTION_HEADER *h=(VOID *)(UINTN)pointers[i];
    assert(h->Signature==signatures[i] && h->Length<=EFI_PAGE_SIZE-(pointers[i]-(UINTN)page) && sum(h,h->Length)==0);
  }
  EFI_ACPI_6_0_FIXED_ACPI_DESCRIPTION_TABLE *f=(VOID *)(UINTN)pointers[0];
  EFI_ACPI_DESCRIPTION_HEADER *d=(VOID *)(UINTN)f->XDsdt;
  assert(d->Signature==SIGNATURE_32('D','S','D','T') && sum(d,d->Length)==0);
  EFI_ACPI_6_0_MULTIPLE_APIC_DESCRIPTION_TABLE_HEADER *m=(VOID *)(UINTN)pointers[1];
  for(UINTN i=0;i<8;i++) {
    EFI_ACPI_6_0_GIC_STRUCTURE *c=(VOID *)((UINT8 *)m+sizeof(*m)+80*i);
    assert(c->AcpiProcessorUid==i && c->MPIDR==i*256 && c->GICRBaseAddress==0xc040000+i*0x20000);
  }
  for(UINTN i=OppoAcpiTableBytes();i<EFI_PAGE_SIZE;i++)assert(b[i]==0);
}
static EFI_STATUS EFIAPI install(EFI_GUID *guid,VOID *p)
{
  installs++;assert(memcmp(guid,&gEfiAcpiTableGuid,sizeof(*guid))==0);validate_tables(p);
  assert(pool_allocs==pool_frees && page_frees==0 && gST->NumberOfTableEntries==0);
  if(scenario==13 || scenario==16)return EFI_OUT_OF_RESOURCES;
  entries[0]=(EFI_CONFIGURATION_TABLE){*guid,p};gST->ConfigurationTable=entries;gST->NumberOfTableEntries=1;
  if(scenario==21)gST->NumberOfTableEntries=0;
  if(scenario==22)entries[0].VendorTable=(UINT8 *)p+16;
  if(scenario==23)((UINT8 *)p)[8]^=1;
  if(scenario==24){EFI_ACPI_6_0_ROOT_SYSTEM_DESCRIPTION_POINTER *r=p;EFI_ACPI_DESCRIPTION_HEADER *x=(VOID *)(UINTN)r->XsdtAddress;x->Length=MAX_UINT32;}
  if(scenario==25){EFI_ACPI_6_0_ROOT_SYSTEM_DESCRIPTION_POINTER *r=p;r->XsdtAddress=MAX_UINT64;}
  if(scenario==26 || scenario==27) {
    EFI_ACPI_6_0_ROOT_SYSTEM_DESCRIPTION_POINTER *r=p;UINT64 pointers[3];
    memcpy(pointers,(UINT8 *)(UINTN)r->XsdtAddress+36,sizeof(pointers));
    EFI_ACPI_DESCRIPTION_HEADER *h=(VOID *)(UINTN)pointers[scenario==26?1:2];
    if(scenario==26)h->Length=44; /* A checksummed MADT with no CPU/GIC entries. */
    else h->Revision=0xff;
    checksum(h,h->Length);
  }
  return EFI_SUCCESS;
}
int main(void)
{
  OPPO_ACPI_PROFILE profile={0};
  /* Profile from physical observation28, not a proof of execution on this PC. */
  for(UINTN i=0;i<8;i++)profile.Cpu[i]=(OPPO_ACPI_CPU){i*256,0xc040000+i*0x20000};
  services.AllocatePages=allocate_pages;services.FreePages=free_pages;
  services.AllocatePool=allocate_pool;services.FreePool=free_pool;
  services.GetMemoryMap=memory_map;services.InstallConfigurationTable=install;
  for(scenario=0;scenario<=27;scenario++) {
    gBS=&services;gST=&system_table;memset(&system_table,0,sizeof(system_table));
    page_allocs=page_frees=pool_allocs=pool_frees=installs=map_calls=0;
    page=map_buffer=NULL;OPPO_ACPI_INSTALL_REPORT report;OPPO_ACPI_PROFILE p=profile;
    if(scenario==2)p.Cpu[1].Mpidr=0;
    if(scenario==17 || scenario==18){entries[0].VendorGuid=scenario==17?gEfiAcpiTableGuid:gEfiAcpi10TableGuid;gST->ConfigurationTable=entries;gST->NumberOfTableEntries=1;}
    if(scenario==19)gST->NumberOfTableEntries=257;
    if(scenario==20)gST=NULL;
    EFI_STATUS status=OppoInstallAcpi(&p,&report);
    if(scenario==0 || scenario==10) {
      assert(status==EFI_SUCCESS && report.Published && report.Base==(UINTN)page && report.Pages==1);
      assert(report.Layout.TotalBytes==1528 && report.MemoryStatus==EFI_SUCCESS && page_frees==0 && installs==1);
      assert(report.ReadbackStatus==EFI_SUCCESS);
      validate_tables((VOID *)(UINTN)report.Base);
      OPPO_ACPI_INSTALL_REPORT again;
      assert(OppoInstallAcpi(&p,&again)==EFI_ALREADY_STARTED && page_allocs==1 && installs==1);
      free(page);page=NULL;
    } else if(scenario>=21) {
      assert(status==EFI_COMPROMISED_DATA && report.Published && report.ReadbackStatus==EFI_COMPROMISED_DATA);
      assert(page_frees==0 && report.Base==(UINTN)page && report.Pages==1 && report.CleanupStatus==EFI_NOT_STARTED);
      free(page);page=NULL;
    } else {
      assert(EFI_ERROR(status) && !report.Published);
      assert(page_allocs==(scenario>=17?0U:1U));
      assert(page_frees==((scenario!=1 && scenario<17)?1U:0U));
      if(scenario==13){assert(report.CleanupStatus==EFI_DEVICE_ERROR && report.Base==(UINTN)page && report.Pages==1);free(page);page=NULL;}
      else assert(report.Base==0 && report.Pages==0);
      if(scenario==9){free(map_buffer);map_buffer=NULL;}
    }
    assert(page==NULL && map_buffer==NULL);
  }
  assert(OppoInstallAcpi(NULL,NULL)==EFI_INVALID_PARAMETER);
  OPPO_ACPI_INSTALL_REPORT alias;
  assert(OppoInstallAcpi((VOID *)&alias,&alias)==EFI_INVALID_PARAMETER);
  puts("ACPI install:28 UEFI transaction cases, physical-profile replay, checksums/pointers/padding, retries, conflicts, cleanup and post-publication damage detection passed; host only");
  return 0;
}
