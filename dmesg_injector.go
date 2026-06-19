package main

import (
	"bufio"
	"fmt"
	"math/rand"
	"net"
	"os"
	"os/signal"
	"strconv"
	"sync"
	"syscall"
	"time"
)

// ---------------------------------------------------------------------------
//  Data
// ---------------------------------------------------------------------------

var putinisms = []string{
	"reminds me of tagging a Siberian tiger in Kamchatka",
	"key to good firmware is regular hockey practice",
	"we drilled for this in the taiga last summer",
	"solved this bug while fishing on Lake Baikal",
	"strength comparable to Russian birch",
	"even Amur leopards handle edge cases better",
	"my personal trainer agrees this DMA allocation is optimal",
	"this init is as vast as the Russian taiga",
	"smooth as my figure skating routine",
	"precision matches my biathlon marksmanship",
	"I studied these registers while piloting a submarine",
	"reminds me of tranquilizing a polar bear",
	"stress-tested this driver while climbing Elbrus",
	"worthy of a standing ovation at the Bolshoi",
	"my falcon and I agree this driver is excellent",
	"siberian cranes migrate more efficiently than this allocator",
	"this handoff is worthy of the Bolshoi Ballet",
	"not bad for a judo warmup routine",
}

var drivers = []string{
	"e1000e", "igb", "r8169", "ath9k", "iwlwifi",
	"snd_hda_intel", "nvidia", "amdgpu", "nouveau", "i915",
	"btrfs", "ext4", "xfs", "nvme", "ahci",
	"xhci_hcd", "usbhid", "evdev", "kvm_intel", "fuse",
	"virtio_net", "virtio_pci", "dm_mod", "tun", "bridge",
	"bluetooth", "btusb", "cfg80211", "mac80211",
	"intel_pstate", "intel_rapl", "rtc_cmos", "sd_mod",
}

// ---------------------------------------------------------------------------
//  Random generators
// ---------------------------------------------------------------------------

var rng = rand.New(rand.NewSource(time.Now().UnixNano()))

func randRange(lo, hi int) int { return lo + rng.Intn(hi-lo+1) }

func rollPct(pct int) bool { return rng.Intn(100) < pct }

func randPCI() string {
	return fmt.Sprintf("%04x:%02x:%02x.%x",
		0, randRange(0, 31), randRange(0, 31), randRange(0, 7))
}

var ioBases = []int{0x3f8, 0x2f8, 0x378, 0x278, 0xc00, 0xd00, 0xe00,
	0x1400, 0x1800, 0x2000, 0x3000, 0x4000, 0x6000, 0x8000, 0xc000}
var ioSizes = []int{0x8, 0x10, 0x20, 0x40, 0x80, 0x100, 0x200}

func randIO() string {
	start := ioBases[rng.Intn(len(ioBases))]
	size := ioSizes[rng.Intn(len(ioSizes))]
	return fmt.Sprintf("0x%04x-0x%04x", start, start+size-1)
}

var memBases = []uint64{0xf0000000, 0xfeb00000, 0xfed00000, 0xfed80000,
	0xd0000000, 0xe0000000, 0xf7c00000, 0xc0000000, 0xa0000000,
	0xdfc00000, 0x7c000000, 0xfe000000}
var memSizes = []uint64{0x10000, 0x20000, 0x40000, 0x80000,
	0x100000, 0x200000, 0x400000, 0x1000000}

func randMEM() string {
	base := memBases[rng.Intn(len(memBases))]
	off := memSizes[rng.Intn(len(memSizes))]
	size := memSizes[rng.Intn(len(memSizes))]
	start := base + off
	return fmt.Sprintf("0x%x-0x%x", start, start+size-1)
}

func randDriver() string { return drivers[rng.Intn(len(drivers))] }

func randPutinism() string { return putinisms[rng.Intn(len(putinisms))] }

// ---------------------------------------------------------------------------
//  Fake message builder
// ---------------------------------------------------------------------------

func buildFakeMsg() string {
	pci := randPCI()
	io := randIO()
	mem := randMEM()
	drv := randDriver()
	err := randRange(2, 61)
	irq := randRange(1, 255)

	// pretend to be a kernel message with timestamp prefix removed
	var line string
	switch rng.Intn(15) {
	case 0:
		line = fmt.Sprintf("%s: probe of %s succeeded", drv, pci)
	case 1:
		line = fmt.Sprintf("%s: probe of %s failed with error -%d", drv, pci, err)
	case 2:
		line = fmt.Sprintf("%s: device %s registered", drv, pci)
	case 3:
		line = fmt.Sprintf("%s: BAR0 %s", drv, mem)
	case 4:
		line = fmt.Sprintf("%s: BAR1 I/O %s", drv, io)
	case 5:
		line = fmt.Sprintf("%s: DMA mask set to %d-bit", drv, randRange(32, 64))
	case 6:
		line = fmt.Sprintf("%s: resetting %s", drv, pci)
	case 7:
		line = fmt.Sprintf("%s: firmware loaded for %s", drv, pci)
	case 8:
		line = fmt.Sprintf("%s: interrupt enabled on %s (irq %d)", drv, pci, irq)
	case 9:
		line = fmt.Sprintf("%s: PM OK for %s", drv, pci)
	case 10:
		line = fmt.Sprintf("%s: link up on %s", drv, pci)
	case 11:
		line = fmt.Sprintf("%s: thermal throttling on %s", drv, pci)
	case 12:
		line = fmt.Sprintf("%s: microcode updated on %s", drv, pci)
	case 13:
		line = fmt.Sprintf("%s: Tx descriptor ring at %s", drv, mem)
	case 14:
		line = fmt.Sprintf("%s: Rx DMA %s mapped", drv, mem)
	}

	if rollPct(9) {
		line += " -- " + randPutinism()
	}
	return line
}

