// SPDX-License-Identifier: GPL-2.0
/*
 * dmesg_injector_efi.c -- UEFI application
 *
 * Injects fake driver-init messages + Putinisms into UEFI console.
 * When a network card is available, connects to a remote AI host
 * via UDP4 to receive live messages for injection.
 *
 * Build:  make efi
 */

#ifndef __x86_64__
#error "x86-64 target required for EFI"
#endif

#define NULL ((void *)0)

/* ------------------------------------------------------------------ */
/*  UEFI base types                                                    */
/* ------------------------------------------------------------------ */
typedef unsigned long long UINT64;
typedef unsigned int       UINT32;
typedef unsigned short     UINT16;
typedef unsigned char      UINT8;
typedef long long          INT64;
typedef int                INT32;
typedef short              INT16;
typedef signed char         INT8;
typedef UINT64             UINTN;
typedef INT64              INTN;
typedef int                BOOLEAN;
#define TRUE  1
#define FALSE 0
typedef UINT64             EFI_STATUS;
typedef void              *EFI_HANDLE;
typedef void              *EFI_EVENT;
typedef UINT16             CHAR16;

#define EFI_ERROR(s)  ((INT64)(s) < 0)
#define EFI_SUCCESS   0
#define EFIAPI __attribute__((ms_abi))

typedef struct { UINT32 a; UINT16 b, c; UINT8 d[8]; } EFI_GUID;

typedef struct {
	UINT64 Signature;
	UINT32 Revision, HeaderSize, CRC32, Reserved;
} EFI_TABLE_HEADER;

/* Forward decl for system table */
typedef struct _EFI_RUNTIME_SERVICES {
	EFI_TABLE_HEADER Hdr;
	UINT8 _rsv[128];
} EFI_RUNTIME_SERVICES;

/* ------------------------------------------------------------------ */
/*  IPv4 address                                                       */
/* ------------------------------------------------------------------ */
typedef struct { UINT8 Addr[4]; } EFI_IPv4_ADDRESS;

/* ------------------------------------------------------------------ */
/*  Event / Timer constants                                           */
/* ------------------------------------------------------------------ */
#define EVT_TIMER          0x80000000
#define EVT_NOTIFY_SIGNAL  0x00000100
#define TimerPeriodic      1
#define EVT_NOTIFY_WAIT    0x00000100

/* ------------------------------------------------------------------ */
/*  Simple Text Output Protocol                                        */
/* ------------------------------------------------------------------ */
typedef EFI_STATUS (EFIAPI *EFI_TEXT_STRING)(void *, CHAR16 *);

/* ------------------------------------------------------------------ */
/*  Simple Network Protocol                                            */
/* ------------------------------------------------------------------ */
#define EFI_SIMPLE_NETWORK_PROTOCOL_GUID \
	{ 0xA19832B9, 0xAC25, 0x11D3, {0x9A,0x2D,0x00,0x90,0x27,0x3F,0xC1,0x4D} }

typedef struct {
	UINT64 Revision;
	void *Start; void *Stop; void *Initialize; void *Reset; void *Shutdown;
	void *ReceiveFilters; void *StationAddress; void *Statistics;
	void *MCastIPtoMAC; void *MCastMACtoIP; void *GetStatus;
	void *Transmit; void *Receive;
	EFI_EVENT WaitForPacket;
	void *Mode;
} EFI_SIMPLE_NETWORK_PROTOCOL;

/* ------------------------------------------------------------------ */
/*  UDP4 Protocol                                                      */
/* ------------------------------------------------------------------ */
#define EFI_UDP4_PROTOCOL_GUID \
	{ 0x3AD9DF29, 0x4501, 0x478D, {0xB1,0xF8,0x7F,0x7F,0xE7,0x0E,0x50,0xF3} }

