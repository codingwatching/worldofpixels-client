#include "Chunk.hpp"

#include <cstdio>

#include <emscripten.h>
#include <GLES2/gl2.h>

#include "rle.hpp"

#include "World.hpp"

static_assert((Chunk::size & (Chunk::size - 1)) == 0,
	"Chunk::size must be a power of 2");

static_assert((Chunk::protectionAreaSize & (Chunk::protectionAreaSize - 1)) == 0,
	"Chunk::protectionAreaSize must be a power of 2");

static_assert((Chunk::size % Chunk::protectionAreaSize) == 0,
	"Chunk::size must be divisible by Chunk::protectionAreaSize");

static_assert((Chunk::pc & (Chunk::pc - 1)) == 0,
	"size / protectionAreaSize must result in a power of 2");

EM_JS(void, actually_abort_async_wget2, (int hdl), {
	var http = Browser.wgetRequests[hdl];
	if (http) {
		http.onload = null;
		http.onerror = null;
		http.onprogress = null;
		http.onabort = null;
		http.abort();
		delete Browser.wgetRequests[hdl];
	}
});

static void destroyWget(void * e) {
	if (e) {
		actually_abort_async_wget2(reinterpret_cast<int>(e));
		//std::printf("Cancelled chunk request\n");
	}
}

Chunk::Chunk(Pos x, Pos y, World& w)
: w(w),
  x(x),
  y(y),
  loaderRequest(nullptr, destroyWget),
  texHdl(0),
  canUnload(false),
  protectionsLoaded(false) { // to check if I need to 0-fill the protection array
	bool readerCalled = false;

	data.setChunkReader("woPp", [this] (u8 * d, sz_t size) {
		protectionsLoaded = rle::decompress(d, size, protectionData.data(), protectionData.size());
		return true;
	});

	loaderRequest.reset(reinterpret_cast<void *>(emscripten_async_wget2_data(
			w.getChunkUrl(x, y), "GET", nullptr, this, true,
			Chunk::loadCompleted, Chunk::loadFailed, nullptr)));

	preventUnloading(false);

	std::printf("[Chunk] Created (%i, %i)\n", x, y);
}

Chunk::~Chunk() {
	std::puts("[Chunk] Destroyed");
	deleteTexture();
	w.signalChunkUnloaded(*this);
}

Chunk::Pos Chunk::getX() const {
	return x;
}

Chunk::Pos Chunk::getY() const {
	return y;
}

bool Chunk::setPixel(u16 x, u16 y, RGB_u clr) {
	x &= Chunk::size - 1;
	y &= Chunk::size - 1;

	if (data.getPixel(x, y).rgb != clr.rgb) {
		data.setPixel(x, y, clr);
		// TODO: send setPixel packet
		return true;
	}

	return false;
}

void Chunk::setProtectionGid(ProtPos x, ProtPos y, u32 gid) {
	x &= Chunk::pc - 1;
	y &= Chunk::pc - 1;

	protectionData[y * Chunk::pc + x] = gid;
	// TODO: send setProtection packet
}

u32 Chunk::getProtectionGid(ProtPos x, ProtPos y) const {
	x &= Chunk::pc - 1;
	y &= Chunk::pc - 1;

	return protectionData[y * Chunk::pc + x];
}

const u8 * Chunk::getData() const {
	return data.getData();
}

bool Chunk::isReady() const {
	// if no request pending, it means the chunk has been loaded (or failed to)
	return !loaderRequest;
}

bool Chunk::shouldUnload() const {
	// request aborting doesn't always work
	return canUnload && isReady();
}

void Chunk::preventUnloading(bool state) {
	canUnload = !state;
}

u32 Chunk::getGlTexture() {
	if (!texHdl) {
		if (const u8 * d = data.getData()) {
			glActiveTexture(GL_TEXTURE0);
			glGenTextures(1, &texHdl);
			glBindTexture(GL_TEXTURE_2D, texHdl);

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,
					data.getWidth(), data.getHeight(),
					0, GL_RGB, GL_UNSIGNED_BYTE, d);

			glBindTexture(GL_TEXTURE_2D, 0);
		}
	}

	return texHdl;
}

void Chunk::deleteTexture() {
	glDeleteTextures(1, &texHdl);
}

Chunk::Key Chunk::key(Chunk::Pos x, Chunk::Pos y) {
	union {
		struct {
			Chunk::Pos x;
			Chunk::Pos y;
		};
		Chunk::Key xy;
	} k = {x, y};
	return k.xy;
}

void Chunk::loadCompleted(unsigned, void * e, void * buf, unsigned len) {
	Chunk& c = *static_cast<Chunk *>(e);
	c.preventUnloading(true);
	if (len) {
		c.data.readFileOnMem(static_cast<u8 *>(buf), len);
	} else { // 204
		c.data.allocate(Chunk::size, Chunk::size, c.w.getBackgroundColor());
	}

	if (!c.protectionsLoaded) {
		c.protectionData.fill(0);
	}

	c.loaderRequest = nullptr;
	c.preventUnloading(false);

	c.w.signalChunkLoaded(c);

	std::printf("[Chunk] Loaded (%i, %i)\n", c.x, c.y);
}

void Chunk::loadFailed(unsigned, void * e, int code, const char * err) {
	Chunk& c = *static_cast<Chunk *>(e);
	c.preventUnloading(true);
	// what do?
	//c.data.allocate(Chunk::size, Chunk::size, c.w.getBackgroundColor());
	c.protectionData.fill(0);

	std::printf("[Chunk] Load failed (%i, %i): %s\n", c.x, c.y, err);

	c.loaderRequest = nullptr;
	c.preventUnloading(false);
}