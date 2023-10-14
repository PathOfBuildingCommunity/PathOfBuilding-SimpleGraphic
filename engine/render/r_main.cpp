// SimpleGraphic Engine
// (c) David Gowor, 2014
//
// Module: Render Main
//

#define GLAD_GLES2_IMPLEMENTATION
#define IMGUI_DEFINE_MATH_OPERATORS
#include "r_local.h"

#include <array>
#include <filesystem>
#include <fmt/chrono.h>
#include <map>
#include <numeric>
#include <random>
#include <sodium.h>
#include <sstream>
#include <vector>

#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

// =======
// Classes
// =======

enum r_takeScreenshot_e {
	R_SSNONE,
	R_SSTGA,
	R_SSJPEG,
	R_SSPNG
};

// ============
// Shader Class
// ============

class r_shader_c {
public:
	r_renderer_c* renderer;
	char* name;
	dword		nameHash;
	int			refCount;
	r_tex_c* tex;

	r_shader_c(r_renderer_c* renderer, const char* shname, int flags);
	r_shader_c(r_renderer_c* renderer, const char* shname, int flags, int width, int height, int type, byte* dat);
	~r_shader_c();
};

r_shader_c::r_shader_c(r_renderer_c* renderer, const char* shname, int flags)
	: renderer(renderer)
{
	name = AllocString(shname);
	nameHash = StringHash(shname, 0xFFFF);
	refCount = 0;
	tex = new r_tex_c(renderer->texMan, name, flags);
	if (tex->error) {
		renderer->sys->con->Warning("couldn't load texture '%s'", name);
	}
}

r_shader_c::r_shader_c(r_renderer_c* renderer, const char* shname, int flags, int width, int height, int type, byte* dat)
	: renderer(renderer)
{
	name = AllocString(shname);
	nameHash = StringHash(shname, 0xFFFF);
	refCount = 0;
	image_c img;
	img.width = width;
	img.height = height;
	img.type = type;
	img.comp = type & 0xF;
	img.dat = dat;
	tex = new r_tex_c(renderer->texMan, &img, flags);
	img.dat = NULL;
}

r_shader_c::~r_shader_c()
{
	FreeString(name);
	delete tex;
}

// ===================
// Shader Handle Class
// ===================

r_shaderHnd_c::r_shaderHnd_c(r_shader_c* sh)
	: sh(sh)
{
	sh->refCount++;
}

r_shaderHnd_c::~r_shaderHnd_c()
{
	sh->refCount--;
	if (sh->refCount == 0) {
		sh->tex->AbortLoad();
	}
}

struct Mat4 {
	float m[16];

	float const* data() const {
		return m;
	}
};

Mat4 OrthoMatrix(double left, double right, double bottom, double top, double nearVal, double farVal)
{
	Mat4 ret;
	std::fill_n(ret.m, 16, 0.0f);
	ret.m[0] = (float)(2.0f / (right - left));
	ret.m[5] = (float)(2.0f / (top - bottom));
	ret.m[10] = (float)(-2.0f / (farVal - nearVal));
	ret.m[12] = (float)-((right + left) / (right - left));
	ret.m[13] = (float)-((top + bottom) / (top - bottom));
	ret.m[14] = (float)-((farVal + nearVal) / (farVal - nearVal));
	ret.m[15] = 1.0f;
	return ret;
}

// =================
// Layer queue class
// =================

struct r_layerCmd_s {
	enum {
		VIEWPORT,
		BLEND,
		BIND,
		COLOR,
		QUAD,
	} cmd;
	union {
		r_viewport_s viewport;
		int blendMode;
		r_tex_c* tex;
		col4_t col;
		struct {
			double s[4];
			double t[4];
			double x[4];
			double y[4];
		} quad;
	};
};

r_layer_c::r_layer_c(r_renderer_c* renderer, int layer, int subLayer)
	: renderer(renderer), layer(layer), subLayer(subLayer)
{
	numCmd = 0;
	cmdSize = 8;
	cmdList = new r_layerCmd_s * [cmdSize];
}

r_layer_c::~r_layer_c()
{
	delete cmdList;
}

r_layerCmd_s* r_layer_c::NewCommand()
{
	r_layerCmd_s* cmd;
	if (renderer->layerCmdBinCount) {
		cmd = renderer->layerCmdBin[--renderer->layerCmdBinCount];
	}
	else {
		cmd = new r_layerCmd_s;
	}
	if (numCmd == cmdSize) {
		cmdSize <<= 1;
		trealloc(cmdList, cmdSize);
	}
	cmdList[numCmd++] = cmd;
	return cmd;
}

void r_layer_c::SetViewport(r_viewport_s* viewport)
{
	r_layerCmd_s* cmd = NewCommand();
	cmd->cmd = r_layerCmd_s::VIEWPORT;
	cmd->viewport.x = viewport->x;
	cmd->viewport.y = viewport->y;
	cmd->viewport.width = viewport->width;
	cmd->viewport.height = viewport->height;
}

void r_layer_c::SetBlendMode(int mode)
{
	r_layerCmd_s* cmd = NewCommand();
	cmd->cmd = r_layerCmd_s::BLEND;
	cmd->blendMode = mode;
}

void r_layer_c::Bind(r_tex_c* tex)
{
	r_layerCmd_s* cmd = NewCommand();
	cmd->cmd = r_layerCmd_s::BIND;
	cmd->tex = tex;
}

void r_layer_c::Color(col4_t col)
{
	r_layerCmd_s* cmd = NewCommand();
	cmd->cmd = r_layerCmd_s::COLOR;
	Vector4Copy(col, cmd->col);
}

void r_layer_c::Quad(double s0, double t0, double x0, double y0, double s1, double t1, double x1, double y1, double s2, double t2, double x2, double y2, double s3, double t3, double x3, double y3)
{
	r_layerCmd_s* cmd = NewCommand();
	cmd->cmd = r_layerCmd_s::QUAD;
	cmd->quad.s[0] = s0; cmd->quad.s[1] = s1; cmd->quad.s[2] = s2; cmd->quad.s[3] = s3;
	cmd->quad.t[0] = t0; cmd->quad.t[1] = t1; cmd->quad.t[2] = t2; cmd->quad.t[3] = t3;
	cmd->quad.x[0] = x0; cmd->quad.x[1] = x1; cmd->quad.x[2] = x2; cmd->quad.x[3] = x3;
	cmd->quad.y[0] = y0; cmd->quad.y[1] = y1; cmd->quad.y[2] = y2; cmd->quad.y[3] = y3;
}

// =================
// Geometric queries
// =================

struct r_aabb_s {
	float lo[2];
	float hi[2];
};

r_aabb_s AabbFromCmdQuad(decltype(r_layerCmd_s::quad)& q, r_viewport_s& vp)
{
	r_aabb_s r{
		{+FLT_MAX, +FLT_MAX},
		{-FLT_MAX, -FLT_MAX},
	};
	for (size_t i = 0; i < 4; ++i) {
		r.lo[0] = (std::min)(r.lo[0], (float)q.x[i]);
		r.lo[1] = (std::min)(r.lo[1], (float)q.y[i]);
		r.hi[0] = (std::max)(r.hi[0], (float)q.x[i]);
		r.hi[1] = (std::max)(r.hi[1], (float)q.y[i]);
	}
	r.lo[0] += vp.x;
	r.lo[1] += vp.y;
	r.hi[0] += vp.x;
	r.hi[1] += vp.y;
	return r;
}

