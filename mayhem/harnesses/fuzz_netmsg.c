/* Fuzzes the decode step of cdogs' ENet multiplayer protocol, as both the server and the client
 * run it on every received packet (net_server.c / net_client.c OnReceive): the 4-byte message id
 * (NET_MSG_SIZE) picks the game-event entry (game_events.c GameEventGetEntry), and its nanopb
 * message (src/proto/msg.proto) is decoded from the rest of the packet (net_util.c NetDecode ->
 * vendored nanopb pb_decode). A packet comes from any peer on the network.
 *
 * Input layout: the first 4 bytes are the message id (zero-padded when the input is shorter),
 * the rest is the nanopb payload. The harness always builds a packet with a full 4-byte header
 * and reduces the id modulo the 55 entries of sGameEventEntries[]. No input is rejected: every
 * input is decoded, under the message type its id selects.
 *
 * Why: upstream's receive path has two framing bugs that crash on almost any mutated packet
 * before nanopb sees a byte, so libFuzzer could not get past its start-up iterations:
 *  - OnReceive reads the id with *(uint32_t *)packet->data and NetDecode passes
 *    dataLength - NET_MSG_SIZE to pb_istream_from_buffer, so a packet under 4 bytes is an
 *    over-read with a wrapped (huge) length (heap-buffer-overflow in pb_decode.c buf_read);
 *  - GameEventGetEntry indexes sGameEventEntries[] (55 entries for 56 enum values: ACTOR_BARK
 *    has none) with the raw id, so id >= 55 reads past the table.
 * Both are reported in the integration PR; this target fuzzes the nanopb message decoders behind
 * them. Ids 36-54 still get the entry of the id before them (the table skew), as upstream does.
 *
 * No state is kept across inputs: each packet decodes into a fresh, zeroed GameEvent. */
#define SDL_MAIN_HANDLED
#include <stdint.h>
#include <string.h>
#include <enet/enet.h>
#include <game_events.h>
#include <net_util.h>

/* sGameEventEntries[] in game_events.c holds one entry per id 0..GAME_EVENT_MISSION_END-1. */
#define NUM_ENTRIES ((uint32_t)GAME_EVENT_MISSION_END)

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	uint32_t msg = 0;
	memcpy(&msg, data, size < NET_MSG_SIZE ? size : NET_MSG_SIZE);
	msg %= NUM_ENTRIES;
	const size_t payload = size > NET_MSG_SIZE ? size - NET_MSG_SIZE : 0;

	ENetPacket *packet =
		enet_packet_create(NULL, NET_MSG_SIZE + payload, ENET_PACKET_FLAG_RELIABLE);
	if (packet == NULL)
		return 0;
	memcpy(packet->data, &msg, NET_MSG_SIZE);
	if (payload > 0)
		memcpy(packet->data + NET_MSG_SIZE, data + NET_MSG_SIZE, payload);

	const GameEventEntry gee = GameEventGetEntry((GameEventType)msg);
	if (gee.Fields != NULL)
	{
		GameEvent e = GameEventNew(gee.Type);
		NetDecode(packet, &e.u, gee.Fields);
	}
	enet_packet_destroy(packet);
	return 0;
}