typedef struct {
	UINT8 AcceptBroadcast, AcceptPromiscuous, AcceptAnyPort, AllowDuplicatePort;
	UINT8 TypeOfService, TimeToLive, DoNotFragment;
	UINT16 ReceiveTimeout, TransmitTimeout;
	BOOLEAN UseDefaultAddress;
	EFI_IPv4_ADDRESS StationAddress, SubnetMask;
	UINT16 StationPort;
	EFI_IPv4_ADDRESS RemoteAddress;
	UINT16 RemotePort;
} EFI_UDP4_CONFIG_DATA;

typedef struct {
	UINT32 FragmentLength;
	void  *FragmentBuffer;
} EFI_UDP4_FRAGMENT_DATA;

typedef struct {
	UINT32      DataLength;
	EFI_EVENT   RecycleEvent;
	UINT32      FragmentCount;
	EFI_UDP4_FRAGMENT_DATA FragmentTable[1];
} EFI_UDP4_RECEIVE_DATA;

typedef struct {
	EFI_EVENT  Event;
	EFI_STATUS Status;
	union {
		EFI_UDP4_RECEIVE_DATA *RxData;
		void *TxData;
	} Packet;
} EFI_UDP4_COMPLETION_TOKEN;

typedef struct _EFI_UDP4_PROTOCOL {
	EFI_STATUS (EFIAPI *Config)(struct _EFI_UDP4_PROTOCOL *, EFI_UDP4_CONFIG_DATA *);
	EFI_STATUS (EFIAPI *Transmit)(struct _EFI_UDP4_PROTOCOL *, EFI_UDP4_COMPLETION_TOKEN *);
	EFI_STATUS (EFIAPI *Receive)(struct _EFI_UDP4_PROTOCOL *, EFI_UDP4_COMPLETION_TOKEN *);
	EFI_STATUS (EFIAPI *Cancel)(struct _EFI_UDP4_PROTOCOL *, EFI_UDP4_COMPLETION_TOKEN *);
	EFI_STATUS (EFIAPI *Poll)(struct _EFI_UDP4_PROTOCOL *);
	void *Mode;
} EFI_UDP4_PROTOCOL;

/* ------------------------------------------------------------------ */
/*  Boot Services (fields at fixed offsets per UEFI 2.x spec)         */
/* ------------------------------------------------------------------ */
typedef struct {
	EFI_TABLE_HEADER Hdr;        /* 0  */
	void *RaiseTPL;              /* 24 */
	void *RestoreTPL;            /* 32 */
	void *AllocatePages;         /* 40 */
	void *FreePages;             /* 48 */
	void *GetMemoryMap;          /* 56 */
	void *AllocatePool;          /* 64 */
	void *FreePool;              /* 72 */
	void *CreateEvent;           /* 80 */
	void *SetTimer;              /* 88 */
	void *WaitForEvent;          /* 96 */
	void *SignalEvent;           /* 104 */
	void *CloseEvent;            /* 112 */
	void *CheckEvent;            /* 120 */
	void *InstallProtocol;       /* 128 */
	void *ReinstallProtocol;     /* 136 */
	void *UninstallProtocol;     /* 144 */
	void *HandleProtocol;        /* 152 */
	void *Void;                  /* 160 */
	void *RegisterProtocolNotify;/* 168 */
	void *LocateHandle;          /* 176 */
	void *LocateDevicePath;      /* 184 */
	void *InstallConfigTable;    /* 192 */
	void *LoadImage;             /* 200 */
	void *StartImage;            /* 208 */
	void *Exit;                  /* 216 */
	void *UnloadImage;           /* 224 */
	void *ExitBootServices;      /* 232 */
	void *GetMonotonicCount;     /* 240 */
	void *Stall;                 /* 248 */
	void *SetWatchdogTimer;      /* 256 */
	void *ConnectController;     /* 264 */
	void *DisconnectController;  /* 272 */
	void *OpenProtocol;          /* 280 */
	void *CloseProtocol;         /* 288 */
	void *OpenProtocolInfo;      /* 296 */
	void *ProtocolsPerHandle;    /* 304 */
	void *LocateHandleBuffer;    /* 312 */
	void *LocateProtocol;        /* 320 */
	void *InstallMultiple;       /* 328 */
	void *UninstallMultiple;     /* 336 */
	void *CalculateCrc32;        /* 344 */
} __attribute__((packed)) EFI_BOOT_SERVICES;

