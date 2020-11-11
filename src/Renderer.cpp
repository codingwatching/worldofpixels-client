#include "Renderer.hpp"

#include <exception>
#include <cstdio>
#include <cmath>

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <glm/ext/vector_float3.hpp>
#include <util/explints.hpp>
#include <world/World.hpp>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>
//#include <glm/ext/matrix_transform.hpp>


// TODO: use resume/pauseRendering()

constexpr std::string_view chunkVertex = R"(#version 100
uniform mediump mat4 view;
uniform mediump mat4 proj;

uniform vec2 chunkOffset;

attribute vec2 vPosA;
attribute vec2 vTexCoordA;

varying vec2 vTexCoordV;

void main() {
	vTexCoordV = vTexCoordA;

	gl_Position = proj * view * vec4(chunkOffset + vPosA, 0.5, 1.0);
}
)";

constexpr std::string_view chunkFragment = R"(#version 100
precision mediump float;

uniform mediump float chunkSize;
uniform mediump float zoom;
uniform sampler2D tex;

varying vec2 vTexCoordV;

void main() {
	gl_FragColor = texture2D(tex, vTexCoordV);

	vec2 pixelPos = vTexCoordV * chunkSize * zoom;

	if (mod(pixelPos.x, 2.0) < 1.0 ^^ mod(pixelPos.y, 2.0) < 1.0) {
		return;
	}

	vec2 line1 = mod(pixelPos, zoom);

	if (!(line1.x < 1.0 || line1.y < 1.0)) {
		return;
	}

	vec2 line16 = mod(pixelPos, 16.0 * zoom);

	lowp float mult = 0.8;
	if (zoom <= 4.0) {
		mult = max(0.8, min(1.0, 2.0 - zoom / 2.0));
	}

	if (pixelPos.x < 1.0 || pixelPos.y < 1.0) {
		mult -= 0.4;
	} else if (line16.x < 1.0 || line16.y < 1.0) {
		mult -= 0.2;
	}

	gl_FragColor = gl_FragColor * vec4(mult, mult, mult, 1.0);
}
)";

static GLint maxTexUnits;

static const GLint attrVPos = 0;
static const GLint attrVTexCoord = 1;

static const float test[] = {
	0.f,   0.f,   0.f, 0.f,
	0.f,   512.f, 0.f, 1.f,
	512.f, 0.f,   1.f, 0.f,
	512.f, 512.f, 1.f, 1.f,
	0.f,   512.f, 0.f, 1.f,
	512.f, 0.f,   1.f, 0.f
};

Renderer::Renderer(World& w)
: w(w),
  ctxInfo(0),
  vpWidth(1),
  vpHeight(1),
  view(1.0f),
  projection(1.0f),
  chunkProgram(0),
  fxProgram(0),
  cursorsProgram(0) {
	if (!(activateRenderingContext() || activateRenderingContext(true))) {
		std::abort();
	}

	std::printf("[Renderer] Initialized\n");
}

Renderer::~Renderer() {
	stopRenderLoop();
	destroyRenderingContext();
	std::printf("[Renderer] Destroyed\n");
}

sz_t Renderer::getMaxVisibleChunks() const {
	return (vpWidth / Chunk::size + 2) * (vpHeight / Chunk::size + 2);
}

void Renderer::loadMissingChunks() {
	float hVpWidth = vpWidth / 2.f / getZoom();
	float hVpHeight = vpHeight / 2.f / getZoom();
	float tlx = std::floor((getX() - hVpWidth) / Chunk::size);
	float tly = std::floor((getY() - hVpHeight) / Chunk::size);
	float brx = std::floor((getX() + hVpWidth) / Chunk::size);
	float bry = std::floor((getY() + hVpHeight) / Chunk::size);

	for (; tly <= bry; tly += 1.f) {
		for (float tlx2 = tlx; tlx2 <= brx; tlx2 += 1.f) {
			w.getOrLoadChunk(tlx2, tly);
		}
	}
}

bool Renderer::isChunkVisible(Chunk::Pos px, Chunk::Pos py) const {
	float x = static_cast<float>(px) * Chunk::size;
	float y = static_cast<float>(py) * Chunk::size;
	float czoom = getZoom();
	float tlcx = getX() - static_cast<float>(vpWidth) / 2.f / czoom;
	float tlcy = getY() - static_cast<float>(vpHeight) / 2.f / czoom;

	return x + Chunk::size > tlcx && y + Chunk::size > tlcy &&
	       x <= tlcx + vpWidth / czoom && y <= tlcy + vpHeight / czoom;
}

bool Renderer::isChunkVisible(const Chunk& c) const {
	return isChunkVisible(c.getX(), c.getY());
}


