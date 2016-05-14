/* cpuid.h - CPU capabilities */

/* Copyright (C) 2015 Bth8 <bth8fwd@gmail.com>
 *
 *  This file is part of Dionysus.
 *
 *  Dionysus is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Dionysus is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Dionysus.  If not, see <http://www.gnu.org/licenses/>
 */

#ifndef CPUID_H
#define CPUID_H

/* Vendor-strings. */
#define CPUID_VENDOR_OLDAMD			"AMDisbetter!" //early engineering samples of AMD K5 processor
#define CPUID_VENDOR_AMD			"AuthenticAMD"
#define CPUID_VENDOR_INTEL			"GenuineIntel"
#define CPUID_VENDOR_VIA			"CentaurHauls"
#define CPUID_VENDOR_OLDTRANSMETA	"TransmetaCPU"
#define CPUID_VENDOR_TRANSMETA		"GenuineTMx86"
#define CPUID_VENDOR_CYRIX			"CyrixInstead"
#define CPUID_VENDOR_CENTAUR		"CentaurHauls"
#define CPUID_VENDOR_NEXGEN			"NexGenDriven"
#define CPUID_VENDOR_UMC			"UMC UMC UMC "
#define CPUID_VENDOR_SIS			"SiS SiS SiS "
#define CPUID_VENDOR_NSC			"Geode by NSC"
#define CPUID_VENDOR_RISE			"RiseRiseRise"

#define CPUID_FEAT_ECX_SSE3			0x00000001
#define CPUID_FEAT_ECX_PCLMUL		0x00000002
#define CPUID_FEAT_ECX_DTES64		0x00000004
#define CPUID_FEAT_ECX_MONITOR		0x00000008
#define CPUID_FEAT_ECX_DS_CPL		0x00000010
#define CPUID_FEAT_ECX_VMX			0x00000020
#define CPUID_FEAT_ECX_SMX			0x00000040
#define CPUID_FEAT_ECX_EST			0x00000080
#define CPUID_FEAT_ECX_TM2			0x00000100
#define CPUID_FEAT_ECX_SSSE3		0x00000200
#define CPUID_FEAT_ECX_CID			0x00000400
#define CPUID_FEAT_ECX_FMA			0x00000800
#define CPUID_FEAT_ECX_CX16			0x00001000
#define CPUID_FEAT_ECX_ETPRD		0x00002000
#define CPUID_FEAT_ECX_PDCM			0x00004000
#define CPUID_FEAT_ECX_DCA			0x00008000
#define CPUID_FEAT_ECX_SSE4_1		0x00010000
#define CPUID_FEAT_ECX_SSE4_2		0x00020000
#define CPUID_FEAT_ECX_x2APIC		0x00040000
#define CPUID_FEAT_ECX_MOVBE		0x00080000
#define CPUID_FEAT_ECX_POPCNT		0000100000
#define CPUID_FEAT_ECX_AES			0x00200000
#define CPUID_FEAT_ECX_XSAVE		0x00400000
#define CPUID_FEAT_ECX_OSXSAVE		0x00800000
#define CPUID_FEAT_ECX_AVX			0x01000000

#define CPUID_FEAT_EDX_FPU			0x00000001
#define CPUID_FEAT_EDX_VME			0x00000002
#define CPUID_FEAT_EDX_DE			0x00000004
#define CPUID_FEAT_EDX_PSE			0x00000008
#define CPUID_FEAT_EDX_TSC			0x00000010
#define CPUID_FEAT_EDX_MSR			0x00000020
#define CPUID_FEAT_EDX_PAE			0x00000040
#define CPUID_FEAT_EDX_MCE			0x00000080
#define CPUID_FEAT_EDX_CX8			0x00000100
#define CPUID_FEAT_EDX_APIC			0x00000200
#define CPUID_FEAT_EDX_SEP			0x00000800
#define CPUID_FEAT_EDX_MTRR			0x00001000
#define CPUID_FEAT_EDX_PGE			0x00002000
#define CPUID_FEAT_EDX_MCA			0x00004000
#define CPUID_FEAT_EDX_CMOV			0x00008000
#define CPUID_FEAT_EDX_PAT			0x00010000
#define CPUID_FEAT_EDX_PSE36		0x00020000
#define CPUID_FEAT_EDX_PSN			0x00040000
#define CPUID_FEAT_EDX_CLF			0x00080000
#define CPUID_FEAT_EDX_DTES			0x00200000
#define CPUID_FEAT_EDX_ACPI			0x00400000
#define CPUID_FEAT_EDX_MMX			0x00800000
#define CPUID_FEAT_EDX_FXSR			0x01000000
#define CPUID_FEAT_EDX_SSE			0x02000000
#define CPUID_FEAT_EDX_SSE2			0x04000000
#define CPUID_FEAT_EDX_SS			0x08000000
#define CPUID_FEAT_EDX_HTT			0x10000000
#define CPUID_FEAT_EDX_TM1			0x20000000
#define CPUID_FEAT_EDX_IA64			0x40000000
#define CPUID_FEAT_EDX_PBE			0x80000000

#define CPUID_GET_VENDOR			0x00000000
#define CPUID_GET_FEATURES			0x00000001
#define CPUID_GET_TLB				0x00000002
#define CUPID_GET_SERIALNO			0x00000004
#define CPUID_EXT					0x80000000
#define CPUID_EXT_FEATURES			0x80000001
#define CPUID_EXT_BRAND0			0x80000002
#define CPUID_EXT_BRAND1			0x80000004
#define CPUID_EXT_BRAND2			0x80000008

extern inline void cpuid(uint32_t cmd, void *regs) {
	uint32_t *ints = (uint32_t *)regs;
	asm volatile("cpuid" :
		"=b"(ints[0]), "=c"(ints[1]), "=d"(ints[2]) :
		"a"(cmd));
}

#endif /* CPUID_H */