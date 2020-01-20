#pragma once

#include <memory>
#include <unordered_map>
#include <string>
#include <string_view>

#include "PacketReader.hpp"
#include "explints.hpp"

#include "InputManager.hpp"
#include "World.hpp"
#include "SelfCursor.hpp"
#include "User.hpp"

// UBSan complains that the class is not aligned to 16-bytes
// when constructing it with make_unique...
class alignas(32) Client {
public:
	static constexpr double ticksPerSec = 20.0;

private:
	InputManager im;
	InputAdapter& aClient;
	PacketReader pr;
	std::unordered_map<User::Id, User> users;
	std::unique_ptr<World> world;
	std::unique_ptr<SelfCursor> preJoinSelfCursorData;
	User::Id selfUid;
	u32 globalCursorCount;
	long tickTimer;

	std::shared_ptr<ImAction> iTest;

public:
	Client();
	~Client();

	bool open(std::string_view worldToJoin);
	void close();

	bool freeMemory();

private:
	void registerPacketTypes();

	void tick();

	void wsOpen();
	void wsClose(u16);
	void wsMessage(const char *, sz_t, bool);

	static void doWsOpen(void *);
	static void doWsClose(void *, u16);
	static void doWsMessage(void *, char *, sz_t, bool);

	static void doTick(void *);
};