/* ------------------------------------------------------------------ */
/*  System Table                                                       */
/* ------------------------------------------------------------------ */
typedef struct {
	EFI_TABLE_HEADER  Hdr;
	CHAR16           *FirmwareVendor;
	UINT32            FirmwareRevision;
	EFI_HANDLE        ConsoleInHandle;
	void             *ConIn;
	EFI_HANDLE        ConsoleOutHandle;
	void             *ConOut;
	EFI_HANDLE        StdErrHandle;
	void             *StdErr;
	EFI_RUNTIME_SERVICES *RuntimeServices;
	EFI_BOOT_SERVICES *BootServices;
} EFI_SYSTEM_TABLE;

/* ------------------------------------------------------------------ */
/*  Function pointer typedefs for boot service calls                  */
/* ------------------------------------------------------------------ */
typedef EFI_STATUS (EFIAPI *FN_CREATE_EVENT)(UINT32, UINTN, void *, void *, EFI_EVENT *);
typedef EFI_STATUS (EFIAPI *FN_SET_TIMER)(EFI_EVENT, UINTN, UINT64);
typedef EFI_STATUS (EFIAPI *FN_LOCATE_PROTOCOL)(EFI_GUID *, void *, void **);

/* ------------------------------------------------------------------ */
/*  Globals                                                            */
/* ------------------------------------------------------------------ */
static EFI_SYSTEM_TABLE      *gST;
static EFI_BOOT_SERVICES     *gBS;
static void                  *gConOut;
static EFI_TEXT_STRING        orig_OutStr;
static EFI_TEXT_STRING        conOut_OutStr;
static EFI_EVENT              gTimerEvent;
static UINT64                 rng_state = 0x1234;

/* Network state */
static EFI_UDP4_PROTOCOL     *gUdp4;
static BOOLEAN                networkUp;
static UINTN                  injection_count;
static const UINTN            MAX_INJ = 40;

#define LISTEN_PORT  4444

/* GUIDs */
static EFI_GUID gEfiSnpGuid = EFI_SIMPLE_NETWORK_PROTOCOL_GUID;
static EFI_GUID gEfiUdp4Guid = EFI_UDP4_PROTOCOL_GUID;

/* ------------------------------------------------------------------ */
/*  Random helpers                                                     */
/* ------------------------------------------------------------------ */
static void srand_efi(UINT64 s) { rng_state = s; }
static UINT64 rand_efi(void) {
	rng_state = rng_state * 6364136223846793005ULL + 1442695040888963407ULL;
	return rng_state >> 33;
}
static int rand_range(int lo, int hi)
{ return lo + (int)(rand_efi() % (hi - lo + 1)); }
static int roll_9(void)
{ return (rand_efi() % 100) < 9; }

/* Pipe-delimited Putin phrase list (compact) */
static const CHAR16 putinisms[] =
	L"reminds me of tagging a Siberian tiger in Kamchatka|"
	L"key to good firmware is regular hockey practice|"
	L"we drilled for this in the taiga last summer|"
	L"solved this bug while fishing on Lake Baikal|"
	L"strength comparable to Russian birch|"
	L"even Amur leopards handle edge cases better|"
	L"my personal trainer agrees this DMA is optimal|"
	L"this init is as vast as the Russian taiga|"
	L"smooth as my figure skating routine|"
	L"precision matches my biathlon marksmanship|"
	L"I studied these registers while piloting a submarine|"
	L"reminds me of tranquilizing a polar bear|"
	L"stress-tested this driver while climbing Elbrus|"
	L"worthy of a standing ovation at the Bolshoi";

static const CHAR16 *pick_putin(void)
{
	int idx = rand_range(0, 13);
	const CHAR16 *p = putinisms;
	while (idx--) { while (*p && *p != L'|') p++; if (*p == L'|') p++; }
	return p;
}

