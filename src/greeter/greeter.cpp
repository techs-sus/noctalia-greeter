#include "greeter/greeter.h"

#include "core/log.h"
#include "greeter/greeter_preferences.h"
#include "greeter/greeter_surface.h"
#include "greeter/greeter_window.h"
#include "greeter/output_layout.h"
#include "render/render_context.h"
#include "render/text/glyph_registry.h"
#include "ui/style.h"
#include "wayland/wayland_client.h"
#include "wayland/wayland_seat.h"

#include <cmath>
#include <cstdlib>
#include <poll.h>
#include <wayland-client.h>

namespace {
constexpr Logger kLog("greeter");

void syncOutputViewport(WaylandClient &client, GreeterSurface &surface) {
  if (!client.needsOutputViewport()) {
    surface.clearOutputViewport();
    return;
  }

  if (const auto layout = client.greeterOutputLayout()) {
    kLog.info("output viewport at ({},{}) {}x{}", layout->x, layout->y,
              layout->width, layout->height);
    surface.setOutputViewport(
        static_cast<float>(layout->x), static_cast<float>(layout->y),
        static_cast<float>(layout->width), static_cast<float>(layout->height));
    return;
  }

  surface.clearOutputViewport();
}

void isolateToPrimaryOutput(WaylandClient &client) {
  if (!client.hasReadyOutputs()) {
    return;
  }
  if (const auto name = client.primaryConnectorName()) {
    (void)greeter::disableNonPreferredOutputs(*name);
  }
}

void applyConfiguredOutput(WaylandClient &client,
                           const std::optional<std::string> &configured) {
  if (!configured.has_value() || configured->empty()) {
    client.forgetPreferredOutput();
    isolateToPrimaryOutput(client);
    return;
  }

  client.setPreferredOutputName(configured);
  if (!client.hasReadyOutputs()) {
    return;
  }

  if (!client.hasResolvedPreferredOutput()) {
    kLog.warn("output '{}' is not connected; using primary display only",
              *configured);
    client.forgetPreferredOutput();
    isolateToPrimaryOutput(client);
    return;
  }

  kLog.info("preferred output connector: {}", *configured);
  (void)greeter::disableNonPreferredOutputs(*configured);
}
} // namespace

Greeter::Greeter() = default;

Greeter::~Greeter() = default;

bool Greeter::initialize(WaylandClient &client) {
  m_client = &client;

  const greeter::GreeterPreferences prefs = greeter::loadGreeterPreferences();
  m_manualUiScale = prefs.scale;

  GlyphRegistry::initialize();

  m_glSharedContext.initialize(client.display());

  m_renderContext = std::make_unique<RenderContext>();
  m_renderContext->initialize(m_glSharedContext);

  connectGreetd();

  m_surface = std::make_unique<GreeterSurface>();
  m_surface->setGreetdClient(&m_greetdClient);
  m_surface->setOnExitRequested([this]() { m_exitRequested = true; });

  m_window = std::make_unique<GreeterWindow>(client, m_glSharedContext,
                                             *m_renderContext, *m_surface);
  client.setOutputsChangedCallback([this, configured = prefs.output]() {
    applyConfiguredOutput(*m_client, configured);
    syncUiScale();
    if (m_window != nullptr) {
      m_window->matchPrimaryOutputSize();
    }
    if (m_surface != nullptr) {
      syncOutputViewport(*m_client, *m_surface);
    }
  });
  if (!m_window->initialize()) {
    kLog.error("failed to create greeter window");
    return false;
  }

  m_window->matchPrimaryOutputSize();
  applyConfiguredOutput(client, prefs.output);
  syncUiScale();

  m_surface->initialize(*m_window, m_renderContext.get());
  syncOutputViewport(client, *m_surface);
  setupInputCallbacks(client);
  m_window->setSceneReady(true);

  if (client.flush() < 0) {
    kLog.error("Wayland flush failed after greeter init");
    return false;
  }

  kLog.info("greeter initialized");
  return true;
}

int Greeter::run(WaylandClient &client) {
  wl_display *display = client.display();

  while (!m_exitRequested) {
    client.repeatTick();

    if (client.flush() < 0) {
      kLog.error("Wayland flush failed");
      return 1;
    }

    const int repeatMs = client.repeatPollTimeoutMs();
    const int timeoutMs = repeatMs >= 0 ? repeatMs : -1;

    while (wl_display_prepare_read(display) != 0) {
      if (wl_display_dispatch_pending(display) < 0) {
        kLog.error("Wayland dispatch_pending failed");
        return 1;
      }
    }

    pollfd pfd{};
    pfd.fd = wl_display_get_fd(display);
    pfd.events = POLLIN;
    if (poll(&pfd, 1, timeoutMs) > 0) {
      wl_display_read_events(display);
    } else {
      wl_display_cancel_read(display);
    }

    if (wl_display_dispatch_pending(display) < 0) {
      kLog.error("Wayland dispatch_pending failed");
      return 1;
    }
  }

  return 0;
}

void Greeter::syncUiScale() {
  if (m_client == nullptr) {
    return;
  }

  const float next =
      m_manualUiScale.has_value() ? *m_manualUiScale : m_client->uiScale();
  if (std::fabs(next - Style::uiScale()) < 0.01f) {
    return;
  }

  Style::setUiScale(next);
  if (m_manualUiScale.has_value()) {
    kLog.info("UI scale set to {:.2f} (manual)", Style::uiScale());
  } else {
    kLog.info("UI scale set to {:.2f}", Style::uiScale());
  }
  if (m_surface != nullptr) {
    m_surface->requestLayout();
  }
}

void Greeter::connectGreetd() {
  const char *sockPath = std::getenv("GREETD_SOCK");
  std::string path = sockPath ? sockPath : "/run/greetd/server.sock";

  if (!m_greetdClient.connect(path)) {
    kLog.error("failed to connect to greetd at {}", path);
  }
}

void Greeter::setupInputCallbacks(WaylandClient &client) {
  client.setPointerEventCallback([this](const PointerEvent &event) {
    switch (event.type) {
    case PointerEvent::Type::Motion:
      onPointerMotion(event.sx, event.sy);
      break;
    case PointerEvent::Type::Button:
      onPointerMotion(event.sx, event.sy);
      onPointerButton(event.sx, event.sy, event.button, event.state != 0);
      break;
    default:
      break;
    }
  });

  client.setKeyboardEventCallback([this](const KeyboardEvent &event) {
    onKeyboardEvent(event.sym, event.utf32, event.modifiers, event.pressed,
                    event.preedit);
  });
}

void Greeter::onKeyboardEvent(std::uint32_t sym, std::uint32_t utf32,
                              std::uint32_t modifiers, bool pressed,
                              bool preedit) {
  if (m_surface) {
    m_surface->onKeyEvent(sym, utf32, modifiers, pressed, preedit);
  }
}

void Greeter::onPointerMotion(double x, double y) {
  if (m_surface) {
    m_surface->onPointerMotion(static_cast<float>(x), static_cast<float>(y));
  }
}

void Greeter::onPointerButton(double x, double y, std::uint32_t button,
                              bool pressed) {
  if (m_surface) {
    m_surface->onPointerEvent(static_cast<float>(x), static_cast<float>(y),
                              button, pressed);
  }
}

void Greeter::onThemeChanged() {
  if (m_surface) {
    m_surface->onThemeChanged();
  }
}

bool Greeter::startSession(const std::string &command) {
  if (!m_greetdClient.isConnected()) {
    return false;
  }

  GreetdSessionCommand cmd;
  cmd.command = command;

  if (!m_greetdClient.startSession(cmd)) {
    kLog.error("failed to start session: {}",
               m_greetdClient.lastError()
                   ? m_greetdClient.lastError()->description
                   : "unknown");
    return false;
  }

  m_sessionStarted = true;
  kLog.info("session started: {}", command);
  return true;
}
