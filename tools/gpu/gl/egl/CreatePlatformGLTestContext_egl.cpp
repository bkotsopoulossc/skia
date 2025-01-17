/*
 * Copyright 2011 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/gpu/gl/GrGLDefines.h"
#include "src/gpu/gl/GrGLUtil.h"
#include "tools/gpu/gl/GLTestContext.h"

#define GL_GLEXT_PROTOTYPES
#include <EGL/egl.h>
#include <EGL/eglext.h>

namespace {

std::function<void()> context_restorer() {
    auto display = eglGetCurrentDisplay();
    auto dsurface = eglGetCurrentSurface(EGL_DRAW);
    auto rsurface = eglGetCurrentSurface(EGL_READ);
    auto context = eglGetCurrentContext();
    return [display, dsurface, rsurface, context] {
        eglMakeCurrent(display, dsurface, rsurface, context);
    };
}

class EGLGLTestContext : public sk_gpu_test::GLTestContext {
public:
    EGLGLTestContext(GrGLStandard forcedGpuAPI, EGLGLTestContext* shareContext);
    ~EGLGLTestContext() override;

    GrEGLImage texture2DToEGLImage(GrGLuint texID) const override;
    void destroyEGLImage(GrEGLImage) const override;
    GrGLuint eglImageToExternalTexture(GrEGLImage) const override;
    std::unique_ptr<sk_gpu_test::GLTestContext> makeNew() const override;

private:
    void destroyGLContext();

    void onPlatformMakeNotCurrent() const override;
    void onPlatformMakeCurrent() const override;
    std::function<void()> onPlatformGetAutoContextRestore() const override;
    GrGLFuncPtr onPlatformGetProcAddress(const char*) const override;

    void setupFenceSync(sk_sp<const GrGLInterface>);

    PFNEGLCREATEIMAGEKHRPROC fEglCreateImageProc = nullptr;
    PFNEGLDESTROYIMAGEKHRPROC fEglDestroyImageProc = nullptr;

    EGLContext fContext;
    EGLDisplay fDisplay;
    EGLSurface fSurface;
};

static EGLContext create_gles_egl_context(EGLDisplay display,
                                          EGLConfig surfaceConfig,
                                          EGLContext eglShareContext,
                                          EGLint eglContextClientVersion) {
    const EGLint contextAttribsForOpenGLES[] = {
        EGL_CONTEXT_CLIENT_VERSION,
        eglContextClientVersion,
        EGL_NONE
    };
    return eglCreateContext(display, surfaceConfig, eglShareContext, contextAttribsForOpenGLES);
}
static EGLContext create_gl_egl_context(EGLDisplay display,
                                        EGLConfig surfaceConfig,
                                        EGLContext eglShareContext) {
    const EGLint contextAttribsForOpenGL[] = {
        EGL_NONE
    };
    return eglCreateContext(display, surfaceConfig, eglShareContext, contextAttribsForOpenGL);
}

EGLGLTestContext::EGLGLTestContext(GrGLStandard forcedGpuAPI, EGLGLTestContext* shareContext)
    : fContext(EGL_NO_CONTEXT)
    , fDisplay(EGL_NO_DISPLAY)
    , fSurface(EGL_NO_SURFACE) {

    EGLContext eglShareContext = shareContext ? shareContext->fContext : nullptr;

    static const GrGLStandard kStandards[] = {
        kGL_GrGLStandard,
        kGLES_GrGLStandard,
    };

    size_t apiLimit = SK_ARRAY_COUNT(kStandards);
    size_t api = 0;
    if (forcedGpuAPI == kGL_GrGLStandard) {
        apiLimit = 1;
    } else if (forcedGpuAPI == kGLES_GrGLStandard) {
        api = 1;
    }
    SkASSERT(forcedGpuAPI == kNone_GrGLStandard || kStandards[api] == forcedGpuAPI);

    sk_sp<const GrGLInterface> gl;

    for (; nullptr == gl.get() && api < apiLimit; ++api) {
        fDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);

        EGLint majorVersion;
        EGLint minorVersion;
        eglInitialize(fDisplay, &majorVersion, &minorVersion);

#if 0
        SkDebugf("VENDOR: %s\n", eglQueryString(fDisplay, EGL_VENDOR));
        SkDebugf("APIS: %s\n", eglQueryString(fDisplay, EGL_CLIENT_APIS));
        SkDebugf("VERSION: %s\n", eglQueryString(fDisplay, EGL_VERSION));
        SkDebugf("EXTENSIONS %s\n", eglQueryString(fDisplay, EGL_EXTENSIONS));
#endif
        bool gles = kGLES_GrGLStandard == kStandards[api];

        if (!eglBindAPI(gles ? EGL_OPENGL_ES_API : EGL_OPENGL_API)) {
            continue;
        }

        EGLint numConfigs = 0;
        const EGLint configAttribs[] = {
            EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
            EGL_RENDERABLE_TYPE, gles ? EGL_OPENGL_ES2_BIT : EGL_OPENGL_BIT,
            EGL_RED_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_BLUE_SIZE, 8,
            EGL_ALPHA_SIZE, 8,
            EGL_NONE
        };

        EGLConfig surfaceConfig;
        if (!eglChooseConfig(fDisplay, configAttribs, &surfaceConfig, 1, &numConfigs)) {
            SkDebugf("eglChooseConfig failed. EGL Error: 0x%08x\n", eglGetError());
            continue;
        }

        if (0 == numConfigs) {
            SkDebugf("No suitable EGL config found.\n");
            continue;
        }

        if (gles) {
#ifdef GR_EGL_TRY_GLES3_THEN_GLES2
            // Some older devices (Nexus7/Tegra3) crash when you try this.  So it is (for now)
            // hidden behind this flag.
            fContext = create_gles_egl_context(fDisplay, surfaceConfig, eglShareContext, 3);
            if (EGL_NO_CONTEXT == fContext) {
                fContext = create_gles_egl_context(fDisplay, surfaceConfig, eglShareContext, 2);
            }
#else
            fContext = create_gles_egl_context(fDisplay, surfaceConfig, eglShareContext, 2);
#endif
        } else {
            fContext = create_gl_egl_context(fDisplay, surfaceConfig, eglShareContext);
        }
        if (EGL_NO_CONTEXT == fContext) {
            SkDebugf("eglCreateContext failed.  EGL Error: 0x%08x\n", eglGetError());
            continue;
        }

        static const EGLint kSurfaceAttribs[] = {
            EGL_WIDTH, 1,
            EGL_HEIGHT, 1,
            EGL_NONE
        };

        fSurface = eglCreatePbufferSurface(fDisplay, surfaceConfig, kSurfaceAttribs);
        if (EGL_NO_SURFACE == fSurface) {
            SkDebugf("eglCreatePbufferSurface failed. EGL Error: 0x%08x\n", eglGetError());
            this->destroyGLContext();
            continue;
        }

        SkScopeExit restorer(context_restorer());
        if (!eglMakeCurrent(fDisplay, fSurface, fSurface, fContext)) {
            SkDebugf("eglMakeCurrent failed.  EGL Error: 0x%08x\n", eglGetError());
            this->destroyGLContext();
            continue;
        }

#ifdef SK_GL
        gl = GrGLMakeNativeInterface();
        if (!gl) {
            SkDebugf("Failed to create gl interface.\n");
            this->destroyGLContext();
            continue;
        }

        this->setupFenceSync(gl);

        if (!gl->validate()) {
            SkDebugf("Failed to validate gl interface.\n");
            this->destroyGLContext();
            continue;
        }
        const char* extensions = eglQueryString(fDisplay, EGL_EXTENSIONS);
        if (strstr(extensions, "EGL_KHR_image")) {
            fEglCreateImageProc = (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");
            fEglDestroyImageProc =
                    (PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR");
        }

        this->init(std::move(gl));
#else
        // Allow the GLTestContext creation to succeed without a GrGLInterface to support
        // GrContextFactory's persistent GL context workaround for Vulkan. We won't need the
        // GrGLInterface since we're not running the GL backend.
        this->init(nullptr);
#endif
        break;
    }
}

static bool supports_egl_extension(EGLDisplay display, const char* extension) {
    size_t extensionLength = strlen(extension);
    const char* extensionsStr = eglQueryString(display, EGL_EXTENSIONS);
    while (const char* match = strstr(extensionsStr, extension)) {
        // Ensure the string we found is its own extension, not a substring of a larger extension
        // (e.g. GL_ARB_occlusion_query / GL_ARB_occlusion_query2).
        if ((match == extensionsStr || match[-1] == ' ') &&
            (match[extensionLength] == ' ' || match[extensionLength] == '\0')) {
            return true;
        }
        extensionsStr = match + extensionLength;
    }
    return false;
}

void EGLGLTestContext::setupFenceSync(sk_sp<const GrGLInterface> interface) {
    GrGLInterface* glInt = const_cast<GrGLInterface*>(interface.get());


    if (kGL_GrGLStandard == glInt->fStandard) {
        if (GrGLGetVersion(glInt) >= GR_GL_VER(3,2) || glInt->hasExtension("GL_ARB_sync")) {
            return;
        }
    } else {
        if (glInt->hasExtension("GL_APPLE_sync") || glInt->hasExtension("GL_NV_fence") ||
            GrGLGetVersion(glInt) >= GR_GL_VER(3, 0)) {
            return;
        }
    }

    if (!supports_egl_extension(fDisplay, "EGL_KHR_fence_sync")) {
        return;
    }

    auto grEGLCreateSyncKHR = (PFNEGLCREATESYNCKHRPROC) eglGetProcAddress("eglCreateSyncKHR");
    auto grEGLClientWaitSyncKHR =
            (PFNEGLCLIENTWAITSYNCKHRPROC) eglGetProcAddress("eglClientWaitSyncKHR");
    auto grEGLDestroySyncKHR = (PFNEGLDESTROYSYNCKHRPROC) eglGetProcAddress("eglDestroySyncKHR");
    auto grEGLGetSyncAttribKHR =
            (PFNEGLGETSYNCATTRIBKHRPROC) eglGetProcAddress("eglGetSyncAttribKHR");
    SkASSERT(grEGLCreateSyncKHR && grEGLClientWaitSyncKHR && grEGLDestroySyncKHR &&
             grEGLGetSyncAttribKHR);

    PFNEGLWAITSYNCKHRPROC grEGLWaitSyncKHR = nullptr;
    if (supports_egl_extension(fDisplay, "EGL_KHR_wait_sync")) {
        grEGLWaitSyncKHR = (PFNEGLWAITSYNCKHRPROC)eglGetProcAddress("eglWaitSyncKHR");
        SkASSERT(grEGLWaitSyncKHR);
    }

    // Fake out glSync using eglSync
    glInt->fExtensions.add("GL_APPLE_sync");

    glInt->fFunctions.fFenceSync =
            [grEGLCreateSyncKHR, display = fDisplay](GrGLenum condition, GrGLbitfield flags) {
        SkASSERT(condition == GR_GL_SYNC_GPU_COMMANDS_COMPLETE);
        SkASSERT(flags == 0);

        EGLSyncKHR sync = grEGLCreateSyncKHR(display, EGL_SYNC_FENCE_KHR, nullptr);

        return reinterpret_cast<GrGLsync>(sync);
    };

    glInt->fFunctions.fDeleteSync = [grEGLDestroySyncKHR, display = fDisplay](GrGLsync sync) {
        EGLSyncKHR eglSync = reinterpret_cast<EGLSyncKHR>(sync);
        grEGLDestroySyncKHR(display, eglSync);
    };

    glInt->fFunctions.fClientWaitSync =
            [grEGLClientWaitSyncKHR, display = fDisplay, surface = fSurface] (
                    GrGLsync sync,
                    GrGLbitfield flags,
                    GrGLuint64 timeout) -> GrGLenum {
        EGLSyncKHR eglSync = reinterpret_cast<EGLSyncKHR>(sync);

        // It seems that, at least on the 2012 N7, later render passes will be reordered before a
        // fence. This really messes up benchmark timings where a large fraction of the work for
        // sample N can occur before the fence for sample N-1 signals. This causes sample N-1 to be
        // artificially slow and N artificially fast. Inserting a swap buffers (to the unused
        // display surface) blocks that reordering.
        eglSwapBuffers(display, surface);

        EGLint egl_flags = 0;
        if (flags & GR_GL_SYNC_FLUSH_COMMANDS_BIT) {
            egl_flags |= EGL_SYNC_FLUSH_COMMANDS_BIT_KHR;
        }

        EGLint result = grEGLClientWaitSyncKHR(display, eglSync, egl_flags, timeout);

        switch (result) {
            case EGL_CONDITION_SATISFIED_KHR:
                return GR_GL_CONDITION_SATISFIED;
            case EGL_TIMEOUT_EXPIRED_KHR:
                return GR_GL_TIMEOUT_EXPIRED;
            case EGL_FALSE:
                return GR_GL_WAIT_FAILED;
        }
        SkUNREACHABLE;
    };

    glInt->fFunctions.fWaitSync =
            [grEGLClientWaitSyncKHR, grEGLWaitSyncKHR, display = fDisplay](GrGLsync sync,
                                                                           GrGLbitfield flags,
                                                                           GrGLuint64 timeout) {
        EGLSyncKHR eglSync = reinterpret_cast<EGLSyncKHR>(sync);

        SkASSERT(timeout == GR_GL_TIMEOUT_IGNORED);
        SkASSERT(flags == 0);

        if (!grEGLWaitSyncKHR) {
            grEGLClientWaitSyncKHR(display, eglSync, 0, EGL_FOREVER_KHR);
            return;
        }

        SkDEBUGCODE(EGLint result =) grEGLWaitSyncKHR(display, eglSync, 0);
        SkASSERT(result);
    };

    glInt->fFunctions.fIsSync =
            [grEGLGetSyncAttribKHR, display = fDisplay](GrGLsync sync) -> GrGLboolean {
        EGLSyncKHR eglSync = reinterpret_cast<EGLSyncKHR>(sync);
        EGLint value;
        if (grEGLGetSyncAttribKHR(display, eglSync, EGL_SYNC_TYPE_KHR, &value)) {
            return true;
        }
        return false;
    };
}

EGLGLTestContext::~EGLGLTestContext() {
    this->teardown();
    this->destroyGLContext();
}

void EGLGLTestContext::destroyGLContext() {
    if (fDisplay) {
        if (fContext) {
            if (eglGetCurrentContext() == fContext) {
                // This will ensure that the context is immediately deleted.
                eglMakeCurrent(fDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            }
            eglDestroyContext(fDisplay, fContext);
            fContext = EGL_NO_CONTEXT;
        }

        if (fSurface) {
            eglDestroySurface(fDisplay, fSurface);
            fSurface = EGL_NO_SURFACE;
        }

        //TODO should we close the display?
        fDisplay = EGL_NO_DISPLAY;
    }
}

GrEGLImage EGLGLTestContext::texture2DToEGLImage(GrGLuint texID) const {
#ifdef SK_GL
    if (!this->gl()->hasExtension("EGL_KHR_gl_texture_2D_image") || !fEglCreateImageProc) {
        return GR_EGL_NO_IMAGE;
    }
    EGLint attribs[] = { GR_EGL_GL_TEXTURE_LEVEL, 0, GR_EGL_NONE };
    GrEGLClientBuffer clientBuffer = reinterpret_cast<GrEGLClientBuffer>(texID);
    return fEglCreateImageProc(fDisplay, fContext, GR_EGL_GL_TEXTURE_2D, clientBuffer, attribs);
#else
    (void)fEglCreateImageProc;
    return nullptr;
#endif
}

void EGLGLTestContext::destroyEGLImage(GrEGLImage image) const {
    fEglDestroyImageProc(fDisplay, image);
}

GrGLuint EGLGLTestContext::eglImageToExternalTexture(GrEGLImage image) const {
#ifdef SK_GL
    while (this->gl()->fFunctions.fGetError() != GR_GL_NO_ERROR) {}
    if (!this->gl()->hasExtension("GL_OES_EGL_image_external")) {
        return 0;
    }
    typedef GrGLvoid (*EGLImageTargetTexture2DProc)(GrGLenum, GrGLeglImage);

    EGLImageTargetTexture2DProc glEGLImageTargetTexture2D =
        (EGLImageTargetTexture2DProc) eglGetProcAddress("glEGLImageTargetTexture2DOES");
    if (!glEGLImageTargetTexture2D) {
        return 0;
    }
    GrGLuint texID;
    GR_GL_CALL(this->gl(), GenTextures(1, &texID));
    if (!texID) {
        return 0;
    }
    GR_GL_CALL_NOERRCHECK(this->gl(), BindTexture(GR_GL_TEXTURE_EXTERNAL, texID));
    if (this->gl()->fFunctions.fGetError() != GR_GL_NO_ERROR) {
        GR_GL_CALL(this->gl(), DeleteTextures(1, &texID));
        return 0;
    }
    glEGLImageTargetTexture2D(GR_GL_TEXTURE_EXTERNAL, image);
    if (this->gl()->fFunctions.fGetError() != GR_GL_NO_ERROR) {
        GR_GL_CALL(this->gl(), DeleteTextures(1, &texID));
        return 0;
    }
    return texID;
#else
    return 0;
#endif
}

std::unique_ptr<sk_gpu_test::GLTestContext> EGLGLTestContext::makeNew() const {
    std::unique_ptr<sk_gpu_test::GLTestContext> ctx(new EGLGLTestContext(this->gl()->fStandard,
                                                                         nullptr));
    if (ctx) {
        ctx->makeCurrent();
    }
    return ctx;
}

void EGLGLTestContext::onPlatformMakeNotCurrent() const {
    if (!eglMakeCurrent(fDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT )) {
        SkDebugf("Could not reset the context.\n");
    }
}

void EGLGLTestContext::onPlatformMakeCurrent() const {
    if (!eglMakeCurrent(fDisplay, fSurface, fSurface, fContext)) {
        SkDebugf("Could not set the context.\n");
    }
}

std::function<void()> EGLGLTestContext::onPlatformGetAutoContextRestore() const {
    if (eglGetCurrentContext() == fContext) {
        return nullptr;
    }
    return context_restorer();
}

GrGLFuncPtr EGLGLTestContext::onPlatformGetProcAddress(const char* procName) const {
    return eglGetProcAddress(procName);
}

}  // anonymous namespace

namespace sk_gpu_test {
GLTestContext *CreatePlatformGLTestContext(GrGLStandard forcedGpuAPI,
                                           GLTestContext *shareContext) {
    EGLGLTestContext* eglShareContext = reinterpret_cast<EGLGLTestContext*>(shareContext);
    EGLGLTestContext *ctx = new EGLGLTestContext(forcedGpuAPI, eglShareContext);
    if (!ctx->isValid()) {
        delete ctx;
        return nullptr;
    }
    return ctx;
}
}  // namespace sk_gpu_test