/* ------------------------------------------------------------------ */
/*  Console helpers                                                    */
/* ------------------------------------------------------------------ */
static CHAR16 linebuf[400];
static CHAR16 numbuf[16];

static void efi_print(const CHAR16 *s)
{ if (conOut_OutStr) conOut_OutStr(gConOut, (CHAR16 *)s); }

static int write_num(int v)
{
	int i = 0, j;
	if (v == 0) { numbuf[0] = L'0'; numbuf[1] = 0; return 1; }
	while (v && i < 14) { numbuf[i++] = L'0' + (v % 10); v /= 10; }
	numbuf[i] = 0;
	for (j = 0; j < i / 2; j++) {
		CHAR16 t = numbuf[j];
		numbuf[j] = numbuf[i - 1 - j];
		numbuf[i - 1 - j] = t;
	}
	return i;
}

static CHAR16 *cpy16(CHAR16 *d, const CHAR16 *s)
{ while (*s) *d++ = *s++; return d; }

static CHAR16 *cpy8(CHAR16 *d, const char *s)
{ while (*s) *d++ = (CHAR16)(unsigned char)*s++; return d; }

static CHAR16 *write_pci(CHAR16 *p)
{
	*p++ = L'0'; *p++ = L'0'; *p++ = L'0'; *p++ = L'0'; *p++ = L':';
	int bus = rand_range(0, 31);
	if (bus > 9) *p++ = L'0' + bus/10;
	*p++ = L'0' + bus%10; *p++ = L':';
	int dev = rand_range(0, 31);
	if (dev > 9) *p++ = L'0' + dev/10;
	*p++ = L'0' + dev%10; *p++ = L'.';
	*p++ = L'0' + rand_range(0, 7);
	return p;
}

static const char *drivers[] = {
	"e1000e","igb","r8169","iwlwifi","nvidia","amdgpu","i915",
	"nvme","xhci_hcd","virtio_pci","dm_mod","bluetooth",
	"intel_pstate","rtl8192ce","ahci","sd_mod",
};

/* ------------------------------------------------------------------ */
/*  Print a fake driver message                                        */
/* ------------------------------------------------------------------ */
static void print_driver_msg(void)
{
	const char *drv = drivers[rand_efi() %
		(sizeof(drivers)/sizeof(drivers[0]))];
	int irq = rand_range(1, 255), err = rand_range(2, 61);
	CHAR16 *p = linebuf;

	p = cpy8(p, drv); p = cpy16(p, L": ");

	switch (rand_efi() % 12) {
	case 0:  p = cpy16(p, L"probe of "); p = write_pci(p);
		 p = cpy16(p, L" succeeded"); break;
	case 1:  p = cpy16(p, L"probe of "); p = write_pci(p);
		 p = cpy16(p, L" failed with error -");
		 p += write_num(err); break;
	case 2:  p = cpy16(p, L"device "); p = write_pci(p);
		 p = cpy16(p, L" registered"); break;
	case 3:  p = cpy16(p, L"BAR0 at 0xFED");
		 *p++ = L'0' + (rand_efi() % 16);
		 *p++ = L'0' + (rand_efi() % 16);
		 *p++ = L'0' + (rand_efi() % 16); break;
	case 4:  p = cpy16(p, L"DMA mask set to ");
		 p += write_num(rand_range(32,64));
		 p = cpy16(p, L"-bit"); break;
	case 5:  p = cpy16(p, L"firmware loaded for ");
		 p = write_pci(p); break;
	case 6:  p = cpy16(p, L"interrupt enabled (irq ");
		 p += write_num(irq); *p++ = L')'; break;
	case 7:  p = cpy16(p, L"PM OK for ");
		 p = write_pci(p); break;
	case 8:  p = cpy16(p, L"link up on ");
		 p = write_pci(p); break;
	case 9:  p = cpy16(p, L"thermal throttling on ");
		 p = write_pci(p); break;
	case 10: p = cpy16(p, L"microcode updated on ");
		 p = write_pci(p); break;
	default: p = cpy16(p, L"reset detected on ");
		 p = write_pci(p); break;
	}
	p = cpy16(p, L"\r\n"); *p = 0;
	efi_print(linebuf);

	if (roll_9()) {
		const CHAR16 *put = pick_putin();
		p = linebuf; p = cpy16(p, L"  -- ");
		p = cpy16(p, put); p = cpy16(p, L"\r\n"); *p = 0;
		efi_print(linebuf);
	}
}