void Renderer::useChunk(Chunk& c) {
	//std::printf("[Renderer] got chunk\n");
	/*glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, c.getGlTexture());*/
	//resumeRendering();
}

void Renderer::unuseChunk(Chunk& c) {

}

void Renderer::setPos(float x, float y) {
	Camera::setPos(x, y);
	setupView();
	glUseProgram(chunkProgram);
	updateUnifViewMatrix();
}

void Renderer::setZoom(float z) {
	Camera::setZoom(z);
	setupProjection();
	glUseProgram(chunkProgram);
	updateUnifProjMatrix();
	updateUnifZoom();
}

void Renderer::translate(float dx, float dy) {
	setPos(getX() + dx, getY() + dy);
}

void Renderer::render() {
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	float czoom = getZoom();
	float hVpWidth = vpWidth / 2.f / czoom;
	float hVpHeight = vpHeight / 2.f / czoom;
	float tlx = std::floor((getX() - hVpWidth) / Chunk::size);
	float tly = std::floor((getY() - hVpHeight) / Chunk::size);
	float brx = std::floor((getX() + hVpWidth) / Chunk::size);
	float bry = std::floor((getY() + hVpHeight) / Chunk::size);

	glUseProgram(chunkProgram);
	
	glBindBuffer(GL_ARRAY_BUFFER, tbuf);
	glEnableVertexAttribArray(attrVPos);
	glEnableVertexAttribArray(attrVTexCoord);

	//glActiveTexture(GL_TEXTURE0);
	//glUniform1i(chunkUnifTex, 0);

	//setupView();
	//updateUnifViewMatrix();
	const auto& chunks = w.getChunkMap();
	sz_t loadCount = 0;
	for (; tly <= bry; tly += 1.f) {
		for (float tlx2 = tlx; tlx2 <= brx; tlx2 += 1.f) {
			auto search = chunks.find(Chunk::key(tlx2, tly));

			if (search == chunks.end()) {
				if (++loadCount < 12) {
					w.getOrLoadChunk(tlx2, tly);
				}

				continue;
			}

			if (u32 tex = search->second.getGlTexture()) {
				glUniform2f(chunkUnifOffset, tlx2 * Chunk::size, tly * Chunk::size);
				glBindTexture(GL_TEXTURE_2D, tex);

				glDrawArrays(GL_TRIANGLES, 0, sizeof(test) / sizeof(float) / 4);
			} else if (!search->second.isReady()) {
				++loadCount;
			}
		}
	}



	//pauseRendering();
}

void Renderer::updateUnifViewMatrix() {
	glUniformMatrix4fv(chunkUnifView, 1, GL_FALSE, glm::value_ptr(view));
}

void Renderer::updateUnifProjMatrix() {
	glUniformMatrix4fv(chunkUnifProj, 1, GL_FALSE, glm::value_ptr(projection));
}

void Renderer::updateUnifZoom() {
	std::printf("[Renderer] Zoom: %f\n", getZoom());
	glUniform1f(chunkUnifZoom, getZoom());
}

GLuint Renderer::buildProgram(std::string_view vertexShader, std::string_view fragmentShader) {
	GLuint vtx = glCreateShader(GL_VERTEX_SHADER);
	GLuint fmt = glCreateShader(GL_FRAGMENT_SHADER);

	if (!vtx || !fmt) {
		std::fprintf(stderr, "[Renderer] Failed to create shader(s)\n");
		glDeleteShader(vtx);
		glDeleteShader(fmt);
		return 0;
	}

	{
		const GLchar * vtxSrc[1] = {vertexShader.data()};
		const GLchar * fmtSrc[1] = {fragmentShader.data()};
		const GLint vtxLen[1]    = {static_cast<GLint>(vertexShader.size())};
		const GLint fmtLen[1]    = {static_cast<GLint>(fragmentShader.size())};

		glShaderSource(vtx, 1, vtxSrc, vtxLen);
		glShaderSource(fmt, 1, fmtSrc, fmtLen);
	}

	glCompileShader(vtx);
	glCompileShader(fmt);

	GLint result = 0;
	glGetShaderiv(vtx, GL_INFO_LOG_LENGTH, &result);
	if (result > 0) {
		std::vector<GLchar> msg(result);
		glGetShaderInfoLog(vtx, result, nullptr, msg.data());
		std::fputs(msg.data(), stderr);
	}

	glGetShaderiv(fmt, GL_INFO_LOG_LENGTH, &result);
	if (result > 0) {
		std::vector<GLchar> msg(result);
		glGetShaderInfoLog(vtx, result, nullptr, msg.data());
		std::fputs(msg.data(), stderr);
	}

	glGetShaderiv(vtx, GL_COMPILE_STATUS, &result);
	if (result != GL_TRUE) {
		std::fprintf(stderr, "[Renderer] Failed to compile vertex shader\n");
		glDeleteShader(vtx);
		glDeleteShader(fmt);
		return 0;
	}

	glGetShaderiv(fmt, GL_COMPILE_STATUS, &result);
	if (result != GL_TRUE) {
		std::fprintf(stderr, "[Renderer] Failed to compile fragment shader\n");
		glDeleteShader(vtx);
		glDeleteShader(fmt);
		return 0;
	}

	GLuint prog = glCreateProgram();
	if (!prog) {
		std::fprintf(stderr, "[Renderer] Failed to create gl program\n");
		glDeleteShader(vtx);
		glDeleteShader(fmt);
		return 0;
	}

	glAttachShader(prog, vtx);
	glAttachShader(prog, fmt);

	glBindAttribLocation(prog, attrVPos, "vPosA");
	glBindAttribLocation(prog, attrVTexCoord, "vTexCoordA");

	glLinkProgram(prog);

	glDetachShader(prog, vtx);
	glDetachShader(prog, fmt);
	glDeleteShader(vtx);
	glDeleteShader(fmt);

	glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &result);
	if (result > 0) {
		std::vector<GLchar> msg(result);
		glGetProgramInfoLog(prog, result, nullptr, msg.data());
		std::fputs(msg.data(), stderr);
	}

	glGetProgramiv(vtx, GL_LINK_STATUS, &result);
	if (result != GL_TRUE) {
		std::fprintf(stderr, "[Renderer] Failed to link gl program\n");
		glDeleteProgram(prog);
		return 0;
	}

	return prog;
}


