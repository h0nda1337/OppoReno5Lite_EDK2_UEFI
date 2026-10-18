/* SPDX-License-Identifier: BSD-2-Clause */
/* Real FDT reader plus role validator: ranges alone must not authorize GPIO. */
#include <Library/OppoHandoffLib.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
STATIC UINT8 *Load(CONST char *Path, UINTN *Bytes)
{
  FILE *F=fopen(Path,"rb");assert(F && !fseek(F,0,SEEK_END));long N=ftell(F);
  assert(N>=40 && N<=2*1024*1024 && !fseek(F,0,SEEK_SET));
  UINT8 *P=malloc(N);assert(P && fread(P,1,N,F)==(size_t)N);fclose(F);*Bytes=N;return P;
}
int main(int Argc, char **Argv)
{
  assert(Argc==2 || Argc==3);UINTN Bytes;OPPO_FDT_VIEW V;
  UINT8 *Original=Load(Argv[1],&Bytes),*Changed=malloc(Bytes);assert(Changed);
  assert(OppoFdtOpen(Original,Bytes,&V) && OppoSdProfileMatches(&V));
  assert(!OppoSdProfileMatches(NULL));
  assert(OppoSdPowerProfileMatches(&V) && !OppoSdPowerProfileMatches(NULL));
  assert(OppoSdPowerEnableProfileMatches(&V) && !OppoSdPowerEnableProfileMatches(NULL));
  for(UINTN Case=0;Case<7;Case++) {
    CONST VOID *Value;UINT32 Length;
    memcpy(Changed,Original,Bytes);assert(OppoFdtOpen(Changed,Bytes,&V));
    CONST CHAR8 *Path=Case==0 ? "/soc/iocfg_rm@11c20000" : Case==1 ? "/soc/pinctrl@10005000" : "/soc/msdc@11240000";
    CONST CHAR8 *Key=Case==0 ? "compatible" : Case==1 || Case==6 ? "reg" :
                      Case==2 ? "host_function" : Case==3 ? "bus-width" : "status";
    assert(OppoFdtProperty(&V,Path,Key,&Value,&Length)==EFI_SUCCESS);
    UINT8 *P=Changed+((CONST UINT8 *)Value-Changed);
    if(Case==0) {assert(Length>=18);P[15]='l';} // Same address, wrong lm role.
    if(Case==1) {assert(Length==32);P[21]=0xe7;} // Existing LB bank is not the RM alias.
    if(Case==2)P[0]=0; // eMMC must not receive this SD-only profile.
    if(Case==3)P[3]=8;
    if(Case==4)memcpy(P,"fail",5);
    if(Case==5)P[Length-1]='x'; // Unterminated status is not okay.
    if(Case==6)P[5]=0x23; // MSDC0 base instead of MSDC1.
    assert(!OppoSdProfileMatches(&V));
  }
  for(UINTN Case=0;Case<9;Case++) {
    CONST VOID *Value;UINT32 Length;
    memcpy(Changed,Original,Bytes);assert(OppoFdtOpen(Changed,Bytes,&V));
    CONST CHAR8 *Path=Case<3 ? "/soc/i2c_common" : Case<7 ? "/soc/i2c5@11016000" :
      Case==7 ? "/soc/i2c5@11016000/subpmic_ldo@64" : "/soc/msdc@11240000";
    CONST CHAR8 *Key=Case==0 ? "ver" : Case==1 ? "set_dt_div" : Case==2 ? "set_ltiming" :
      Case==3 ? "reg" : Case==4 ? "id" : Case==5 ? "clock-div" : Case==6 ? "sda-gpio-id" :
      Case==7 ? "compatible" : "vmmc-supply";
    assert(OppoFdtProperty(&V,Path,Key,&Value,&Length)==EFI_SUCCESS && Length);
    ((UINT8 *)Value)[Case==3 ? 5 : Length-1]^=1;
    assert(OppoSdProfileMatches(&V) && !OppoSdPowerProfileMatches(&V));
  }
  for(UINTN Case=0;Case<4;Case++) {
    CONST VOID *Value;UINT32 Length;
    memcpy(Changed,Original,Bytes);assert(OppoFdtOpen(Changed,Bytes,&V));
    CONST CHAR8 *Path=Case<2 ? "/soc/i2c5@11016000/subpmic_ldo@64/ldo3" : "/soc/i2c5@11016000/subpmic_ldo@64/ldo5";
    CONST CHAR8 *Key=Case%2 ? "regulator-max-microvolt" : "regulator-min-microvolt";
    assert(OppoFdtProperty(&V,Path,Key,&Value,&Length)==EFI_SUCCESS && Length==4);
    ((UINT8 *)Value)[0]=1;
    assert(OppoSdPowerProfileMatches(&V) && !OppoSdPowerEnableProfileMatches(&V));
  }
  free(Changed);free(Original);
  if(Argc==3) {
    Original=Load(Argv[2],&Bytes);
    assert(OppoFdtOpen(Original,Bytes,&V) && OppoSdProfileMatches(&V) && OppoSdPowerProfileMatches(&V) && OppoSdPowerEnableProfileMatches(&V));free(Original);
    puts("Saved CPH2205 runtime FDT accepted by named-role SD profile; no MMIO or phone operation.");
  }
  puts("SD profile: wrong named bank, pinctrl alias, eMMC, width, status and controller rejected with host sanitizers.");
  puts("SD power profile: real I2C V2 role, PIO/divider contract and supply links accepted; 9 mutations rejected without MMIO.");
  puts("SD enable profile: 4 voltage-bound mutations rejected; read-only query profile remains available.");
  return 0;
}
