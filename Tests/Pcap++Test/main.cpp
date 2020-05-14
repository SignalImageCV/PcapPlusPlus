#include <stdio.h> 
#include <stdlib.h>
#include "PcapLiveDeviceList.h"
#include <getopt.h>
#include "PcapPlusPlusVersion.h"
#include "Logger.h"
#include "../PcppTestFramework/PcppTestFrameworkRun.h"
#include "TestDefinition.h"
#include "Common/GlobalTestArgs.h"

static struct option PcapTestOptions[] =
{
	{"debug-mode", no_argument, 0, 'b'},
	{"use-ip",  required_argument, 0, 'i'},
	{"remote-ip", required_argument, 0, 'r'},
	{"remote-port", required_argument, 0, 'p'},
	{"dpdk-port", required_argument, 0, 'd' },
	{"no-networking", no_argument, 0, 'n' },
	{"verbose", no_argument, 0, 'v' },
	{"mem-verbose", no_argument, 0, 'm' },
	{"kni-ip", no_argument, 0, 'k' },
	{"skip-mem-leak-check", no_argument, 0, 's' },
	{"tags",  required_argument, 0, 't'},
	{"show-skipped-tests", no_argument, 0, 'w' },
	{"help", no_argument, 0, 'h'},
	{0, 0, 0, 0}
};


void printUsage()
{
	printf("Usage: Pcap++Test -i ip_to_use | [-n] [-b] [-s] [-m] [-r remote_ip_addr] [-p remote_port] [-d dpdk_port] [-k ip_addr] [-t tags] [-h]\n\n"
				"Flags:\n"
				"-i --use-ip              IP to use for sending and receiving packets\n"
				"-b --debug-mode          Set log level to DEBUG\n"
				"-r --remote-ip	          IP of remote machine running rpcapd to test remote capture\n"
				"-p --remote-port         Port of remote machine running rpcapd to test remote capture\n"
				"-d --dpdk-port           The DPDK NIC port to test. Required if compiling with DPDK\n"
				"-n --no-networking       Do not run tests that requires networking\n"
				"-v --verbose             Run in verbose mode (emits more output in several tests)\n"
				"-m --mem-verbose         Output information about each memory allocation and deallocation\n"			
				"-s --skip-mem-leak-check Skip memory leak check\n"
				"-k --kni-ip              IP address for KNI device tests to use must not be the same\n"
				"                         as any of existing network interfaces in your system.\n"
				"                         If this parameter is omitted KNI tests will be skipped. Must be an IPv4.\n"
				"                         For Linux systems only\n"
				"-t --tags                A list of semicolon separated tags for tests to run\n"
				"-w --show-skipped-tests  Show tests that are skipped. Default is to hide them in tests results\n"
				"-h --help                Display this help message and exit\n"
	);
}

PcapTestArgs PcapTestGlobalArgs;