bool Renderer::setupView() {
	view = glm::translate(glm::mat4(1.0f), glm::vec3(-getX(), -getY(), 0.f));

	//view = glm::lookAt(eye, center, up);
	return true;
}

bool Renderer::setupProjection() {
	float vpHalfW = static_cast<float>(vpWidth) / 2.0f;
	float vpHalfH = static_cast<float>(vpHeight) / 2.0f;

	float vpHalfZoomedW = /*std::floor*/(vpHalfW) / getZoom();
	float vpHalfZoomedH = /*std::ceil*/(vpHalfH) / getZoom();

	// left, right, bottom, top, near, far
	projection = glm::ortho(-vpHalfZoomedW, vpHalfZoomedW, vpHalfZoomedH, -vpHalfZoomedH, 0.5f, 1.5f);
	projection = glm::scale(projection, glm::vec3(1.f, 1.f, -1.f));

	//std::puts(glm::to_string(projection).c_str());
	return true;
}

bool Renderer::setupShaders() {
	chunkProgram = buildProgram(chunkVertex, chunkFragment);
	chunkUnifView      = glGetUniformLocation(chunkProgram, "view");
	chunkUnifProj      = glGetUniformLocation(chunkProgram, "proj");
	chunkUnifZoom      = glGetUniformLocation(chunkProgram, "zoom");
	chunkUnifChunkSize = glGetUniformLocation(chunkProgram, "chunkSize");
	chunkUnifOffset    = glGetUniformLocation(chunkProgram, "chunkOffset");
	chunkUnifTex       = glGetUniformLocation(chunkProgram, "tex");

	glUseProgram(chunkProgram);
	glUniform1i(chunkUnifTex, 0);
	glUniform1f(chunkUnifChunkSize, static_cast<float>(Chunk::size));
	updateUnifViewMatrix();
	updateUnifProjMatrix();
	updateUnifZoom();

	return chunkProgram;
}

bool Renderer::resizeRenderingContext() {
	getViewportSize();
	glViewport(0, 0, vpWidth, vpHeight);
	setupProjection();
	glUseProgram(chunkProgram);
	updateUnifProjMatrix();
	resumeRendering();
	return true;
}

bool Renderer::setupRenderingContext() {
	glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &maxTexUnits);
	std::printf("[Renderer] Max texture units: %i\n", maxTexUnits);

	startRenderLoop();
	resizeRenderingContext();

	RGB_u bgClr = w.getBackgroundColor();
	glClearColor(bgClr.c.r / 255.f, bgClr.c.g / 255.f, bgClr.c.b / 255.f, 0.5f);

	setupView();
	setupShaders();

	glGenBuffers(1, &tbuf);
	glBindBuffer(GL_ARRAY_BUFFER, tbuf);
	glBufferData(GL_ARRAY_BUFFER, sizeof(test), test, GL_STATIC_DRAW);

	glUseProgram(chunkProgram);
	glVertexAttribPointer(attrVPos, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);

	glVertexAttribPointer(attrVTexCoord, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)(2 * sizeof(float)));

	return true;
}

void Renderer::doRender(void * r) {
	static_cast<Renderer *>(r)->render();
}