r_aabb_s AabbFromViewport(r_viewport_s& vp)
{
	r_aabb_s r{
		{(float)vp.x, (float)vp.y },
		{(float)(vp.x + vp.width), (float)(vp.y + vp.height) },
	};
	return r;
}

bool AabbAabbIntersects(r_aabb_s& a, r_aabb_s& b)
{
	// A.lo <= B.hi && A.hi >= B.lo
	return a.lo[0] <= b.hi[0] && a.lo[1] <= b.hi[1] && a.hi[0] >= b.lo[0] && a.hi[1] >= b.lo[1];
}

struct Vertex {
	float x, y;
	float u, v;
	float r, g, b, a;
	float viewX, viewY, viewW, viewH;
	float texId;
};

struct Batch {
	explicit Batch(GLuint prog);
	Batch(Batch&& rhs);
	Batch& operator = (Batch&& rhs);
	Batch(Batch const&) = delete;
	Batch& operator = (Batch const&) = delete;
	~Batch();

	GLuint prog;
	GLint xyAttr;
	GLint uvAttr;
	GLint tintAttr;
	GLint texIdAttr;
	GLint viewportAttr;

	std::vector<Vertex> vertices;

	void Execute(GLuint sharedVbo, size_t vertexBase);
};

Batch::Batch(GLuint prog)
	: prog(prog)
{
	xyAttr = glGetAttribLocation(prog, "a_vertex");
	uvAttr = glGetAttribLocation(prog, "a_texcoord");
	tintAttr = glGetAttribLocation(prog, "a_tint");
	texIdAttr = glGetAttribLocation(prog, "a_texId");
	viewportAttr = glGetAttribLocation(prog, "a_viewport");
}

Batch::Batch(Batch&& rhs)
	: prog(rhs.prog)
	, xyAttr(rhs.xyAttr)
	, uvAttr(rhs.uvAttr)
	, tintAttr(rhs.tintAttr)
	, viewportAttr(rhs.viewportAttr)
	, vertices(std::move(rhs.vertices))
{
}

Batch& Batch::operator = (Batch&& rhs) {
	prog = rhs.prog;
	xyAttr = rhs.xyAttr;
	uvAttr = rhs.uvAttr;
	tintAttr = rhs.tintAttr;
	texIdAttr = rhs.texIdAttr;
	viewportAttr = rhs.viewportAttr;
	vertices = std::move(rhs.vertices);

	return *this;
}

Batch::~Batch() {}

void Batch::Execute(GLuint sharedVbo, size_t vertexBase)
{
	if (vertices.empty()) {
		return;
	}

	glBindBuffer(GL_ARRAY_BUFFER, sharedVbo);
	auto dataPtr = (uint8_t const*)vertices.data();
	auto dataOff = vertexBase * sizeof(Vertex);
	auto dataSize = vertices.size() * sizeof(Vertex);
	glBufferSubData(GL_ARRAY_BUFFER, dataOff, dataSize, dataPtr);
	glVertexAttribPointer(xyAttr, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void const*)offsetof(Vertex, x));
	glVertexAttribPointer(uvAttr, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void const*)offsetof(Vertex, u));
	glVertexAttribPointer(tintAttr, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void const*)offsetof(Vertex, r));
	glVertexAttribPointer(texIdAttr, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void const*)offsetof(Vertex, texId));
	glVertexAttribPointer(viewportAttr, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void const*)offsetof(Vertex, viewX));
	glEnableVertexAttribArray(xyAttr);
	glEnableVertexAttribArray(uvAttr);
	glEnableVertexAttribArray(tintAttr);
	glEnableVertexAttribArray(texIdAttr);
	glEnableVertexAttribArray(viewportAttr);
	glDrawArrays(GL_TRIANGLES, 0, (GLsizei)vertices.size());
	glDisableVertexAttribArray(xyAttr);
	glDisableVertexAttribArray(uvAttr);
	glDisableVertexAttribArray(tintAttr);
	glDisableVertexAttribArray(texIdAttr);
	glDisableVertexAttribArray(viewportAttr);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	vertices.clear();
}

struct RenderStrategy {
	virtual ~RenderStrategy() = default;

	virtual void ProcessCommand(r_layerCmd_s* cmd) = 0;
	virtual void Flush() = 0;
	virtual void SetShowStats(bool showStats) { showStats_ = showStats; }

protected:
	bool showStats_{};
};

static std::map<r_blendMode_e, char const*> const s_blendModeString{
	{RB_ALPHA, "RB_ALPHA"},
	{RB_PRE_ALPHA, "RB_PRE_ALPHA"},
	{RB_ADDITIVE, "RB_ADDITIVE"},
};

struct AdjacentMergeStrategy : RenderStrategy {
	AdjacentMergeStrategy(r_layer_c* layer, r_renderer_c* renderer, GLuint prog)
		: layer_(layer), renderer_(renderer), prog_(prog), batch_(prog)
	{
		for (size_t i = 0;; ++i) {
			GLint loc = glGetUniformLocation(prog, fmt::format("s_tex[{}]", i).c_str());
			if (loc == -1) {
				break;
			}
			texLocs_.push_back(loc);
		}
		mvpMatrixLoc_ = glGetUniformLocation(prog_, "mvp_matrix");
		batchTextureCap_ = texLocs_.size();
		glGenBuffers(1, &vbo_);
	}

	~AdjacentMergeStrategy() {
		glDeleteBuffers(1, &vbo_);
	}

	struct BatchKey {
		int blendMode = -1;

		bool operator < (BatchKey const& rhs) const {
			return blendMode < rhs.blendMode;
		}

		bool operator == (BatchKey const& rhs) const {
			return !(*this < rhs) && !(rhs < *this);
		}

		bool operator != (BatchKey const& rhs) const {
			return !(*this == rhs);
		}
	};

	void ProcessCommand(r_layerCmd_s* cmd) override {
		switch (cmd->cmd) {
		case r_layerCmd_s::VIEWPORT:
			nextViewport_ = cmd->viewport;
			if (showStats_) {
				// ImGui::Text("VIEWPORT: %dx%d @ %d,%d", cmd->viewport.width, cmd->viewport.height, cmd->viewport.x, cmd->viewport.y);
			}
			break;
		case r_layerCmd_s::BLEND:
			latchKey_.blendMode = cmd->blendMode;
			if (showStats_) {
				// ImGui::Text("BLEND: %s", s_blendModeString.at((r_blendMode_e)cmd->blendMode));
			}
			break;
		case r_layerCmd_s::BIND:
			nextTex_ = cmd->tex;
			if (showStats_) {
				// ImGui::Text("TEX: %s", cmd->tex->fileName.c_str());
			}
			break;
		case r_layerCmd_s::COLOR:
			std::copy_n(cmd->col, 4, tint_.data());
			break;
		case r_layerCmd_s::QUAD: {
			if (showStats_) {
				// ImGui::Text("QUAD");
			}

			// Cull the quad first before it influences any boundary cuts.
			if (!!renderer_->r_drawCull->intVal) {
				auto a = AabbFromCmdQuad(cmd->quad, nextViewport_);
				auto b = AabbFromViewport(nextViewport_);
				bool intersects = AabbAabbIntersects(a, b);
				if (!intersects) {
					break;
				}
			}

			// If the current batch is incompatible key-wise, dispatch it to get a fresh
			// batch to grow in.
			if (!batch_.batch.vertices.empty() && batch_.key != latchKey_) {
				Dispatch();
			}
			batch_.key = latchKey_;

			// Check current (and only) batch if the texture set has the latched texture.
			// If it's there, use its index as vertex attribute.
			// If it's not, insert it if room, otherwise dispatch batch and prepare a fresh one.
			size_t texSlot{};
			{
				auto& textures = batch_.textures;
				auto texI = std::find(textures.begin(), textures.end(), nextTex_);
				if (texI == textures.end()) {
					if (textures.size() == batchTextureCap_) {
						Dispatch();
					}
					texI = textures.insert(textures.end(), nextTex_);
				}
				texSlot = std::distance(textures.begin(), texI);
			}

			Vertex quad[4];
			for (int v = 0; v < 4; v++) {
				auto& q = quad[v];
				auto& vp = nextViewport_;
				q.u = (float)cmd->quad.s[v];
				q.v = (float)cmd->quad.t[v];
				q.x = (float)cmd->quad.x[v];
				q.y = (float)cmd->quad.y[v];
				q.r = tint_[0];
				q.g = tint_[1];
				q.b = tint_[2];
				q.a = tint_[3];
				q.texId = (float)texSlot;
				q.viewX = (float)vp.x;
				q.viewY = (float)vp.y;
				q.viewW = (float)vp.width;
				q.viewH = (float)vp.height;
			}
			// 3-2
			// |/|
			// 0-1
			size_t indices[] = { 0, 1, 2, 0, 2, 3 };
			for (auto idx : indices) {
				batch_.batch.vertices.push_back(quad[idx]);
			}
			totalVertexCount_ += std::size(indices);
		} break;
		}
	}

	void Flush() {
		if (!batch_.batch.vertices.empty()) {
			Dispatch();
		}
		if (showStats_) {
			ImGui::BulletText("Layer %d:%d - %d batches", layer_->layer, layer_->subLayer, batchIndex);
		}
	}

private:
	void Dispatch() {
		glBindBuffer(GL_ARRAY_BUFFER, vbo_);
		auto& batch = batch_.batch;
		auto& textures = batch_.textures;
		size_t vertexCount = batch.vertices.size();
		glBufferData(GL_ARRAY_BUFFER, vertexCount * sizeof(Vertex), nullptr, GL_STREAM_DRAW);
		glUseProgram(prog_);

		auto& key = batch_.key;
		auto& lastKey = lastDispatchKey_;

		if (showStats_) {
			ImGui::Text("Batch %d", batchIndex);
			ImGui::Text("%d verts", batch.vertices.size());
		}

		{
			auto& vid = renderer_->sys->video->vid;
			float fbScaleX = vid.fbSize[0] / (float)vid.size[0];
			float fbScaleY = vid.fbSize[1] / (float)vid.size[1];
			int virtualW = renderer_->VirtualScreenWidth();
			int virtualH = renderer_->VirtualScreenHeight();
			glViewport(0, 0, virtualW, virtualH);
			Mat4 mvpMatrix = OrthoMatrix(0, virtualW, virtualH, 0, -9999, 9999);
			glUniformMatrix4fv(mvpMatrixLoc_, 1, GL_FALSE, mvpMatrix.data());
		}
		if (!lastKey || lastKey->blendMode != key.blendMode) {
			if (showStats_) {
				ImGui::Text("New blend mode %s", s_blendModeString.at((r_blendMode_e)key.blendMode));
			}
			switch (key.blendMode) {
			case RB_ALPHA:
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				break;
			case RB_PRE_ALPHA:
				glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
				break;
			case RB_ADDITIVE:
				glBlendFunc(GL_ONE, GL_ONE);
				break;
			}
		}
		{
			for (size_t i = 0, numTex = texLocs_.size(); i < numTex; ++i) {
				glUniform1i(texLocs_[i], (GLint)i);
				glActiveTexture((GLenum)(GL_TEXTURE0 + i));
				if (i < textures.size()) {
					auto tex = textures[i];
					tex->Bind();
					if (showStats_) {
						ImGui::Text("New tex %d (%s)", tex->texId, tex->fileName.c_str());
					}
				}
				else {
					glBindTexture(GL_TEXTURE_2D, 0);
				}
			}
			glActiveTexture(GL_TEXTURE0);
		}

		batch.Execute(vbo_, 0);

		lastDispatchKey_ = key;
		batch_.batch.vertices.clear();
		batch_.textures.clear();

		glUseProgram(0);

		batchIndex += 1;
	}

	r_layer_c* layer_{};
	r_renderer_c* renderer_{};
	GLuint prog_{};
	std::vector<GLint> texLocs_;
	GLint mvpMatrixLoc_{};

	size_t batchTextureCap_{};
	GLuint vbo_{};

	struct TexturedBatch {
		explicit TexturedBatch(GLuint prog) : batch(prog) {
			textures.reserve(1ull << 20);
		}

		BatchKey key{};
		Batch batch;
		std::vector<r_tex_c*> textures;
	};

	BatchKey latchKey_{};
	r_viewport_s nextViewport_{};
	r_tex_c* nextTex_{};
	std::optional<BatchKey> lastDispatchKey_;
	TexturedBatch batch_;

	std::array<float, 4> tint_{ 1.0f, 1.0f, 1.0f, 1.0f };

	size_t totalVertexCount_ = 0;
	size_t batchIndex = 0;
};

void r_layer_c::Render()
{
	int const optLevel = renderer->r_layerOptimize->intVal;
	bool const shuffle = renderer->r_layerShuffle->intVal == 1;

	std::unique_ptr<RenderStrategy> strat(new AdjacentMergeStrategy(this, renderer, renderer->tintedTextureProgram));

	if (renderer->glPushGroupMarkerEXT)
	{
		std::ostringstream oss;
		oss << "Layer " << layer << ", sub-layer " << subLayer;
		renderer->glPushGroupMarkerEXT(0, oss.str().c_str());
	}

	if (strat) {
		bool showStats{};
		if (renderer->debugLayers) {
			if (ImGui::Begin("Layers", &renderer->debugLayers)) {
				std::string heading = fmt::format("Layer {}:{}", layer, subLayer);
				showStats = ImGui::CollapsingHeader(heading.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
			}
		}
		strat->SetShowStats(showStats);

		for (int i = 0; i < numCmd; i++) {
			r_layerCmd_s* cmd = cmdList[i];
			strat->ProcessCommand(cmd);
		}

		strat->Flush();

		if (renderer->debugLayers) {
			ImGui::End();
		}
	}

	for (int i = 0; i < numCmd; i++) {
		r_layerCmd_s* cmd = cmdList[i];
		if (renderer->layerCmdBinCount == renderer->layerCmdBinSize) {
			renderer->layerCmdBinSize <<= 1;
			trealloc(renderer->layerCmdBin, renderer->layerCmdBinSize);
		}
		renderer->layerCmdBin[renderer->layerCmdBinCount++] = cmd;
	}
	numCmd = 0;
	if (renderer->glPopGroupMarkerEXT) {
		renderer->glPopGroupMarkerEXT();
	}
}

void r_layer_c::Discard()
{
	for (int i = 0; i < numCmd; i++) {
		r_layerCmd_s* cmd = cmdList[i];
		if (renderer->layerCmdBinCount == renderer->layerCmdBinSize) {
			renderer->layerCmdBinSize <<= 1;
			trealloc(renderer->layerCmdBin, renderer->layerCmdBinSize);
		}
		renderer->layerCmdBin[renderer->layerCmdBinCount++] = cmd;
	}
	numCmd = 0;
}

// =====================
// r_IRenderer Interface
// =====================

r_IRenderer* r_IRenderer::GetHandle(sys_IMain* sysHnd)
{
	return new r_renderer_c(sysHnd);
}

void r_IRenderer::FreeHandle(r_IRenderer* hnd)
{
	delete (r_renderer_c*)hnd;
}

r_renderer_c::r_renderer_c(sys_IMain* sysHnd)
	: conCmdHandler_c(sysHnd->con), sys(sysHnd)
{
	r_compress = sys->con->Cvar_Add("r_compress", CV_ARCHIVE, "0");
	r_screenshotFormat = sys->con->Cvar_Add("r_screenshotFormat", CV_ARCHIVE, "jpg");
	r_layerDebug = sys->con->Cvar_Add("r_layerDebug", CV_ARCHIVE, "0");
	r_layerOptimize = sys->con->Cvar_Add("r_layerOptimize", CV_ARCHIVE | CV_CLAMP, "1", 0, 1);
	r_layerShuffle = sys->con->Cvar_Add("r_layerShuffle", CV_ARCHIVE | CV_CLAMP, "0", 0, 1);
	r_elideFrames = sys->con->Cvar_Add("r_elideFrames", CV_ARCHIVE | CV_CLAMP, "1", 0, 1);
	r_drawCull = sys->con->Cvar_Add("r_drawCull", CV_ARCHIVE | CV_CLAMP, "1", 0, 1);

	Cmd_Add("screenshot", 0, "[<format>]", this, &r_renderer_c::C_Screenshot);
}

static bool GetShaderCompileSuccess(GLuint id)
{
	GLint success{};
	glGetShaderiv(id, GL_COMPILE_STATUS, &success);
	return success == GL_TRUE;
}

static std::string GetShaderInfoLog(GLuint id)
{
	GLint len{};
	glGetShaderiv(id, GL_INFO_LOG_LENGTH, &len);
	std::vector<char> msg(len);
	glGetShaderInfoLog(id, (GLsizei)msg.size(), &len, msg.data());
	return std::string(msg.data(), msg.data() + len);
}

static bool GetProgramLinkSuccess(GLuint id)
{
	GLint success{};
	glGetProgramiv(id, GL_LINK_STATUS, &success);
	return success == GL_TRUE;
}

static std::string GetProgramInfoLog(GLuint id)
{
	GLint len{};
	glGetProgramiv(id, GL_INFO_LOG_LENGTH, &len);
	std::vector<char> msg(len);
	glGetProgramInfoLog(id, (GLsizei)msg.size(), &len, msg.data());
	return std::string(msg.data(), msg.data() + len);
}

static char const* s_tintedTextureVertexSource = R"(#version 100

uniform mat4 mvp_matrix;

attribute vec2 a_vertex;
attribute vec2 a_texcoord;
attribute vec4 a_tint;
attribute float a_texId;
attribute vec4 a_viewport;

varying vec2 v_screenPos;
varying vec2 v_texcoord;
varying vec4 v_tint;
varying float v_texId;
varying vec4 v_viewport;

void main(void)
{
	v_texcoord = a_texcoord;
	v_tint = a_tint;
	v_texId = a_texId;
	vec2 vp0 = a_viewport.xy + vec2(0.0, a_viewport.w);
	vec2 vp1 = a_viewport.xy + vec2(a_viewport.z, 0.0);
	v_viewport = vec4(
		(mvp_matrix * vec4(vp0, 0.0, 1.0)).xy,
		(mvp_matrix * vec4(vp1, 0.0, 1.0)).xy);
	vec4 pos = mvp_matrix * vec4(a_vertex + a_viewport.xy, 0.0, 1.0);
	v_screenPos = pos.xy;
	gl_Position = pos;
}
)";

static char const* s_tintedTextureFragmentTemplate = R"(#version 100
precision mediump float;

uniform sampler2D s_tex[{SG_TEXTURE_COUNT}];
uniform vec4 i_tint;

varying vec2 v_screenPos;
varying vec2 v_texcoord;
varying vec4 v_tint;
varying float v_texId;
varying vec4 v_viewport; // x0, y0, x1, y1

void main(void)
{{
	float x = v_screenPos[0], y = v_screenPos[1];
	if (x < v_viewport[0] ||
	    y < v_viewport[1] ||
	    x >= v_viewport[2] ||
	    y >= v_viewport[3]) {{
		discard;
	}}
	vec4 color;
	{SG_TEXTURE_SWITCH}
	gl_FragColor = color * v_tint;
}}
)";

std::string const s_scaleVsSource = R"(#version 100
attribute vec4 a_position;
attribute vec2 a_texcoord;

varying vec2 v_texcoord;

void main(void) {
	gl_Position = a_position;
	v_texcoord = a_texcoord;
}
)";

std::string const s_scaleFsSource = R"(#version 100
precision mediump float;

uniform sampler2D s_tex;

varying vec2 v_texcoord;

void main(void) {
	vec3 color = texture2D(s_tex, v_texcoord).rgb;
	gl_FragColor = vec4(color, 1.0);
}
)";

// =============
// Init/Shutdown
// =============