int main(int argc, char* argv[])
{
	PcapTestGlobalArgs.ipToSendReceivePackets = "";
	PcapTestGlobalArgs.debugMode = false;
	PcapTestGlobalArgs.dpdkPort = -1;
	PcapTestGlobalArgs.kniIp = "";

	std::string userTags = "", configTags = "";
	bool runWithNetworking = true;
	bool memVerbose = false;
	bool skipMemLeakCheck = false;

	int optionIndex = 0;
	char opt = 0;
	while((opt = getopt_long(argc, argv, "k:i:br:p:d:nvt:sm", PcapTestOptions, &optionIndex)) != -1)
	{
		switch (opt)
		{
			case 0:
				break;
			case 'k':
				PcapTestGlobalArgs.kniIp = optarg;
				break;
			case 'i':
				PcapTestGlobalArgs.ipToSendReceivePackets = optarg;
				break;
			case 'b':
				PcapTestGlobalArgs.debugMode = true;
				break;
			case 'r':
				PcapTestGlobalArgs.remoteIp = optarg;
				break;
			case 'p':
				PcapTestGlobalArgs.remotePort = (uint16_t)atoi(optarg);
				break;
			case 'd':
				PcapTestGlobalArgs.dpdkPort = (int)atoi(optarg);
				break;
			case 'n':
				runWithNetworking = false;
				break;
			case 'v':
				PTF_SET_VERBOSE_MODE(true);
				break;
			case 't':
				userTags = optarg;
				break;
			case 's':
				skipMemLeakCheck = true;
				break;
			case 'm':
				memVerbose = true;
				break;
			case 'h':
				printUsage();
				exit(0);
			default:
				printUsage();
				exit(-1);
		}
	}

	if (!runWithNetworking)
	{
		if (userTags != "")
			userTags += ";";

		userTags += "no_network";
		printf("Running only tests that don't require network connection\n");
	}
	else if (PcapTestGlobalArgs.ipToSendReceivePackets == "")
	{
		printf("Please provide an IP address to send and receive packets (-i argument)\n\n");
		printUsage();
		exit(-1);
	}
	
	#ifdef NDEBUG
	skipMemLeakCheck = true;
	printf("Disabling memory leak check in MSVC Release builds due to caching logic in stream objects that looks like a memory leak:\n");
	printf("     https://github.com/cpputest/cpputest/issues/786#issuecomment-148921958\n");
	#endif

	if (skipMemLeakCheck)
	{
		if (configTags != "")
			configTags += ";";

		configTags += "skip_mem_leak_check";
		printf("Skipping memory leak check for all test cases\n");
	}

	if (memVerbose)
	{
		if (configTags != "")
			configTags += ";";

		configTags += "mem_leak_check_verbose";
		printf("Turning on verbose information on memory allocations\n");
	}

#ifdef USE_DPDK
	if (PcapTestGlobalArgs.dpdkPort == -1 && runWithNetworking)
	{
		printf("When testing with DPDK you must provide the DPDK NIC port to test\n\n");
		printUsage();
		exit(-1);
	}
#endif // USE_DPDK

	if (PcapTestGlobalArgs.debugMode)
	{
		pcpp::LoggerPP::getInstance().setAllModlesToLogLevel(pcpp::LoggerPP::Debug);
	}

	printf("PcapPlusPlus version: %s\n", pcpp::getPcapPlusPlusVersionFull().c_str());
	printf("Built: %s\n", pcpp::getBuildDateTime().c_str());
	printf("Git info: %s\n", pcpp::getGitInfo().c_str());
	printf("Using ip: %s\n", PcapTestGlobalArgs.ipToSendReceivePackets.c_str());
	printf("Debug mode: %s\n", PcapTestGlobalArgs.debugMode ? "on" : "off");

#ifdef USE_DPDK
	if (runWithNetworking)
	{
		printf("Using DPDK port: %d\n", PcapTestGlobalArgs.dpdkPort);
		if (PcapTestGlobalArgs.kniIp == "")
			printf("DPDK KNI tests: skipped\n");
		else
			printf("Using IP address for KNI: %s\n", PcapTestGlobalArgs.kniIp.c_str());
	}
#endif

	char errString[1000];

	PcapTestGlobalArgs.errString = errString;

	PTF_START_RUNNING_TESTS(userTags, configTags);

	pcpp::PcapLiveDeviceList::getInstance();

	PTF_RUN_TEST(TestIPAddress, "no_network;ip");
	PTF_RUN_TEST(TestMacAddress, "no_network;mac");
	PTF_RUN_TEST(TestLRUList, "no_network");
	PTF_RUN_TEST(TestGeneralUtils, "no_network");
	PTF_RUN_TEST(TestGetMacAddress, "mac");

	PTF_RUN_TEST(TestPcapFileReadWrite, "no_network;pcap");
	PTF_RUN_TEST(TestPcapSllFileReadWrite, "no_network;pcap");
	PTF_RUN_TEST(TestPcapRawIPFileReadWrite, "no_network;pcap");
	PTF_RUN_TEST(TestPcapFileAppend, "no_network;pcap");
	PTF_RUN_TEST(TestPcapNgFileReadWrite, "no_network;pcap;pcapng");
	PTF_RUN_TEST(TestPcapNgFileReadWriteAdv, "no_network;pcap;pcapng");

	PTF_RUN_TEST(TestPcapLiveDeviceList, "no_network;live_device;skip_mem_leak_check");
	PTF_RUN_TEST(TestPcapLiveDeviceListSearch, "live_device");
	PTF_RUN_TEST(TestPcapLiveDevice, "live_device");
	PTF_RUN_TEST(TestPcapLiveDeviceNoNetworking, "no_network;live_device");
	PTF_RUN_TEST(TestPcapLiveDeviceStatsMode, "live_device");
	PTF_RUN_TEST(TestPcapLiveDeviceBlockingMode, "live_device");
	PTF_RUN_TEST(TestPcapLiveDeviceSpecialCfg, "live_device");
	PTF_RUN_TEST(TestWinPcapLiveDevice, "live_device;winpcap");
	PTF_RUN_TEST(TestSendPacket, "live_device;send");
	PTF_RUN_TEST(TestSendPackets, "live_device;send");
	PTF_RUN_TEST(TestRemoteCapture, "live_device;remote_capture;winpcap");

	PTF_RUN_TEST(TestPcapFiltersLive, "filters");
	PTF_RUN_TEST(TestPcapFilters_General_BPFStr, "no_network;filters;skip_mem_leak_check");
	PTF_RUN_TEST(TestPcapFiltersOffline, "no_network;filters");

	PTF_RUN_TEST(TestHttpRequestParsing, "no_network;http");
	PTF_RUN_TEST(TestHttpResponseParsing, "no_network;http");
	PTF_RUN_TEST(TestPrintPacketAndLayers, "no_network;print");
	PTF_RUN_TEST(TestDnsParsing, "no_network;dns");

	// PTF_RUN_TEST(TestPfRingDevice, "pf_ring");
	// PTF_RUN_TEST(TestPfRingDeviceSingleChannel, "pf_ring");
	// PTF_RUN_TEST(TestPfRingMultiThreadAllCores, "pf_ring");
	// PTF_RUN_TEST(TestPfRingMultiThreadSomeCores, "pf_ring");
	// PTF_RUN_TEST(TestPfRingSendPacket, "pf_ring");
	// PTF_RUN_TEST(TestPfRingSendPackets, "pf_ring");
	// PTF_RUN_TEST(TestPfRingFilters, "pf_ring");
	// PTF_RUN_TEST(TestDpdkInitDevice, "dpdk;dpdk-init;skip_mem_leak_check");
	// PTF_RUN_TEST(TestDpdkDevice, "dpdk");
	// PTF_RUN_TEST(TestDpdkMultiThread, "dpdk");
	// PTF_RUN_TEST(TestDpdkDeviceSendPackets, "dpdk");
	// PTF_RUN_TEST(TestKniDevice, "dpdk;kni");
	// PTF_RUN_TEST(TestKniDeviceSendReceive, "dpdk;kni");
	// PTF_RUN_TEST(TestDpdkMbufRawPacket, "dpdk");
	// PTF_RUN_TEST(TestDpdkDeviceWorkerThreads, "dpdk");

	PTF_RUN_TEST(TestTcpReassemblySanity, "no_network;tcp_reassembly");
	PTF_RUN_TEST(TestTcpReassemblyRetran, "no_network;tcp_reassembly");
	PTF_RUN_TEST(TestTcpReassemblyMissingData, "no_network;tcp_reassembly");
	PTF_RUN_TEST(TestTcpReassemblyOutOfOrder, "no_network;tcp_reassembly");
	PTF_RUN_TEST(TestTcpReassemblyWithFIN_RST, "no_network;tcp_reassembly");
	PTF_RUN_TEST(TestTcpReassemblyMalformedPkts, "no_network;tcp_reassembly");
	PTF_RUN_TEST(TestTcpReassemblyMultipleConns, "no_network;tcp_reassembly");
	PTF_RUN_TEST(TestTcpReassemblyIPv6, "no_network;tcp_reassembly");
	PTF_RUN_TEST(TestTcpReassemblyIPv6MultConns, "no_network;tcp_reassembly");
	PTF_RUN_TEST(TestTcpReassemblyIPv6_OOO, "no_network;tcp_reassembly");
	PTF_RUN_TEST(TestTcpReassemblyCleanup, "no_network;tcp_reassembly");
	PTF_RUN_TEST(TestTcpReassemblyMaxSeq, "no_network;tcp_reassembly");

	PTF_RUN_TEST(TestIPFragmentationSanity, "no_network;ip_frag");
	PTF_RUN_TEST(TestIPFragOutOfOrder, "no_network;ip_frag");
	PTF_RUN_TEST(TestIPFragPartialData, "no_network;ip_frag");
	PTF_RUN_TEST(TestIPFragMultipleFrags, "no_network;ip_frag");
	PTF_RUN_TEST(TestIPFragMapOverflow, "no_network;ip_frag");
	PTF_RUN_TEST(TestIPFragRemove, "no_network;ip_frag");

	PTF_RUN_TEST(TestRawSockets, "raw_sockets");

	PTF_END_RUNNING_TESTS;
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif
