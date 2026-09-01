# CA2 Ricochet - Dogfight Game
- LAN multiplayer game using TCP as primary network protocol
- Up to 15 player, connected at the same time.
- Physics for each player and every projectile type
- Strictly Online Only, no local play with this version.
## Multiplayer Networking
- Game has strict control over player movement with clients having their own copy of position and veloctiy while server attempts to correct positions, fixed at 60Hz.
- Broadcasts the aircraft at 20 Hz per snapshot.
- Local aircraft apply input immediately through the game server.
- Create aircraft interpolation for smoother motion across the network.
- Sends bits whenever action is called instead of every frame.
## Packet Structure
### kUpdateClientState
| Field          | Data Type | Size (Bytes) | Description                              |
|----------------|-----------|--------------|------------------------------------------|
| Packet type    | `uint8_t` | 1 byte       | `Server::PacketType::kUpdateClientState` |
| Aircraft count | `uint8_t` | 1 byte       | Number of aircraft in the snapshot       |
| Aircraft ID    | `uint8_t` | 1 byte       | Unique aircraft identifier               |
| Position X     | `float`   | 4 bytes      | Authoritative X coordinate               |
| Position Y     | `float`   | 4 bytes      | Authoritative Y coordinate               |
| Rotation       | `float`   | 4 bytes      | Authoritative heading in degrees         |

- Per aircraft its 13 bytes so 13 x 15 = 197 bytes.
### Client to Server: kInputCommand
- Input state is compressed into a bitfield and sent only when its changes.

 | Field          | Data Type | Size (Bytes) | Description                          |
|----------------|-----------|--------------|--------------------------------------|
| Packet type    | `uint8_t` | 1 byte       | `Client::PacketType::kInputCommand`  |
| Aircraft ID    | `uint8_t` | 1 byte       | Aircraft controlled by the sender    |
| Input sequence | `uint8_t` | 1 byte       | Sequence number for the input change |
| Input flags    | `uint8_t` | 1 byte       | Bit-packed input actions             |

### Initial State
- When a client connects, the server sends an initial snapshot containing each existing aircraft's ID, position, health, and missile ammunition.
- | Field          | Data Type | Size                 |
|----------------|-----------|----------------------|
| Packet type    | `uint8_t` | 1 byte               |
| Aircraft count | `uint8_t` | 1 byte               |
| Aircraft ID    | `uint8_t` | 1 byte per aircraft  |
| Position X     | `float`   | 4 bytes per aircraft |
| Position Y     | `float`   | 4 bytes per aircraft |
| Hitpoints      | `uint8_t` | 1 byte per aircraft  |
| Missile ammo   | `uint8_t` | 1 byte per aircraft  |

## Bandwidth Estimate
### Server Upload: Movement Snapshots
| Players | One Snapshot | Server Upload | Approx. Bitrate |
|---------|--------------|---------------|-----------------|
| 2       | 28 B         | 1,120 B/s     | 0.009 Mbps      |
| 8       | 106 B        | 16,960 B/s    | 0.136 Mbps      |
| 15      | 197 B        | 59,100 B/s    | 0.473 Mbps      |

### Server Download: Input Commands
| Players | Upper-Bound Input Payload | Approx. Bitrate |
|---------|---------------------------|-----------------|
| 8       | 1,920 B/s                 | 0.015 Mbps      |
| 15      | 3,600 B/s                 | 0.029 Mbps      |

## Optimisation Work
- Compact identifiers and counter, this involves Aircraft ID's, health, ammunition, packet types and input sequences are all uint8_t.
- Bit packed input: Used to store five player actions inside a single uint8_t, reducing input-state data from five bytes to one byte.
- Event batching: Pickup spawns and removals share packet headers.

## Possible Improvements
- <b> Synchronize combat state: </b> Add health, missile ammo, fire-rate level, and spread level to an authoritative gameplay snapshot or reliable change events. The current regular snapshot only transmits ID, position, and rotation, even though older protocol comments describe additional fields.
- <b> Make combat fully server-authoritative: </b>Validate projectile hits, damage, deaths, respawns, pickups, and scores on the server. This would reduce divergence between clients and make cheating more difficult.