void r_renderer_c::Init(r_featureFlag_e features)
{
	sys->con->PrintFunc("Render Init");

	apiDpiAware = !!(features & F_DPI_AWARE);

	timer_c timer;
	timer.Start();

	// Initialise OpenGL
	openGL = sys_IOpenGL::GetHandle(sys);
	sys_glSet_s set;
	set.bColor = 32;
	set.bDepth = 24;
	set.bStencil = 0;
	set.vsync = true;
	if (openGL->Init(&set)) {
		sys->Error("OpenGL initialisation failed");
	}

	// Get strings
	st_vendor = (const char*)glGetString(GL_VENDOR);
	st_renderer = (const char*)glGetString(GL_RENDERER);
	st_ver = (const char*)glGetString(GL_VERSION);
	st_ext = (const char*)glGetString(GL_EXTENSIONS);

	glGetIntegerv(GL_MAX_TEXTURE_SIZE, (int*)&texMaxDim);
	sys->con->Printf("GL_MAX_TEXTURE_SIZE: %d\n", texMaxDim);

	// Set default state
	glClearColor(0.0, 0.0, 0.0, 1.0);
	glEnable(GL_TEXTURE_2D);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);

	// Load extensions
	sys->con->Printf("Loading OpenGL extensions...\n");

	if (strstr(st_ext, "GL_EXT_texture_compression_s3tc")) {
		sys->con->Printf("using GL_EXT_texture_compression_s3tc\n");
		glCompressedTexImage2D = (PFNGLCOMPRESSEDTEXIMAGE2DPROC)openGL->GetProc("glCompressedTexImage2D");
	}
	else {
		sys->con->Printf("GL_EXT_texture_compression_s3tc not supported\n");
		glCompressedTexImage2D = NULL;
	}

	if (strstr(st_ext, "GL_EXT_debug_marker")) {
		sys->con->Printf("using GL_EXT_debug_marker\n");
		glInsertEventMarkerEXT = (PFNGLINSERTEVENTMARKEREXTPROC)openGL->GetProc("glInsertEventMarkerEXT");
		glPushGroupMarkerEXT = (PFNGLPUSHGROUPMARKEREXTPROC)openGL->GetProc("glPushGroupMarkerEXT");
		glPopGroupMarkerEXT = (PFNGLPOPGROUPMARKEREXTPROC)openGL->GetProc("glPopGroupMarkerEXT");
	}
	else {
		sys->con->Printf("GL_EXT_debug_marker not supported\n");
		glInsertEventMarkerEXT = NULL;
		glPushGroupMarkerEXT = NULL;
		glPopGroupMarkerEXT = NULL;
	}

	texNonPOT = true;

	// Initialise texture manager
	texMan = r_ITexManager::GetHandle(this);

	// Initialise shader array
	numShader = 0;
	memset(shaderList, 0, sizeof(shaderList));

	GLint maxTextureImageUnits{};
	glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &maxTextureImageUnits);

	// Initialise vertex programs
	{
		GLint success = GL_FALSE;
		GLuint prog = glCreateProgram();
		GLuint vs = glCreateShader(GL_VERTEX_SHADER);
		glShaderSource(vs, 1, &s_tintedTextureVertexSource, nullptr);
		glCompileShader(vs);
		if (!GetShaderCompileSuccess(vs)) {
			std::string log = GetShaderInfoLog(vs);
			sys->Error("Failed to compile vertex shader:\n%s", log.c_str());
		}
		GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
		std::string textureSwitch;
		{
			fmt::memory_buffer buf;
			for (size_t i = 0; i < maxTextureImageUnits; ++i) {
				if (i == 0) {
					fmt::format_to(fmt::appender(buf), "if (v_texId < {}.5) ", i);
				}
				else if (i == maxTextureImageUnits - 1) {
					fmt::format_to(fmt::appender(buf), "else ");
				}
				else {
					fmt::format_to(fmt::appender(buf), "else if (v_texId < {}.5)", i);
				}
				fmt::format_to(fmt::appender(buf), "color = texture2D(s_tex[{}], v_texcoord);\n", i);
			}
			textureSwitch = to_string(buf);
		}
		std::string fragSource = fmt::format(s_tintedTextureFragmentTemplate,
			fmt::arg("SG_TEXTURE_COUNT", maxTextureImageUnits),
			fmt::arg("SG_TEXTURE_SWITCH", textureSwitch));
		char const* fragSourcePtr = fragSource.c_str();
		glShaderSource(fs, 1, &fragSourcePtr, nullptr);
		glCompileShader(fs);
		if (!GetShaderCompileSuccess(fs)) {
			std::string log = GetShaderInfoLog(fs);
			sys->Error("Failed to compile fragment shader:\n%s", log.c_str());
		}

		glAttachShader(prog, vs);
		glAttachShader(prog, fs);
		glLinkProgram(prog);
		if (!GetProgramLinkSuccess(prog)) {
			std::string log = GetProgramInfoLog(prog);
			sys->Error("Failed to link program:\n%s", log.c_str());
		}
		glDeleteShader(vs);
		glDeleteShader(fs);
		tintedTextureProgram = prog;
	}

	// Initialise layer array
	numLayer = 1;
	layerListSize = 16;
	layerList = new r_layer_c * [layerListSize];
	layerList[0] = new r_layer_c(this, 0, 0);

	// Initialise layer command bin
	layerCmdBinCount = 0;
	layerCmdBinSize = 1024;
	layerCmdBin = new r_layerCmd_s * [layerCmdBinSize];

	takeScreenshot = R_SSNONE;

	// Set up DPI-scaling render target
	{
		glGenFramebuffers(1, &rttMain.framebuffer);
		glGenTextures(1, &rttMain.colorTexture);

		auto compileShader = [](std::string_view src, GLenum type) -> GLuint {
			GLuint id = glCreateShader(type);
			auto sourcePtr = src.data();
			glShaderSource(id, 1, &sourcePtr, nullptr);
			glCompileShader(id);
			return id;
			};

		auto vsId = compileShader(s_scaleVsSource, GL_VERTEX_SHADER);
		if (!GetShaderCompileSuccess(vsId)) {
			auto log = GetShaderInfoLog(vsId);
			sys->con->Printf("Scaling VS compile failure: %s\n", log.c_str());
		}
		auto fsId = compileShader(s_scaleFsSource, GL_FRAGMENT_SHADER);
		if (!GetShaderCompileSuccess(fsId)) {
			auto log = GetShaderInfoLog(fsId);
			sys->con->Printf("Scaling FS compile failure: %s\n", log.c_str());
		}

		GLuint prog = rttMain.blitProg = glCreateProgram();
		glAttachShader(prog, vsId);
		glAttachShader(prog, fsId);
		glLinkProgram(prog);
		if (!GetProgramLinkSuccess(prog)) {
			auto log = GetProgramInfoLog(prog);
			sys->con->Printf("Scaling program link failure: %s\n", log.c_str());
		}

		GLint linked = GL_FALSE;
		glGetProgramiv(prog, GL_LINK_STATUS, &linked);

		glDeleteShader(vsId);
		glDeleteShader(fsId);

		rttMain.blitAttribLocPos = glGetAttribLocation(prog, "a_position");
		rttMain.blitAttibLocTC = glGetAttribLocation(prog, "a_texcoord");
		rttMain.blitSampleLocColour = glGetUniformLocation(prog, "s_tex");
	}

	// Load render resources
	sys->con->Printf("Loading resources...\n");

	whiteImage = RegisterShader("@white", 0);

	imguiCtx = ImGui::CreateContext();
	ImGui::SetCurrentContext(imguiCtx);

	ImGui_ImplGlfw_InitForOpenGL((GLFWwindow*)sys->video->GetWindowHandle(), true);
	ImGui_ImplOpenGL3_Init("#version 100");

	fonts[F_FIXED] = new r_font_c(this, "Bitstream Vera Sans Mono");
	fonts[F_VAR] = new r_font_c(this, "Liberation Sans");
	fonts[F_VAR_BOLD] = new r_font_c(this, "Liberation Sans Bold");

	sys->con->Printf("Renderer initialised in %d msec.\n", timer.Get());
}

void r_renderer_c::Shutdown()
{
	sys->con->PrintFunc("Render Shutdown");

	sys->con->Printf("Unloading resources...\n");

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext(imguiCtx);

	delete whiteImage;

	for (int f = 0; f < F_NUMFONTS; f++) {
		delete fonts[f];
	}

	for (int s = 0; s < numShader; s++) {
		delete shaderList[s];
	}

	for (int l = 0; l < numLayer; l++) {
		delete layerList[l];
	}
	delete layerList;
	for (int c = 0; c < layerCmdBinCount; c++) {
		delete layerCmdBin[c];
	}
	delete layerCmdBin;

	glDeleteTextures(1, &rttMain.colorTexture);
	glDeleteFramebuffers(1, &rttMain.framebuffer);
	glDeleteProgram(rttMain.blitProg);

	// Shutdown texture manager
	r_ITexManager::FreeHandle(texMan);

	// Shutdown OpenGL
	openGL->Shutdown();
	sys_IOpenGL::FreeHandle(openGL);

	sys->con->Printf("Renderer shutdown complete.\n");
}

// =================
// Render Management
// =================

void r_renderer_c::BeginFrame()
{
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
	{
		auto& vid = sys->video->vid;
		int wNew = VirtualScreenWidth();
		int hNew = VirtualScreenHeight();
		if (rttMain.width != wNew || rttMain.height != hNew) {
			GLint prevTex2D, prevFB;
			glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTex2D);
			glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFB);
			glBindTexture(GL_TEXTURE_2D, rttMain.colorTexture);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, wNew, hNew, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

			rttMain.width = wNew;
			rttMain.height = hNew;

			glBindFramebuffer(GL_FRAMEBUFFER, rttMain.framebuffer);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rttMain.colorTexture, 0);

			glCheckFramebufferStatus(GL_FRAMEBUFFER);

			glBindFramebuffer(GL_FRAMEBUFFER, prevFB);
			glBindTexture(GL_TEXTURE_2D, prevTex2D);
		}
	}

	curLayer = layerList[0];

	SetViewport();
	SetBlendMode(RB_ALPHA);
	DrawColor();
}

static int layerCompFunc(const void* va, const void* vb)
{
	r_layer_c* a = *(r_layer_c**)va;
	r_layer_c* b = *(r_layer_c**)vb;
	if (a->layer < b->layer) {
		return -1;
	}
	else if (a->layer > b->layer) {
		return 1;
	}
	else if (a->subLayer < b->subLayer) {
		return -1;
	}
	else {
		return 1;
	}
}

void CVarSliderInt(char const* label, conVar_c* cvar) {
	int curOpt = cvar->intVal;
	if (ImGui::SliderInt(label, &curOpt, cvar->min, cvar->max, "%d", ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_NoInput)) {
		if (curOpt != cvar->intVal) {
			cvar->Set(curOpt);
		}
	}
}

void CVarCheckbox(char const* label, conVar_c* cvar) {
	bool checked = cvar->intVal == 1;
	if (ImGui::Checkbox(label, &checked)) {
		cvar->intVal = +checked;
	}
}

void r_renderer_c::EndFrame()
{
	static bool showDemo = false;
	static bool showMetrics = false;
	if (debugImGui) {
		if (ImGui::Begin("Debug Hub", &debugImGui)) {
			if (ImGui::Button("Demo")) {
				showDemo = true;
			}
			if (ImGui::Button("Metrics")) {
				showMetrics = true;
			}
			if (ImGui::Button("Layers")) {
				debugLayers = true;
			}
		}
		ImGui::End();
	}

	if (showDemo) {
		ImGui::ShowDemoWindow(&showDemo);
	}
	if (showMetrics) {
		ImGui::ShowMetricsWindow(&showMetrics);
	}

	r_layer_c** layerSort = new r_layer_c * [numLayer];
	for (int l = 0; l < numLayer; l++) {
		layerSort[l] = layerList[l];
	}
	qsort(layerSort, numLayer, sizeof(r_layer_c*), layerCompFunc);
	if (r_layerDebug->intVal) {
		int totalCmd = 0;
		for (int l = 0; l < numLayer; l++) {
			totalCmd += layerSort[l]->numCmd;
			char str[1024];
			sprintf(str, "%d (%4d,%4d) [%2d]", layerSort[l]->numCmd, layerSort[l]->layer, layerSort[l]->subLayer, l);
			float w = (float)DrawStringWidth(16, F_FIXED, str);
			DrawColor(0x7F000000);
			DrawImage(NULL, (float)VirtualScreenWidth() - w, VirtualScreenHeight() - (l + 2) * 16.0f, w, 16);
			DrawStringFormat(0, VirtualScreenHeight() - (l + 2) * 16.0f, F_RIGHT, 16, colorWhite, F_FIXED, str);
		}
		char str[1024];
		sprintf(str, "%d", totalCmd);
		float w = (float)DrawStringWidth(16, F_FIXED, str);
		DrawColor(0xAF000000);
		DrawImage(NULL, (float)VirtualScreenWidth() - w, VirtualScreenHeight() - 16.0f, w, 16);
		DrawStringFormat(0, VirtualScreenHeight() - 16.0f, F_RIGHT, 16, colorWhite, F_FIXED, str);
	}

	std::optional<std::pair<int, int>> layerBreak;
	if (debugLayers) {
		if (ImGui::Begin("Layers", &debugLayers)) {
			ImGui::Text("Layers: %d", numLayer);
			ImGui::Text("%d out of %d frames drawn, %d saved.", drawnFrames, totalFrames, savedFrames);
			CVarSliderInt("Optimization", r_layerOptimize);
			CVarCheckbox("Elide identical frames", r_elideFrames);
			CVarCheckbox("Draw command culling", r_drawCull);

			int totalCmd{};
			if (ImGui::BeginTable("Layer stats", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit)) {
				ImGui::TableSetupColumn("Index");
				ImGui::TableSetupColumn("Layer");
				ImGui::TableSetupColumn("Sublayer");
				ImGui::TableSetupColumn("Command count");
				ImGui::TableSetupColumn("Debug");
				ImGui::TableHeadersRow();
				for (int l = 0; l < numLayer; ++l) {
					auto layer = layerSort[l];
					ImGui::PushID(layer->layer);
					ImGui::PushID(layer->subLayer);
					totalCmd += layer->numCmd;
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::Text("%d", l);
					ImGui::TableNextColumn();
					ImGui::Text("%d", layer->layer);
					ImGui::TableNextColumn();
					ImGui::Text("%d", layer->subLayer);
					ImGui::TableNextColumn();
					ImGui::Text("%d", layer->numCmd);
					ImGui::TableNextColumn();
					if (ImGui::Button("Debug")) {
						layerBreak = { layer->layer, layer->subLayer };
					}
					ImGui::PopID();
					ImGui::PopID();
				}
				ImGui::EndTable();
			}
		}
		ImGui::End();
	}

	if (elideFrames != !!r_elideFrames->intVal) {
		elideFrames = !!r_elideFrames->intVal;
		lastFrameHash.clear();
	}

	std::vector<uint8_t> commandDigest(crypto_shorthash_bytes());
	if (elideFrames) {
		{
			std::vector<char> commandBytes;
			commandBytes.reserve(1ull << 24);
			auto hash_primitive = [&](auto& x) {
				auto p = (uint8_t const*)&x;
				commandBytes.insert(commandBytes.end(), p, p + sizeof(x));
				};
			for (auto lIdx = 0; lIdx < numLayer; ++lIdx) {
				auto layer = layerSort[lIdx];
				hash_primitive(layer->layer);
				hash_primitive(layer->subLayer);
				for (auto cIdx = 0; cIdx < layer->numCmd; ++cIdx) {
					auto cmd = layer->cmdList[cIdx];
					hash_primitive(cmd->cmd);
					switch (cmd->cmd) {
					case r_layerCmd_s::VIEWPORT: {
						hash_primitive(cmd->viewport);
					} break;
					case r_layerCmd_s::BLEND: {
						hash_primitive(cmd->blendMode);
					} break;
					case r_layerCmd_s::BIND: {
						// not safe around handle/ID reuse but probably good enough
						hash_primitive(cmd->tex);
						hash_primitive(cmd->tex->texId);
						auto status = cmd->tex->status.load();
						hash_primitive(status);
					} break;
					case r_layerCmd_s::COLOR: {
						for (auto comp : cmd->col) {
							hash_primitive(comp);
						}
					} break;
					case r_layerCmd_s::QUAD: {
						hash_primitive(cmd->quad);
					} break;
					}
				}
			}
			static std::vector<uint8_t> const commandKey(crypto_shorthash_keybytes());
			crypto_shorthash((unsigned char*)commandDigest.data(), (unsigned char const*)commandBytes.data(), commandBytes.size(), (unsigned char const*)commandKey.data());
		}
	}

	++totalFrames;
	if (!elideFrames || lastFrameHash != commandDigest) {
		lastFrameHash = commandDigest;
		++drawnFrames;

		glBindFramebuffer(GL_FRAMEBUFFER, rttMain.framebuffer);
		glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
		for (int l = 0; l < numLayer; l++) {
			auto& layer = layerSort[l];
			if (layerBreak && layerBreak->first == layer->layer && layerBreak->second == layer->subLayer) {
#ifdef _WIN32
				DebugBreak();
#endif
			}
			layer->Render();
		}
	}
	else {
		++savedFrames;
		for (int l = 0; l < numLayer; l++) {
			layerSort[l]->Discard();
		}
	}
	delete[] layerSort;

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	float blitTriPos[] = {
		-1.0f, -1.0f, //
		3.0f, -1.0f, //
		-1.0f, 3.0f, //
	};
	float blitTriUV[] = {
		0.0f, 0.0f, //
		2.0f, 0.0f, //
		0.0f, 2.0f, //
	};

	glViewport(0, 0, sys->video->vid.fbSize[0], sys->video->vid.fbSize[1]);
	glUseProgram(rttMain.blitProg);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, std::data(blitTriPos));
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, std::data(blitTriUV));
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glBindTexture(GL_TEXTURE_2D, rttMain.colorTexture);
	glUniform1i(rttMain.blitSampleLocColour, 0);
	glDrawArrays(GL_TRIANGLES, 0, 3);
	glBindTexture(GL_TEXTURE_2D, 0);
	glUseProgram(0);

	glFlush();

	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	// Swap output buffers
	openGL->Swap();

	// Take screenshot
	switch (takeScreenshot) {
	case R_SSTGA:
	{
		targa_c i(sys->con);
		i.type = IMGTYPE_RGB;
		DoScreenshot(&i, "tga");
	}
	break;
	case R_SSJPEG:
	{
		jpeg_c i(sys->con);
		i.type = IMGTYPE_RGB;
		DoScreenshot(&i, "jpg");
	}
	break;
	case R_SSPNG:
	{
		png_c i(sys->con);
		i.type = IMGTYPE_RGB;
		DoScreenshot(&i, "png");
	}
	break;
	}
	takeScreenshot = R_SSNONE;

	PurgeShaders();
}

// =================
// Shader Management
// =================

void r_renderer_c::PurgeShaders()
{
	// Delete released shaders
	for (int s = 0; s < numShader; s++) {
		if (shaderList[s] && shaderList[s]->refCount == 0 && shaderList[s]->tex->status == r_tex_c::DONE) {
			delete shaderList[s];
			shaderList[s] = NULL;
		}
	}
}

r_shaderHnd_c* r_renderer_c::RegisterShader(const char* shname, int flags)
{
	if (*shname == 0) {
		return NULL;
	}

	dword nameHash = StringHash(shname, 0xFFFF);
	int newId = -1;
	for (int s = 0; s < numShader; s++) {
		if (!shaderList[s]) {
			newId = s;
		}
		else if (shaderList[s]->nameHash == nameHash && _stricmp(shname, shaderList[s]->name) == 0 && shaderList[s]->tex->flags == flags) {
			// Shader already exists, return a new handle for it
			// Ensure texture is loaded as soon as possible
			shaderList[s]->tex->ForceLoad();
			return new r_shaderHnd_c(shaderList[s]);
		}
	}
	if (newId == -1) {
		if (numShader == R_MAXSHADERS) {
			sys->con->Warning("shader limit reached");
			return NULL;
		}
		newId = numShader++;
	}
	shaderList[newId] = new r_shader_c(this, shname, flags);
	return new r_shaderHnd_c(shaderList[newId]);
}

r_shaderHnd_c* r_renderer_c::RegisterShaderFromData(int width, int height, int type, byte* dat, int flags)
{
	int newId = -1;
	for (int s = 0; s < numShader; s++) {
		if (!shaderList[s]) {
			newId = s;
			break;
		}
	}
	if (newId == -1) {
		if (numShader == R_MAXSHADERS) {
			sys->con->Warning("shader limit reached");
			return NULL;
		}
		newId = numShader++;
	}
	char shname[32];
	sprintf(shname, "data:%d", newId);
	shaderList[newId] = new r_shader_c(this, shname, flags, width, height, type, dat);
	return new r_shaderHnd_c(shaderList[newId]);
}

void r_renderer_c::GetShaderImageSize(r_shaderHnd_c* hnd, int& width, int& height)
{
	if (hnd && hnd->sh->tex->status == r_tex_c::DONE) {
		width = hnd->sh->tex->fileWidth;
		height = hnd->sh->tex->fileHeight;
	}
	else {
		width = 0;
		height = 0;
	}
}

void r_renderer_c::SetShaderLoadingPriority(r_shaderHnd_c* hnd, int pri)
{
	if (hnd && hnd->sh->tex->status != r_tex_c::DONE) {
		hnd->sh->tex->loadPri = pri;
	}
}

int r_renderer_c::GetTexAsyncCount()
{
	return texMan->GetAsyncCount();
}

// ==========
// 2D Drawing
// ==========

void r_renderer_c::SetClearColor(const col4_t col)
{
	glClearColor(col[0], col[1], col[2], col[3]);
}

void r_renderer_c::SetDrawLayer(int layer, int subLayer)
{
	if (layer == curLayer->layer && subLayer == curLayer->subLayer) {
		return;
	}
	r_layer_c* newCurLayer = NULL;
	for (int l = 0; l < numLayer; l++) {
		if (layerList[l]->layer == layer && layerList[l]->subLayer == subLayer) {
			newCurLayer = layerList[l];
			break;
		}
	}
	if (!newCurLayer) {
		if (numLayer == layerListSize) {
			layerListSize <<= 1;
			trealloc(layerList, layerListSize);
		}
		layerList[numLayer] = new r_layer_c(this, layer, subLayer);
		newCurLayer = layerList[numLayer];
		numLayer++;
	}
	curLayer = newCurLayer;
	curLayer->SetViewport(&curViewport);
	curLayer->SetBlendMode(curBlendMode);
}

