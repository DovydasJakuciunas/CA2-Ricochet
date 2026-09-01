#pragma once
#include <cstdint>
#include <string>
#include <sstream>
#include <iomanip>

struct NetworkStats
{
	uint64_t packets_sent = 0;
	uint64_t packets_received = 0;
	uint64_t bytes_sent = 0;
	uint64_t bytes_received = 0;
	uint32_t connected_players = 0;

	// Helper function to format large numbers with K/M suffix
	static std::string FormatBytes(uint64_t bytes)
	{
		std::stringstream ss;
		if (bytes >= 1048576) // 1 MB
		{
			ss << std::fixed << std::setprecision(2) << (bytes / 1048576.0) << " MB";
		}
		else if (bytes >= 1024) // 1 KB
		{
			ss << std::fixed << std::setprecision(2) << (bytes / 1024.0) << " KB";
		}
		else
		{
			ss << bytes << " B";
		}
		return ss.str();
	}

	std::string ToString() const
	{
		std::stringstream ss;
		ss << "=== Network Statistics ===\n";
		ss << "Connected Players: " << connected_players << "\n";
		ss << "Packets Sent: " << packets_sent << "\n";
		ss << "Packets Received: " << packets_received << "\n";
		ss << "Bytes Sent: " << FormatBytes(bytes_sent) << "\n";
		ss << "Bytes Received: " << FormatBytes(bytes_received) << "\n";
		return ss.str();
	}
};