// ---------------------------------------------------------------------------
//  /dev/kmsg writer
// ---------------------------------------------------------------------------

var kmsgMu sync.Mutex
var kmsgFd *os.File

func openKmsg() error {
	kmsgMu.Lock()
	defer kmsgMu.Unlock()
	if kmsgFd != nil {
		return nil
	}
	var err error
	kmsgFd, err = os.OpenFile("/dev/kmsg", os.O_WRONLY, 0644)
	return err
}

func closeKmsg() {
	kmsgMu.Lock()
	defer kmsgMu.Unlock()
	if kmsgFd != nil {
		kmsgFd.Close()
		kmsgFd = nil
	}
}

// writeToKmsg writes a line to /dev/kmsg with KERN_INFO priority.
// The kernel prepends a timestamp automatically.
func writeToKmsg(msg string) {
	kmsgMu.Lock()
	defer kmsgMu.Unlock()
	if kmsgFd == nil {
		return
	}
	// Kernel log level <6> = KERN_INFO
	_, _ = kmsgFd.WriteString("<6>" + msg + "\n")
}

// ---------------------------------------------------------------------------
//  Message injector — writes periodic fake messages
// ---------------------------------------------------------------------------

func injectWorker(stopCh <-chan struct{}, interval time.Duration,
	burst int, networkMsgCh chan string) {
	ticker := time.NewTicker(interval)
	defer ticker.Stop()

	// Initial burst
	for i := 0; i < burst; i++ {
		writeToKmsg(buildFakeMsg())
	}

	for {
		select {
		case <-stopCh:
			return
		case <-ticker.C:
			writeToKmsg(buildFakeMsg())
		case msg := <-networkMsgCh:
			writeToKmsg("[AI] " + msg)
		}
	}
}

// ---------------------------------------------------------------------------
//  AI host connection — receives messages from remote server
// ---------------------------------------------------------------------------

func aiHostWorker(stopCh <-chan struct{}, addr string,
	networkMsgCh chan<- string) {
	for {
		select {
		case <-stopCh:
			return
		default:
		}

		conn, err := net.DialTimeout("tcp", addr, 5*time.Second)
		if err != nil {
			select {
			case <-stopCh:
				return
			case <-time.After(5 * time.Second):
			}
			continue
		}

		scanner := bufio.NewScanner(conn)
		for scanner.Scan() {
			line := scanner.Text()
			select {
			case networkMsgCh <- line:
			case <-stopCh:
				conn.Close()
				return
			default:
			}
		}
		conn.Close()

		select {
		case <-stopCh:
			return
		case <-time.After(3 * time.Second):
		}
	}
}

// ---------------------------------------------------------------------------
//  Self-destruct — delete binary and exit
// ---------------------------------------------------------------------------

func selfDestruct(selfPath string) {
	// Delete our own binary
	os.Remove(selfPath)

	// Also try to wipe the path from /proc/self/exe
	os.Remove("/proc/self/exe")

	// Spawn a background goroutine that waits a moment then exits hard
	go func() {
		time.Sleep(500 * time.Millisecond)
		closeKmsg()
		os.Exit(0)
	}()
}

// ---------------------------------------------------------------------------
//  Main
// ---------------------------------------------------------------------------

func main() {
	aiHost := os.Getenv("AI_HOST")
	if aiHost == "" {
		aiHost = "10.0.2.2:4444"
	}
	selfPath, _ := os.Executable()
	if selfPath == "" {
		selfPath = "/proc/self/exe"
	}

	// Parse optional flags
	burst := 25
	if b := os.Getenv("INJECT_BURST"); b != "" {
		if v, err := strconv.Atoi(b); err == nil && v > 0 {
			burst = v
		}
	}
	lifespan := 90
	if ls := os.Getenv("INJECT_LIFESPAN"); ls != "" {
		if v, err := strconv.Atoi(ls); err == nil && v > 0 {
			lifespan = v
		}
	}

	// Open /dev/kmsg
	if err := openKmsg(); err != nil {
		fmt.Fprintf(os.Stderr, "open /dev/kmsg: %v (try sudo)\n", err)
		os.Exit(1)
	}
	defer closeKmsg()

	stopCh := make(chan struct{})
	netMsgCh := make(chan string, 64)

	// Start injector worker
	go injectWorker(stopCh, 5*time.Second, burst, netMsgCh)

	// Start AI host connection
	go aiHostWorker(stopCh, aiHost, netMsgCh)

	fmt.Printf("dmesg_injector — writing to /dev/kmsg every 5s (burst=%d, lifespan=%ds)\n",
		burst, lifespan)
	fmt.Printf("AI host: %s\n", aiHost)

	// Handle signals
	sigCh := make(chan os.Signal, 1)
	signal.Notify(sigCh, syscall.SIGINT, syscall.SIGTERM, syscall.SIGQUIT)

	// Auto self-destruct after lifespan seconds
	timer := time.NewTimer(time.Duration(lifespan) * time.Second)

	select {
	case <-timer.C:
		fmt.Println("lifespan expired — self-destructing")
		close(stopCh)
		selfDestruct(selfPath)
		// wait for cleanup
		time.Sleep(1 * time.Second)
	case sig := <-sigCh:
		fmt.Printf("signal %v — exiting\n", sig)
		close(stopCh)
		time.Sleep(200 * time.Millisecond)
	}
}