void r_renderer_c::SetDrawSubLayer(int subLayer)
{
	SetDrawLayer(curLayer->layer, subLayer);
}

int r_renderer_c::GetDrawLayer()
{
	return curLayer->subLayer;
}

void r_renderer_c::SetViewport(int x, int y, int width, int height)
{
	if (height == 0) {
		auto& vid = sys->video->vid;
		width = VirtualScreenWidth();
		height = VirtualScreenHeight();
	}
	curViewport.x = x;
	curViewport.y = y;
	curViewport.width = width;
	curViewport.height = height;
	curLayer->SetViewport(&curViewport);
}

void r_renderer_c::SetBlendMode(int mode)
{
	curBlendMode = mode;
	curLayer->SetBlendMode(mode);
}

void r_renderer_c::DrawColor(const col4_t col)
{
	if (col) {
		Vector4Copy(col, drawColor);
	}
	else {
		drawColor[0] = 1.0f;
		drawColor[1] = 1.0f;
		drawColor[2] = 1.0f;
		drawColor[3] = 1.0f;
	}
}

void r_renderer_c::DrawColor(dword col)
{
	drawColor[0] = ((col >> 16) & 0xFF) / 255.0f;
	drawColor[1] = ((col >> 8) & 0xFF) / 255.0f;
	drawColor[2] = (col & 0xFF) / 255.0f;
	drawColor[3] = (col >> 24) / 255.0f;
}

void r_renderer_c::DrawImage(r_shaderHnd_c* hnd, float x, float y, float w, float h, float s1, float t1, float s2, float t2)
{
	DrawImageQuad(hnd, x, y, x + w, y, x + w, y + h, x, y + h, s1, t1, s2, t1, s2, t2, s1, t2);
}

void r_renderer_c::DrawImageQuad(r_shaderHnd_c* hnd, float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3, float s0, float t0, float s1, float t1, float s2, float t2, float s3, float t3)
{
	if (hnd) {
		curLayer->Bind(hnd->sh->tex);
	}
	else {
		curLayer->Bind(whiteImage->sh->tex);
	}
	curLayer->Color(drawColor);
	curLayer->Quad(s0, t0, x0, y0, s1, t1, x1, y1, s2, t2, x2, y2, s3, t3, x3, y3);
}

void r_renderer_c::DrawString(float x, float y, int align, int height, const col4_t col, int font, const char* str)
{
	if (font < 0 || font >= F_NUMFONTS) {
		font = F_FIXED;
	}

	scp_t pos = { x, y };
	if (col) {
		col4_t tcol;
		Vector4Copy(col, tcol);
		fonts[font]->Draw(pos, align, height, tcol, str);
	}
	else {
		fonts[font]->Draw(pos, align, height, drawColor, str);
	}
}

void r_renderer_c::DrawStringFormat(float x, float y, int align, int height, const col4_t col, int font, const char* fmt, ...)
{
	if (font < 0 || font >= F_NUMFONTS) {
		font = F_FIXED;
	}

	va_list va;
	va_start(va, fmt);

	scp_t pos = { x, y };
	if (col) {
		col4_t tcol;
		Vector4Copy(col, tcol);
		fonts[font]->VDraw(pos, align, height, tcol, fmt, va);
	}
	else {
		fonts[font]->VDraw(pos, align, height, drawColor, fmt, va);
	}

	va_end(va);
}

int	r_renderer_c::DrawStringWidth(int height, int font, const char* str)
{
	if (font < 0 || font >= F_NUMFONTS) {
		font = F_FIXED;
	}
	return fonts[font]->StringWidth(height, str);
}

int r_renderer_c::DrawStringCursorIndex(int height, int font, const char* str, int curX, int curY)
{
	if (font < 0 || font >= F_NUMFONTS) {
		font = F_FIXED;
	}
	return fonts[font]->StringCursorIndex(height, str, curX, curY);
}

// ==============
// Virtual screen
// ==============

int r_renderer_c::VirtualScreenWidth() {
	return VirtualMap(sys->video->vid.size[0]);
}

int r_renderer_c::VirtualScreenHeight() {
	return VirtualMap(sys->video->vid.size[1]);
}

float r_renderer_c::VirtualScreenScaleFactor() {
	if (apiDpiAware) {
		return sys->video->vid.dpiScale;
	}
	return 1.0f;
}

int r_renderer_c::VirtualMap(int properValue) {
	if (apiDpiAware) {
		return properValue;
	}
	return (int)(properValue / sys->video->vid.dpiScale);
}

int r_renderer_c::VirtualUnmap(int mappedValue) {
	if (apiDpiAware) {
		return mappedValue;
	}
	return (int)(mappedValue * sys->video->vid.dpiScale);
}

// =====
// Debug
// =====

void r_renderer_c::ToggleDebugImGui() {
	debugImGui = !debugImGui;
}

// ===========
// Screenshots
// ===========

void r_renderer_c::C_Screenshot(IConsole* conHnd, args_c& args)
{
	const char* fmtName = args.argc >= 2 ? args.argv[1] : r_screenshotFormat->strVal.c_str();
	takeScreenshot = R_SSNONE;
	if (!_stricmp(fmtName, "tga")) {
		takeScreenshot = R_SSTGA;
	}
	else if (!_stricmp(fmtName, "jpg") || !_stricmp(fmtName, "jpeg")) {
		takeScreenshot = R_SSJPEG;
	}
	else if (!_stricmp(fmtName, "png")) {
		takeScreenshot = R_SSPNG;
	}
	else {
		conHnd->Warning("Unknown screenshot format '%s', valid formats: jpg, tga, png", fmtName);
	}
}

void r_renderer_c::DoScreenshot(image_c* i, const char* ext)
{
	int xs = sys->video->vid.size[0];
	int ys = sys->video->vid.size[1];

	int size = xs * ys * 3;
	byte* sbuf = new byte[size];

	// Read the front buffer
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0, 0, xs, ys, r_tex_c::GLTypeForImgType(i->type), GL_UNSIGNED_BYTE, sbuf);

	// Flip the image
	int	span = xs * 3;
	byte* ss = new byte[size];
	byte* p1 = sbuf;
	byte* p2 = ss + size - span;
	for (int y = 0; y < ys; y++, p1 += span, p2 -= span) {
		memcpy(p2, p1, span);
	}
	delete[] sbuf;

	// Set image info
	i->dat = ss;
	i->width = xs;
	i->height = ys;
	i->comp = 3;

	time_t curTime;
	time(&curTime);
	std::string ssname = fmt::format(CFG_DATAPATH "Screenshots/{:%m%d%y_%H%M%S}.{}",
		fmt::localtime(curTime), ext);
	// curTimeSt.tm_mon+1, curTimeSt.tm_mday, curTimeSt.tm_year%100,
	// curTimeSt.tm_hour, curTimeSt.tm_min, curTimeSt.tm_sec, ext);

	// Make folder if it doesn't exist
	std::filesystem::create_directory(CFG_DATAPATH "Screenshots");
	
	// Save image
	if (i->Save(ssname.c_str())) {
		sys->con->Print("Couldn't write screenshot!\n");
	}
	else {
		sys->con->Print(fmt::format("Wrote screenshot to {}\n", ssname).c_str());
	}
}