/* ------------------------------------------------------------------ */
/*  Inject a received network message into console                    */
/* ------------------------------------------------------------------ */
static void inject_network_msg(const CHAR16 *msg, UINTN len)
{
	CHAR16 *p = linebuf;
	UINTN i;
	p = cpy16(p, L"[AI] ");
	for (i = 0; i < len && i < 360 && msg[i]; i++)
		*p++ = msg[i];
	p = cpy16(p, L"\r\n"); *p = 0;
	efi_print(linebuf);

	if (roll_9()) {
		const CHAR16 *put = pick_putin();
		p = linebuf; p = cpy16(p, L"  -- ");
		p = cpy16(p, put); p = cpy16(p, L"\r\n"); *p = 0;
		efi_print(linebuf);
	}
}

/* ------------------------------------------------------------------ */
/*  Network initialization                                             */
/* ------------------------------------------------------------------ */
static void init_network(void)
{
	void *snp;
	int i;
	EFI_UDP4_CONFIG_DATA cfg;
	UINT8 *cp;

	/* Try LocateProtocol for SNP */
	FN_LOCATE_PROTOCOL locate = (FN_LOCATE_PROTOCOL)gBS->LocateProtocol;
	if (!locate) return;

	snp = NULL;
	locate(&gEfiSnpGuid, NULL, &snp);
	if (!snp) return;

	/* Locate UDP4 */
	gUdp4 = NULL;
	locate(&gEfiUdp4Guid, NULL, (void **)&gUdp4);
	if (!gUdp4) return;

	/* Configure UDP4 */
	cp = (UINT8 *)&cfg;
	for (i = 0; i < (int)sizeof(cfg); i++) cp[i] = 0;
	cfg.AcceptBroadcast   = TRUE;
	cfg.AcceptAnyPort     = TRUE;
	cfg.UseDefaultAddress = TRUE;
	cfg.StationPort       = LISTEN_PORT;

	if (EFI_ERROR(gUdp4->Config(gUdp4, &cfg))) {
		gUdp4 = NULL;
		return;
	}

	networkUp = TRUE;
}

/* ------------------------------------------------------------------ */
/*  Poll for network messages                                          */
/* ------------------------------------------------------------------ */
static void poll_network(void)
{
	EFI_UDP4_COMPLETION_TOKEN token;
	EFI_UDP4_RECEIVE_DATA *rx;
	CHAR16 wide_msg[256];
	UINTN i;
	int ret;
	UINT8 *pkt;

	if (!networkUp || !gUdp4) return;

	gUdp4->Poll(gUdp4);

	/* Set up a synchronous receive attempt */
	/* We zero the token and set a bogus event pointer — some
	 * implementations will still accept a synchronous Poll-based read.
	 * If the firmware rejects this, we gracefully fall through. */
	for (i = 0; i < sizeof(token); i++)
		((UINT8 *)&token)[i] = 0;

	ret = gUdp4->Receive(gUdp4, &token);
	if (EFI_ERROR(ret)) return;

	/* If we got data, process it */
	rx = token.Packet.RxData;
	if (!rx || !rx->DataLength) {
		gUdp4->Cancel(gUdp4, &token);
		return;
	}

	/* Convert received bytes to CHAR16 */
	pkt = (UINT8 *)rx->FragmentTable[0].FragmentBuffer;
	if (pkt && rx->FragmentTable[0].FragmentLength > 0) {
		UINTN flen = rx->FragmentTable[0].FragmentLength;
		if (flen > 250) flen = 250;
		for (i = 0; i < flen; i++)
			wide_msg[i] = (CHAR16)pkt[i];
		wide_msg[flen] = 0;
		inject_network_msg(wide_msg, flen);
	}

	/* Recycle */
	if (rx->RecycleEvent) {
		typedef EFI_STATUS (EFIAPI *FN_SIGNAL)(EFI_EVENT);
		FN_SIGNAL sig = (FN_SIGNAL)gBS->SignalEvent;
		if (sig) sig(rx->RecycleEvent);
	}

	/* Cancel any pending */
	gUdp4->Cancel(gUdp4, &token);
}

