#pragma once
#include <SFML/System/Vector2.hpp>
#include <SFML/Network/TcpSocket.hpp>
#include <SFML/Network/TcpListener.hpp>
#include <SFML/System/Clock.hpp>
#include <SFML/Graphics/Rect.hpp>
#include <thread>
#include <cstdint>
#include <map>
#include <set>
#include <memory>
#include <mutex>
#include "network_stats.hpp"

class GameServer
{
public:
	explicit GameServer(sf::Vector2f battlefield_size);
	~GameServer();
	void NotifyPlayerSpawn(uint8_t aircraft_identifier);
	void NotifyPlayerRealtimeChange(uint8_t aircraft_identifier, uint8_t action, bool action_enabled);
	void NotifyPlayerEvent(uint8_t aircraft_identifier, int8_t action);
	void NotifyPickupSpawn(uint32_t pickup_id, int pickup_type, sf::Vector2f position);
	void NotifyPickupCollected(uint32_t pickup_id);
	NetworkStats GetNetworkStats() const;
	std::vector<uint8_t> GetAndClearRecentlyDisconnectedAircraft();

private:
	struct RemotePeer
	{
		RemotePeer();
		sf::TcpSocket m_socket;
		sf::Time m_last_packet_time;
		std::vector<uint8_t> m_aircraft_identifiers;
		bool m_ready;
		bool m_timed_out;
	};

	struct AircraftInfo
	{
		sf::Vector2f m_position;
		float m_rotation = 0.f;
		uint8_t m_hitpoints;
		uint8_t m_missile_ammo;
		std::map<uint8_t, bool> m_real_time_actions;

		// Authoritative movement simulation state
		sf::Vector2f m_velocity = sf::Vector2f(0.f, 0.f);
		sf::Time m_forward_time = sf::Time::Zero;          // accumulated while forward is held
		sf::Time m_release_time = sf::Time::Zero;          // accumulated since forward was released
		sf::Vector2f m_velocity_at_release = sf::Vector2f(0.f, 0.f);
		bool m_was_forward_pressed = false;
	};

	typedef std::unique_ptr<RemotePeer> PeerPtr;

private:
	void SetListening(bool enable);
	void ExecutionThread();
	void SimulateMovement(sf::Time dt);
	void Tick();
	sf::Time Now() const;

	void HandleIncomingPackets();
	void HandleIncomingPackets(sf::Packet& packet, RemotePeer& receiving_peer, bool& detected_timeout);

	void HandleIncomingConnections();
	void HandleDisconnections();

	void InformWorldState(sf::TcpSocket& socket);
	void BroadcastMessage(const std::string& message);
	void SendToAll(sf::Packet& packet);
	void SendToPeer(RemotePeer& peer, sf::Packet& packet);
	void UpdateClientState();

private:	
	sf::Clock m_clock;
	sf::TcpListener m_listener_socket;
	bool m_listening_state;
	sf::Time m_client_timeout;

	std::size_t m_max_connected_players;
	std::size_t m_connected_players;

	sf::FloatRect m_battlefield_rect;

	std::size_t m_aircraft_count;
	std::map<uint8_t, AircraftInfo> m_aircraft_info;

	std::vector<PeerPtr> m_peers;
	std::set<uint8_t> m_available_aircraft_ids;
	bool m_waiting_thread_end = false;
	mutable std::recursive_mutex m_state_mutex;

	sf::Time m_last_spawn_time;
	sf::Time m_time_for_next_spawn;

	// Network update rate: send state every 2 ticks (10 Hz network, 20 Hz simulation)
	int m_network_tick_counter;

	// Bandwidth monitoring
	size_t m_total_bytes_sent;
	sf::Clock m_bandwidth_clock;

	// Network statistics tracking
	mutable std::mutex m_stats_mutex;
	uint64_t m_packets_sent = 0;
	uint64_t m_packets_received = 0;

	// Track recently disconnected aircraft for host-side GUI cleanup
	std::vector<uint8_t> m_recently_disconnected_aircraft;

	// Packet batching for pickups - accumulate spawns and send in batches
	struct PickupSpawnData
	{
		uint32_t pickup_id;
		int32_t pickup_type;
		float x, y;
	};
	std::vector<PickupSpawnData> m_batched_pickup_spawns;
	std::vector<uint32_t> m_batched_pickup_despawns;
	void FlushPickupBatch();
	void FlushPickupDespawns();

	std::thread m_thread;
};