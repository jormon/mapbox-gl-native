#ifndef MBGL_RENDERER_GL_CONFIG
#define MBGL_RENDERER_GL_CONFIG

#include <cstdint>
#include <tuple>
#include <array>

#include <mbgl/platform/gl.hpp>

namespace mbgl {
namespace gl {

template <typename T>
class Value {
public:
    inline void operator=(const typename T::Type& value) {
        if (dirty || current != value) {
            dirty = false;
            current = value;
            T::Set(current);
        }
    }

    inline void reset() {
        operator=(T::Default);
    }

    inline void setDirty() {
        dirty = true;
    }

private:
    typename T::Type current = T::Default;
    bool dirty = false;
};

struct ClearDepth {
    using Type = GLfloat;
    static const Type Default;
    inline static void Set(const Type& value) {
        MBGL_CHECK_ERROR(glClearDepth(value));
    }
};

struct ClearColor {
    struct Type { GLfloat r, g, b, a; };
    static const Type Default;
    inline static void Set(const Type& value) {
        MBGL_CHECK_ERROR(glClearColor(value.r, value.g, value.b, value.a));
    }
};

inline bool operator!=(const ClearColor::Type& a, const ClearColor::Type& b) {
    return a.r != b.r || a.g != b.g || a.b != b.b || a.a != b.a;
}

struct ClearStencil {
    using Type = GLint;
    static const Type Default;
    inline static void Set(const Type& value) {
        MBGL_CHECK_ERROR(glClearStencil(value));
    }
};

struct StencilMask {
    using Type = GLuint;
    static const Type Default;
    inline static void Set(const Type& value) {
        MBGL_CHECK_ERROR(glStencilMask(value));
    }
};

struct DepthMask {
    using Type = GLboolean;
    static const Type Default;
    inline static void Set(const Type& value) {
        MBGL_CHECK_ERROR(glDepthMask(value));
    }
};

struct ColorMask {
    struct Type { bool r, g, b, a; };
    static const Type Default;
    inline static void Set(const Type& value) {
        MBGL_CHECK_ERROR(glColorMask(value.r, value.g, value.b, value.a));
    }
};

inline bool operator!=(const ColorMask::Type& a, const ColorMask::Type& b) {
    return a.r != b.r || a.g != b.g || a.b != b.b || a.a != b.a;
}

struct StencilFunc {
    struct Type { GLenum func; GLint ref; GLuint mask; };
    static const Type Default;
    inline static void Set(const Type& value) {
        MBGL_CHECK_ERROR(glStencilFunc(value.func, value.ref, value.mask));
    }
};

inline bool operator!=(const StencilFunc::Type& a, const StencilFunc::Type& b) {
    return a.func != b.func || a.ref != b.ref || a.mask != b.mask;
}

struct StencilTest {
    using Type = bool;
    static const Type Default;
    inline static void Set(const Type& value) {
        MBGL_CHECK_ERROR(value ? glEnable(GL_STENCIL_TEST) : glDisable(GL_STENCIL_TEST));
    }
};

struct StencilOp {
    struct Type { GLenum sfail, dpfail, dppass; };
    static const Type Default;
    inline static void Set(const Type& value) {
        MBGL_CHECK_ERROR(glStencilOp(value.sfail, value.dpfail, value.dppass));
    }
};

inline bool operator!=(const StencilOp::Type& a, const StencilOp::Type& b) {
    return a.sfail != b.sfail || a.dpfail != b.dpfail || a.dppass != b.dppass;
}

struct DepthRange {
    struct Type { GLfloat near, far; };
    static const Type Default;
    inline static void Set(const Type& value) {
        MBGL_CHECK_ERROR(glDepthRange(value.near, value.far));
    }
};

inline bool operator!=(const DepthRange::Type& a, const DepthRange::Type& b) {
    return a.near != b.near || a.far != b.far;
}

struct DepthTest {
    using Type = bool;
    static const Type Default;
    inline static void Set(const Type& value) {
        MBGL_CHECK_ERROR(value ? glEnable(GL_DEPTH_TEST) : glDisable(GL_DEPTH_TEST));
    }
};

struct DepthFunc {
    using Type = GLenum;
    static const Type Default;
    inline static void Set(const Type& value) {
        MBGL_CHECK_ERROR(glDepthFunc(value));
    }
};

struct Blend {
    using Type = bool;
    static const Type Default;
    inline static void Set(const Type& value) {
        MBGL_CHECK_ERROR(value ? glEnable(GL_BLEND) : glDisable(GL_BLEND));
    }
};

struct BlendFunc {
    struct Type { GLenum sfactor, dfactor; };
    static const Type Default;
    inline static void Set(const Type& value) {
        MBGL_CHECK_ERROR(glBlendFunc(value.sfactor, value.dfactor));
    }
};

inline bool operator!=(const BlendFunc::Type& a, const BlendFunc::Type& b) {
    return a.sfactor != b.sfactor || a.dfactor != b.dfactor;
}

struct Program {
    using Type = GLuint;
    static const Type Default;
    inline static void Set(const Type& value) {
        MBGL_CHECK_ERROR(glUseProgram(value));
    }
};

struct LineWidth {
    using Type = GLfloat;
    static const Type Default;
    inline static void Set(const Type& value) {
        MBGL_CHECK_ERROR(glLineWidth(value));
    }
};

class Config {
public:
    void reset() {
        stencilFunc.reset();
        stencilMask.reset();
        stencilTest.reset();
        stencilOp.reset();
        depthRange.reset();
        depthMask.reset();
        depthTest.reset();
        depthFunc.reset();
        blend.reset();
        blendFunc.reset();
        colorMask.reset();
        clearDepth.reset();
        clearColor.reset();
        clearStencil.reset();
        program.reset();
        lineWidth.reset();
    }

    void setDirty() {
        stencilFunc.setDirty();
        stencilMask.setDirty();
        stencilTest.setDirty();
        stencilOp.setDirty();
        depthRange.setDirty();
        depthMask.setDirty();
        depthTest.setDirty();
        depthFunc.setDirty();
        blend.setDirty();
        blendFunc.setDirty();
        colorMask.setDirty();
        clearDepth.setDirty();
        clearColor.setDirty();
        clearStencil.setDirty();
        program.setDirty();
        lineWidth.setDirty();
    }

    Value<StencilFunc> stencilFunc;
    Value<StencilMask> stencilMask;
    Value<StencilTest> stencilTest;
    Value<StencilOp> stencilOp;
    Value<DepthRange> depthRange;
    Value<DepthMask> depthMask;
    Value<DepthTest> depthTest;
    Value<DepthFunc> depthFunc;
    Value<Blend> blend;
    Value<BlendFunc> blendFunc;
    Value<ColorMask> colorMask;
    Value<ClearDepth> clearDepth;
    Value<ClearColor> clearColor;
    Value<ClearStencil> clearStencil;
    Value<Program> program;
    Value<LineWidth> lineWidth;
};

} // namespace gl
} // namespace mbgl

#endif // MBGL_RENDERER_GL_CONFIG