/* ------------------------------------------------------------------ */
/*  Timer callback                                                     */
/* ------------------------------------------------------------------ */
static void EFIAPI timer_cb(EFI_EVENT ev, void *ctx)
{
	(void)ev; (void)ctx;
	if (injection_count >= MAX_INJ) return;

	if (networkUp)
		poll_network();
	print_driver_msg();
	injection_count++;
}

/* ------------------------------------------------------------------ */
/*  Hooked OutputString                                                */
/* ------------------------------------------------------------------ */
static EFI_STATUS EFIAPI
hooked_OutputString(void *this, CHAR16 *s)
{
	EFI_STATUS r = orig_OutStr(this, s);
	if (!EFI_ERROR(r) && roll_9() && injection_count < MAX_INJ) {
		const CHAR16 *put = pick_putin();
		CHAR16 *p = linebuf;
		p = cpy16(p, L"\r\n  \\_ "); p = cpy16(p, put);
		p = cpy16(p, L"\r\n"); *p = 0;
		conOut_OutStr(this, linebuf);
		injection_count++;
	}
	return r;
}

/* ------------------------------------------------------------------ */
/*  Entry point                                                        */
/* ------------------------------------------------------------------ */
EFI_STATUS EFIAPI
efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *systab)
{
	FN_CREATE_EVENT crate;
	FN_SET_TIMER    stimer;
	int i;

	gST = systab;
	gBS = systab->BootServices;
	gConOut = systab->ConOut;

	/* OutputString is at offset 8 in SimpleTextOutput protocol */
	conOut_OutStr = *(EFI_TEXT_STRING *)((UINT8 *)gConOut + 8);
	orig_OutStr = conOut_OutStr;
	(void)image;

	srand_efi((UINT64)(UINTN)gBS);

	efi_print(L"\r\n=== dmesg_injector_efi v0.2 (NET) ===\r\n");

	/* 1. Hook ConOut */
	*(EFI_TEXT_STRING *)((UINT8 *)gConOut + 8) = hooked_OutputString;
	efi_print(L"ConOut hooked\r\n");

	/* 2. Init network */
	init_network();
	if (networkUp) {
		efi_print(L"NIC found, UDP4 listening on port ");
		write_num(LISTEN_PORT);
		efi_print(numbuf);
		efi_print(L"\r\n");
	} else {
		efi_print(L"No NIC — using local fake messages\r\n");
	}

	/* 3. Burst of startup messages */
	for (i = 0; i < 10 && injection_count < MAX_INJ; i++) {
		print_driver_msg();
		injection_count++;
	}

	/* 4. Periodic timer every 5 seconds */
	crate = (FN_CREATE_EVENT)gBS->CreateEvent;
	stimer = (FN_SET_TIMER)gBS->SetTimer;
	if (crate && stimer) {
		EFI_STATUS st = crate(EVT_TIMER | EVT_NOTIFY_SIGNAL,
				     1, timer_cb, NULL, &gTimerEvent);
		if (!EFI_ERROR(st))
			stimer(gTimerEvent, TimerPeriodic, 50000000ULL);
	}

	efi_print(L"Timer active — returning to boot\r\n");
	efi_print(L"Messages from AI host on UDP ");
	write_num(LISTEN_PORT);
	efi_print(numbuf);
	efi_print(L" will be injected\r\n");

	return EFI_SUCCESS;
}
